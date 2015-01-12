/*
    SSSD

    LDAP Backend Module -- prime ccache with TGT in a child process

    Authors:
        Jakub Hrozek <jhrozek@redhat.com>

    Copyright (C) 2009 Red Hat

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <popt.h>

#include "util/util.h"
#include "util/sss_krb5.h"
#include "util/child_common.h"
#include "providers/dp_backend.h"
#include "providers/krb5/krb5_common.h"

static krb5_context krb5_error_ctx;
#define LDAP_CHILD_DEBUG(level, error) KRB5_DEBUG(level, krb5_error_ctx, error)

static const char *__ldap_child_krb5_error_msg;
#define KRB5_SYSLOG(krb5_error) do { \
    __ldap_child_krb5_error_msg = sss_krb5_get_error_message(krb5_error_ctx, krb5_error); \
    sss_log(SSS_LOG_ERR, "%s", __ldap_child_krb5_error_msg); \
    sss_krb5_free_error_message(krb5_error_ctx, __ldap_child_krb5_error_msg); \
} while(0)

struct input_buffer {
    const char *realm_str;
    const char *princ_str;
    char *keytab_name;
    krb5_deltat lifetime;
    krb5_context context;
    uid_t uid;
    gid_t gid;
};

static errno_t unpack_buffer(uint8_t *buf, size_t size,
                             struct input_buffer *ibuf)
{
    size_t p = 0;
    uint32_t len;

    DEBUG(SSSDBG_TRACE_LIBS, "total buffer size: %zu\n", size);

    /* realm_str size and length */
    SAFEALIGN_COPY_UINT32_CHECK(&len, buf + p, size, &p);

    DEBUG(SSSDBG_TRACE_LIBS, "realm_str size: %d\n", len);
    if (len) {
        if ((p + len ) > size) return EINVAL;
        ibuf->realm_str = talloc_strndup(ibuf, (char *)(buf + p), len);
        DEBUG(SSSDBG_TRACE_LIBS, "got realm_str: %s\n", ibuf->realm_str);
        if (ibuf->realm_str == NULL) return ENOMEM;
        p += len;
    }

    /* princ_str size and length */
    SAFEALIGN_COPY_UINT32_CHECK(&len, buf + p, size, &p);

    DEBUG(SSSDBG_TRACE_LIBS, "princ_str size: %d\n", len);
    if (len) {
        if ((p + len ) > size) return EINVAL;
        ibuf->princ_str = talloc_strndup(ibuf, (char *)(buf + p), len);
        DEBUG(SSSDBG_TRACE_LIBS, "got princ_str: %s\n", ibuf->princ_str);
        if (ibuf->princ_str == NULL) return ENOMEM;
        p += len;
    }

    /* keytab_name size and length */
    SAFEALIGN_COPY_UINT32_CHECK(&len, buf + p, size, &p);

    DEBUG(SSSDBG_TRACE_LIBS, "keytab_name size: %d\n", len);
    if (len) {
        if ((p + len ) > size) return EINVAL;
        ibuf->keytab_name = talloc_strndup(ibuf, (char *)(buf + p), len);
        DEBUG(SSSDBG_TRACE_LIBS, "got keytab_name: %s\n", ibuf->keytab_name);
        if (ibuf->keytab_name == NULL) return ENOMEM;
        p += len;
    }

    /* ticket lifetime */
    SAFEALIGN_COPY_UINT32_CHECK(&ibuf->lifetime, buf + p, size, &p);
    DEBUG(SSSDBG_TRACE_LIBS, "lifetime: %u\n", ibuf->lifetime);

    /* UID and GID to run as */
    SAFEALIGN_COPY_UINT32_CHECK(&ibuf->uid, buf + p, size, &p);
    SAFEALIGN_COPY_UINT32_CHECK(&ibuf->gid, buf + p, size, &p);
    DEBUG(SSSDBG_FUNC_DATA,
          "Will run as [%"SPRIuid"][%"SPRIgid"].\n", ibuf->uid, ibuf->gid);

    return EOK;
}

static int pack_buffer(struct response *r, int result, krb5_error_code krberr,
                       const char *msg, time_t expire_time)
{
    int len;
    size_t p = 0;

    len = strlen(msg);
    r->size = 2 * sizeof(uint32_t) + sizeof(krb5_error_code) +
              len + sizeof(time_t);

    DEBUG(SSSDBG_TRACE_INTERNAL, "response size: %zu\n",r->size);

    r->buf = talloc_array(r, uint8_t, r->size);
    if(!r->buf) {
        return ENOMEM;
    }

    DEBUG(SSSDBG_TRACE_LIBS,
          "result [%d] krberr [%d] msgsize [%d] msg [%s]\n",
           result, krberr, len, msg);

    /* result */
    SAFEALIGN_SET_UINT32(&r->buf[p], result, &p);

    /* krb5 error code */
    safealign_memcpy(&r->buf[p], &krberr, sizeof(krberr), &p);

    /* message size */
    SAFEALIGN_SET_UINT32(&r->buf[p], len, &p);

    /* message itself */
    safealign_memcpy(&r->buf[p], msg, len, &p);

    /* ticket expiration time */
    safealign_memcpy(&r->buf[p], &expire_time, sizeof(expire_time), &p);

    return EOK;
}

static errno_t
set_child_debugging(krb5_context ctx)
{
    krb5_error_code kerr;

    /* Set the global error context */
    krb5_error_ctx = ctx;

    if (debug_level & SSSDBG_TRACE_ALL) {
        kerr = sss_child_set_krb5_tracing(ctx);
        if (kerr) {
            LDAP_CHILD_DEBUG(SSSDBG_MINOR_FAILURE, kerr);
            return EIO;
        }
    }

    return EOK;
}

static int lc_verify_keytab_ex(const char *principal,
                               const char *keytab_name,
                               krb5_context context,
                               krb5_keytab keytab)
{
    bool found;
    char *kt_principal;
    krb5_error_code krberr;
    krb5_kt_cursor cursor;
    krb5_keytab_entry entry;

    krberr = krb5_kt_start_seq_get(context, keytab, &cursor);
    if (krberr) {
        DEBUG(SSSDBG_FATAL_FAILURE,
              "Cannot read keytab [%s].\n", KEYTAB_CLEAN_NAME);

        sss_log(SSS_LOG_ERR, "Error reading keytab file [%s]: [%d][%s]. "
                             "Unable to create GSSAPI-encrypted LDAP "
                             "connection.",
                             KEYTAB_CLEAN_NAME, krberr,
                             sss_krb5_get_error_message(context, krberr));

        return EIO;
    }

    found = false;
    while ((krb5_kt_next_entry(context, keytab, &entry, &cursor)) == 0) {
        krberr = krb5_unparse_name(context, entry.principal, &kt_principal);
        if (krberr) {
            DEBUG(SSSDBG_FATAL_FAILURE,
                  "Could not parse keytab entry\n");
            sss_log(SSS_LOG_ERR, "Could not parse keytab entry\n");
            return EIO;
        }

        if (strcmp(principal, kt_principal) == 0) {
            found = true;
        }
        free(kt_principal);
        krberr = sss_krb5_free_keytab_entry_contents(context, &entry);
        if (krberr) {
            /* This should never happen. The API docs for this function
             * specify only success for this function
             */
            DEBUG(SSSDBG_CRIT_FAILURE,"Could not free keytab entry contents\n");
            /* This is non-fatal, so we'll continue here */
        }

        if (found) {
            break;
        }
    }

    krberr = krb5_kt_end_seq_get(context, keytab, &cursor);
    if (krberr) {
        DEBUG(SSSDBG_FATAL_FAILURE, "Could not close keytab.\n");
        sss_log(SSS_LOG_ERR, "Could not close keytab file [%s].",
                             KEYTAB_CLEAN_NAME);
        return EIO;
    }

    if (!found) {
        DEBUG(SSSDBG_FATAL_FAILURE,
              "Principal [%s] not found in keytab [%s]\n",
               principal,
               KEYTAB_CLEAN_NAME);
        sss_log(SSS_LOG_ERR, "Error processing keytab file [%s]: "
                             "Principal [%s] was not found. "
                             "Unable to create GSSAPI-encrypted LDAP connection.",
                             KEYTAB_CLEAN_NAME, principal);

        return EFAULT;
    }

    return EOK;
}

static krb5_error_code ldap_child_get_tgt_sync(TALLOC_CTX *memctx,
                                               krb5_context context,
                                               const char *realm_str,
                                               const char *princ_str,
                                               const char *keytab_name,
                                               const krb5_deltat lifetime,
                                               const char **ccname_out,
                                               time_t *expire_time_out)
{
    int fd;
    char *ccname;
    char *ccname_dummy;
    char *realm_name = NULL;
    char *full_princ = NULL;
    char *default_realm = NULL;
    char *tmp_str = NULL;
    krb5_keytab keytab = NULL;
    krb5_ccache ccache = NULL;
    krb5_principal kprinc;
    krb5_creds my_creds;
    krb5_get_init_creds_opt options;
    krb5_error_code krberr;
    krb5_timestamp kdc_time_offset;
    int canonicalize = 0;
    int kdc_time_offset_usec;
    int ret;
    TALLOC_CTX *tmp_ctx;
    char *ccname_file_dummy;
    char *ccname_file;
    mode_t old_umask;

    tmp_ctx = talloc_new(memctx);
    if (tmp_ctx == NULL) {
        krberr = KRB5KRB_ERR_GENERIC;
        goto done;
    }

    krberr = set_child_debugging(context);
    if (krberr != EOK) {
        DEBUG(SSSDBG_MINOR_FAILURE, "Cannot set krb5_child debugging\n");
    }

    if (!realm_str) {
        krberr = krb5_get_default_realm(context, &default_realm);
        if (krberr) {
            DEBUG(SSSDBG_OP_FAILURE, "Failed to get default realm name: %s\n",
                      sss_krb5_get_error_message(context, krberr));
            goto done;
        }

        realm_name = talloc_strdup(tmp_ctx, default_realm);
        krb5_free_default_realm(context, default_realm);
        if (!realm_name) {
            krberr = KRB5KRB_ERR_GENERIC;
            goto done;
        }
    } else {
        realm_name = talloc_strdup(tmp_ctx, realm_str);
        if (!realm_name) {
            krberr = KRB5KRB_ERR_GENERIC;
            goto done;
        }
    }

    DEBUG(SSSDBG_TRACE_INTERNAL, "got realm_name: [%s]\n", realm_name);

    if (princ_str) {
        if (!strchr(princ_str, '@')) {
            full_princ = talloc_asprintf(tmp_ctx, "%s@%s",
                                         princ_str, realm_name);
        } else {
            full_princ = talloc_strdup(tmp_ctx, princ_str);
        }
    } else {
        char hostname[HOST_NAME_MAX + 1];

        ret = gethostname(hostname, HOST_NAME_MAX);
        if (ret == -1) {
            krberr = KRB5KRB_ERR_GENERIC;
            goto done;
        }
        hostname[HOST_NAME_MAX] = '\0';

        DEBUG(SSSDBG_TRACE_LIBS, "got hostname: [%s]\n", hostname);

        ret = select_principal_from_keytab(tmp_ctx, hostname, realm_name,
                keytab_name, &full_princ, NULL, NULL);
        if (ret) {
            krberr = KRB5_KT_IOERR;
            goto done;
        }
    }
    if (!full_princ) {
        krberr = KRB5KRB_ERR_GENERIC;
        goto done;
    }
    DEBUG(SSSDBG_CONF_SETTINGS, "Principal name is: [%s]\n", full_princ);

    krberr = krb5_parse_name(context, full_princ, &kprinc);
    if (krberr) {
        DEBUG(SSSDBG_OP_FAILURE, "Unable to build principal: %s\n",
                  sss_krb5_get_error_message(context, krberr));
        goto done;
    }

    if (keytab_name) {
        krberr = krb5_kt_resolve(context, keytab_name, &keytab);
    } else {
        krberr = krb5_kt_default(context, &keytab);
    }
    DEBUG(SSSDBG_CONF_SETTINGS, "Using keytab [%s]\n", KEYTAB_CLEAN_NAME);
    if (krberr) {
        DEBUG(SSSDBG_FATAL_FAILURE,
              "Failed to read keytab file [%s]: %s\n",
               KEYTAB_CLEAN_NAME,
               sss_krb5_get_error_message(context, krberr));
        goto done;
    }

    /* Verify the keytab */
    ret = lc_verify_keytab_ex(full_princ, keytab_name, context, keytab);
    if (ret) {
        DEBUG(SSSDBG_OP_FAILURE,
                "Unable to verify principal is present in the keytab\n");
        krberr = KRB5_KT_IOERR;
        goto done;
    }

    memset(&my_creds, 0, sizeof(my_creds));
    memset(&options, 0, sizeof(options));

    krb5_get_init_creds_opt_set_address_list(&options, NULL);
    krb5_get_init_creds_opt_set_forwardable(&options, 0);
    krb5_get_init_creds_opt_set_proxiable(&options, 0);
    krb5_get_init_creds_opt_set_tkt_life(&options, lifetime);

    tmp_str = getenv("KRB5_CANONICALIZE");
    if (tmp_str != NULL && strcasecmp(tmp_str, "true") == 0) {
        DEBUG(SSSDBG_CONF_SETTINGS, "Will canonicalize principals\n");
        canonicalize = 1;
    }
    sss_krb5_get_init_creds_opt_set_canonicalize(&options, canonicalize);

    ccname_file = talloc_asprintf(tmp_ctx, "%s/ccache_%s",
                                  DB_PATH, realm_name);
    if (ccname_file == NULL) {
        ret = ENOMEM;
        goto done;
    }

    ccname_file_dummy = talloc_asprintf(tmp_ctx, "%s/ccache_%s_XXXXXX",
                                        DB_PATH, realm_name);
    if (ccname_file_dummy == NULL) {
        ret = ENOMEM;
        goto done;
    }

    old_umask = umask(077);
    fd = mkstemp(ccname_file_dummy);
    umask(old_umask);
    if (fd == -1) {
        ret = errno;
        goto done;
    }
    /* We only care about creating a unique file name here, we don't
     * need the fd
     */
    close(fd);

    krberr = krb5_get_init_creds_keytab(context, &my_creds, kprinc,
                                        keytab, 0, NULL, &options);
    krb5_kt_close(context, keytab);
    keytab = NULL;
    if (krberr) {
        DEBUG(SSSDBG_FATAL_FAILURE,
              "Failed to init credentials: %s\n",
               sss_krb5_get_error_message(context, krberr));
        sss_log(SSS_LOG_ERR,
                "Failed to initialize credentials using keytab [%s]: %s. "
                "Unable to create GSSAPI-encrypted LDAP connection.",
                KEYTAB_CLEAN_NAME,
                sss_krb5_get_error_message(context, krberr));
        goto done;
    }
    DEBUG(SSSDBG_TRACE_INTERNAL, "credentials initialized\n");

    ccname_dummy = talloc_asprintf(tmp_ctx, "FILE:%s", ccname_file_dummy);
    ccname = talloc_asprintf(tmp_ctx, "FILE:%s", ccname_file);
    if (ccname_dummy == NULL || ccname == NULL) {
        krberr = ENOMEM;
        goto done;
    }
    DEBUG(SSSDBG_TRACE_INTERNAL, "keytab ccname: [%s]\n", ccname_dummy);

    krberr = krb5_cc_resolve(context, ccname_dummy, &ccache);
    if (krberr) {
        DEBUG(SSSDBG_OP_FAILURE, "Failed to set cache name: %s\n",
                  sss_krb5_get_error_message(context, krberr));
        goto done;
    }

    /* Use updated principal if changed due to canonicalization. */
    krberr = krb5_cc_initialize(context, ccache, my_creds.client);
    if (krberr) {
        DEBUG(SSSDBG_OP_FAILURE, "Failed to init ccache: %s\n",
                  sss_krb5_get_error_message(context, krberr));
        goto done;
    }

    krberr = krb5_cc_store_cred(context, ccache, &my_creds);
    if (krberr) {
        DEBUG(SSSDBG_OP_FAILURE, "Failed to store creds: %s\n",
                  sss_krb5_get_error_message(context, krberr));
        goto done;
    }
    DEBUG(SSSDBG_TRACE_INTERNAL, "credentials stored\n");

#ifdef HAVE_KRB5_GET_TIME_OFFSETS
    krberr = krb5_get_time_offsets(context, &kdc_time_offset,
            &kdc_time_offset_usec);
    if (krberr) {
        DEBUG(SSSDBG_OP_FAILURE, "Failed to get KDC time offset: %s\n",
                  sss_krb5_get_error_message(context, krberr));
        kdc_time_offset = 0;
    } else {
        if (kdc_time_offset_usec > 0) {
            kdc_time_offset++;
        }
    }
    DEBUG(SSSDBG_TRACE_INTERNAL, "Got KDC time offset\n");
#else
    /* If we don't have this function, just assume no offset */
    kdc_time_offset = 0;
#endif

    DEBUG(SSSDBG_TRACE_INTERNAL,
          "Renaming [%s] to [%s]\n", ccname_file_dummy, ccname_file);
    ret = rename(ccname_file_dummy, ccname_file);
    if (ret == -1) {
        ret = errno;
        DEBUG(SSSDBG_CRIT_FAILURE,
              "rename failed [%d][%s].\n", ret, strerror(ret));
        goto done;
    }

    krberr = 0;
    *ccname_out = talloc_steal(memctx, ccname);
    *expire_time_out = my_creds.times.endtime - kdc_time_offset;

done:
    talloc_free(tmp_ctx);
    if (krberr != 0) KRB5_SYSLOG(krberr);
    if (keytab) krb5_kt_close(context, keytab);
    if (context) krb5_free_context(context);
    return krberr;
}

static int prepare_response(TALLOC_CTX *mem_ctx,
                            const char *ccname,
                            time_t expire_time,
                            krb5_error_code kerr,
                            struct response **rsp)
{
    int ret;
    struct response *r = NULL;
    const char *krb5_msg = NULL;

    r = talloc_zero(mem_ctx, struct response);
    if (!r) return ENOMEM;

    r->buf = NULL;
    r->size = 0;

    DEBUG(SSSDBG_TRACE_FUNC, "Building response for result [%d]\n", kerr);

    if (kerr == 0) {
        ret = pack_buffer(r, EOK, kerr, ccname, expire_time);
    } else {
        krb5_msg = sss_krb5_get_error_message(krb5_error_ctx, kerr);
        if (krb5_msg == NULL) {
            DEBUG(SSSDBG_CRIT_FAILURE,
                    "sss_krb5_get_error_message failed.\n");
            return ENOMEM;
        }

        ret = pack_buffer(r, EFAULT, kerr, krb5_msg, 0);
        sss_krb5_free_error_message(krb5_error_ctx, krb5_msg);
    }

    if (ret != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE, "pack_buffer failed\n");
        return ret;
    }

    *rsp = r;
    return EOK;
}

static krb5_error_code privileged_krb5_setup(struct input_buffer *ibuf)
{
    krb5_error_code kerr;
    char *keytab_name;

    kerr = krb5_init_context(&ibuf->context);
    if (kerr != 0) {
        DEBUG(SSSDBG_CRIT_FAILURE, "Failed to init kerberos context\n");
        return kerr;
    }
    DEBUG(SSSDBG_TRACE_INTERNAL, "Kerberos context initialized\n");

    kerr = copy_keytab_into_memory(ibuf, ibuf->context, ibuf->keytab_name,
                                   &keytab_name, NULL);
    if (kerr != 0) {
        DEBUG(SSSDBG_OP_FAILURE, "copy_keytab_into_memory failed.\n");
        return kerr;
    }
    talloc_free(ibuf->keytab_name);
    ibuf->keytab_name = keytab_name;

    return 0;
}

int main(int argc, const char *argv[])
{
    int ret;
    int kerr;
    int opt;
    int debug_fd = -1;
    poptContext pc;
    TALLOC_CTX *main_ctx = NULL;
    uint8_t *buf = NULL;
    ssize_t len = 0;
    const char *ccname = NULL;
    time_t expire_time = 0;
    struct input_buffer *ibuf = NULL;
    struct response *resp = NULL;
    ssize_t written;

    struct poptOption long_options[] = {
        POPT_AUTOHELP
        {"debug-level", 'd', POPT_ARG_INT, &debug_level, 0,
         _("Debug level"), NULL},
        {"debug-timestamps", 0, POPT_ARG_INT, &debug_timestamps, 0,
         _("Add debug timestamps"), NULL},
        {"debug-microseconds", 0, POPT_ARG_INT, &debug_microseconds, 0,
         _("Show timestamps with microseconds"), NULL},
        {"debug-fd", 0, POPT_ARG_INT, &debug_fd, 0,
         _("An open file descriptor for the debug logs"), NULL},
        {"debug-to-stderr", 0, POPT_ARG_NONE | POPT_ARGFLAG_DOC_HIDDEN, &debug_to_stderr, 0, \
         _("Send the debug output to stderr directly."), NULL }, \
        POPT_TABLEEND
    };

    /* Set debug level to invalid value so we can decide if -d 0 was used. */
    debug_level = SSSDBG_INVALID;

    pc = poptGetContext(argv[0], argc, argv, long_options, 0);
    while((opt = poptGetNextOpt(pc)) != -1) {
        switch(opt) {
        default:
        fprintf(stderr, "\nInvalid option %s: %s\n\n",
                  poptBadOption(pc, 0), poptStrerror(opt));
            poptPrintUsage(pc, stderr, 0);
            _exit(-1);
        }
    }

    poptFreeContext(pc);

    DEBUG_INIT(debug_level);

    debug_prg_name = talloc_asprintf(NULL, "[sssd[ldap_child[%d]]]", getpid());
    if (!debug_prg_name) {
        debug_prg_name = "[sssd[ldap_child]]";
        DEBUG(SSSDBG_CRIT_FAILURE, "talloc_asprintf failed.\n");
        goto fail;
    }

    if (debug_fd != -1) {
        ret = set_debug_file_from_fd(debug_fd);
        if (ret != EOK) {
            DEBUG(SSSDBG_CRIT_FAILURE, "set_debug_file_from_fd failed.\n");
        }
    }

    DEBUG(SSSDBG_TRACE_FUNC, "ldap_child started.\n");

    main_ctx = talloc_new(NULL);
    if (main_ctx == NULL) {
        DEBUG(SSSDBG_CRIT_FAILURE, "talloc_new failed.\n");
        talloc_free(discard_const(debug_prg_name));
        goto fail;
    }
    talloc_steal(main_ctx, debug_prg_name);

    buf = talloc_size(main_ctx, sizeof(uint8_t)*IN_BUF_SIZE);
    if (buf == NULL) {
        DEBUG(SSSDBG_CRIT_FAILURE, "talloc_size failed.\n");
        goto fail;
    }

    ibuf = talloc_zero(main_ctx, struct input_buffer);
    if (ibuf == NULL) {
        DEBUG(SSSDBG_CRIT_FAILURE, "talloc_size failed.\n");
        goto fail;
    }

    DEBUG(SSSDBG_TRACE_INTERNAL, "context initialized\n");

    errno = 0;
    len = sss_atomic_read_s(STDIN_FILENO, buf, IN_BUF_SIZE);
    if (len == -1) {
        ret = errno;
        DEBUG(SSSDBG_CRIT_FAILURE, "read failed [%d][%s].\n", ret, strerror(ret));
        goto fail;
    }

    close(STDIN_FILENO);

    ret = unpack_buffer(buf, len, ibuf);
    if (ret != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE,
              "unpack_buffer failed.[%d][%s].\n", ret, strerror(ret));
        goto fail;
    }

    kerr = privileged_krb5_setup(ibuf);
    if (kerr != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE, "Privileged Krb5 setup failed.\n");
        goto fail;
    }
    DEBUG(SSSDBG_TRACE_INTERNAL, "Kerberos context initialized\n");

    kerr = become_user(ibuf->uid, ibuf->gid);
    if (kerr != 0) {
        DEBUG(SSSDBG_CRIT_FAILURE, "become_user failed.\n");
        goto fail;
    }

    DEBUG(SSSDBG_TRACE_INTERNAL,
          "Running as [%"SPRIuid"][%"SPRIgid"].\n", geteuid(), getegid());

    DEBUG(SSSDBG_TRACE_INTERNAL, "getting TGT sync\n");
    kerr = ldap_child_get_tgt_sync(main_ctx, ibuf->context,
                                   ibuf->realm_str, ibuf->princ_str,
                                   ibuf->keytab_name, ibuf->lifetime,
                                   &ccname, &expire_time);
    if (kerr != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE, "ldap_child_get_tgt_sync failed.\n");
        /* Do not return, must report failure */
    }

    ret = prepare_response(main_ctx, ccname, expire_time, kerr, &resp);
    if (ret != EOK) {
        DEBUG(SSSDBG_CRIT_FAILURE, "prepare_response failed. [%d][%s].\n",
                    ret, strerror(ret));
        goto fail;
    }

    errno = 0;
    written = sss_atomic_write_s(STDOUT_FILENO, resp->buf, resp->size);
    if (written == -1) {
        ret = errno;
        DEBUG(SSSDBG_CRIT_FAILURE, "write failed [%d][%s].\n", ret,
                    strerror(ret));
        goto fail;
    }

    if (written != resp->size) {
        DEBUG(SSSDBG_CRIT_FAILURE, "Expected to write %zu bytes, wrote %zu\n",
              resp->size, written);
        goto fail;
    }

    DEBUG(SSSDBG_TRACE_FUNC, "ldap_child completed successfully\n");
    close(STDOUT_FILENO);
    talloc_free(main_ctx);
    _exit(0);

fail:
    DEBUG(SSSDBG_CRIT_FAILURE, "ldap_child failed!\n");
    close(STDOUT_FILENO);
    talloc_free(main_ctx);
    _exit(-1);
}
