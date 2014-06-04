/*
    Authors:
        Sumit Bose <sbose@redhat.com>

    Copyright (C) 2013 Red Hat

    SSSD tests: Tests for utility functions

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

#include <popt.h>

#include "tests/cmocka/common_mock.h"
#include "util/sss_nss.h"

#define DOM_COUNT 10
#define DOMNAME_TMPL "name_%zu.dom"
#define FLATNAME_TMPL "name_%zu"
#define SID_TMPL "S-1-5-21-1-2-%zu"

#define MACRO_EXPAND(tok) #tok
#define STR(tok) MACRO_EXPAND(tok)

#define USERNAME "sssduser"
#define UID      1234
#define DOMAIN   "sssddomain"
#define ORIGINAL_HOME "/home/user"
#define FLATNAME "flatname"
#define HOMEDIR_SUBSTR "/mnt/home"

#define DUMMY "dummy"
#define DUMMY2 "dummy2"

struct dom_list_test_ctx {
    size_t dom_count;
    struct sss_domain_info *dom_list;
};

void setup_dom_list(void **state)
{
    struct dom_list_test_ctx *test_ctx;
    struct sss_domain_info *dom = NULL;
    size_t c;

    assert_true(leak_check_setup());

    test_ctx = talloc_zero(global_talloc_context, struct dom_list_test_ctx);
    assert_non_null(test_ctx);

    test_ctx->dom_count = DOM_COUNT;

    for (c = 0; c < test_ctx->dom_count; c++) {
        dom = talloc_zero(test_ctx, struct sss_domain_info);
        assert_non_null(dom);

        dom->name = talloc_asprintf(dom, DOMNAME_TMPL, c);
        assert_non_null(dom->name);

        dom->flat_name = talloc_asprintf(dom, FLATNAME_TMPL, c);
        assert_non_null(dom->flat_name);

        dom->domain_id = talloc_asprintf(dom, SID_TMPL, c);
        assert_non_null(dom->domain_id);

        DLIST_ADD(test_ctx->dom_list, dom);
    }

    check_leaks_push(test_ctx);
    *state = test_ctx;
}

void teardown_dom_list(void **state)
{
    struct dom_list_test_ctx *test_ctx = talloc_get_type(*state,
                                                      struct dom_list_test_ctx);
    if (test_ctx == NULL) {
        DEBUG(SSSDBG_CRIT_FAILURE, "Type mismatch\n");
        return;
    }

    assert_true(check_leaks_pop(test_ctx) == true);
    talloc_free(test_ctx);
    assert_true(leak_check_teardown());
}

void test_find_subdomain_by_sid_null(void **state)
{
    struct dom_list_test_ctx *test_ctx = talloc_get_type(*state,
                                                      struct dom_list_test_ctx);
    struct sss_domain_info *dom;

    dom = find_subdomain_by_sid(NULL, NULL);
    assert_null(dom);

    dom = find_subdomain_by_sid(test_ctx->dom_list, NULL);
    assert_null(dom);

    dom = find_subdomain_by_sid(NULL, "S-1-5-21-1-2-3");
    assert_null(dom);
}

void test_find_subdomain_by_sid(void **state)
{
    struct dom_list_test_ctx *test_ctx = talloc_get_type(*state,
                                                      struct dom_list_test_ctx);
    struct sss_domain_info *dom;
    size_t c;
    char *name;
    char *flat_name;
    char *sid;

    for (c = 0; c < test_ctx->dom_count; c++) {
        name = talloc_asprintf(global_talloc_context, DOMNAME_TMPL, c);
        assert_non_null(name);

        flat_name = talloc_asprintf(global_talloc_context, FLATNAME_TMPL, c);
        assert_non_null(flat_name);

        sid = talloc_asprintf(global_talloc_context, SID_TMPL, c);
        assert_non_null(sid);

        dom = find_subdomain_by_sid(test_ctx->dom_list, sid);
        assert_non_null(dom);
        assert_string_equal(name, dom->name);
        assert_string_equal(flat_name, dom->flat_name);
        assert_string_equal(sid, dom->domain_id);

        talloc_free(name);
        talloc_free(flat_name);
        talloc_free(sid);
    }
}

void test_find_subdomain_by_sid_missing_sid(void **state)
{
    struct dom_list_test_ctx *test_ctx = talloc_get_type(*state,
                                                      struct dom_list_test_ctx);
    struct sss_domain_info *dom;
    size_t c;
    char *name;
    char *flat_name;
    char *sid;
    size_t mis;

    mis = test_ctx->dom_count/2;
    assert_true((mis >= 1 && mis < test_ctx->dom_count));

    dom = test_ctx->dom_list;
    for (c = 0; c < mis; c++) {
        assert_non_null(dom);
        dom = dom->next;
    }
    assert_non_null(dom);
    dom->domain_id = NULL;

    for (c = 0; c < test_ctx->dom_count; c++) {
        name = talloc_asprintf(global_talloc_context, DOMNAME_TMPL, c);
        assert_non_null(name);

        flat_name = talloc_asprintf(global_talloc_context, FLATNAME_TMPL, c);
        assert_non_null(flat_name);

        sid = talloc_asprintf(global_talloc_context, SID_TMPL, c);
        assert_non_null(sid);

        dom = find_subdomain_by_sid(test_ctx->dom_list, sid);
        if (c == mis - 1) {
            assert_null(dom);
        } else {
            assert_non_null(dom);
            assert_string_equal(name, dom->name);
            assert_string_equal(flat_name, dom->flat_name);
            assert_string_equal(sid, dom->domain_id);
        }

        talloc_free(name);
        talloc_free(flat_name);
        talloc_free(sid);
    }
}

#define TEST_SANITIZE_INPUT "TestUser@Test.Domain"
#define TEST_SANITIZE_LC_INPUT "testuser@test.domain"

void test_sss_filter_sanitize_for_dom(void **state)
{
    struct dom_list_test_ctx *test_ctx;
    int ret;
    char *sanitized;
    char *lc_sanitized;
    struct sss_domain_info *dom;

    test_ctx = talloc_get_type(*state, struct dom_list_test_ctx);
    dom = test_ctx->dom_list;

    dom->case_sensitive = true;

    ret = sss_filter_sanitize_for_dom(test_ctx, TEST_SANITIZE_INPUT, dom,
                                      &sanitized, &lc_sanitized);
    assert_int_equal(ret, EOK);
    assert_string_equal(sanitized, TEST_SANITIZE_INPUT);
    assert_string_equal(lc_sanitized, TEST_SANITIZE_INPUT);
    talloc_free(sanitized);
    talloc_free(lc_sanitized);

    dom->case_sensitive = false;

    ret = sss_filter_sanitize_for_dom(test_ctx, TEST_SANITIZE_INPUT, dom,
                                      &sanitized, &lc_sanitized);
    assert_int_equal(ret, EOK);
    assert_string_equal(sanitized, TEST_SANITIZE_INPUT);
    assert_string_equal(lc_sanitized, TEST_SANITIZE_LC_INPUT);
    talloc_free(sanitized);
    talloc_free(lc_sanitized);
}

void check_expanded_value(TALLOC_CTX *tmp_ctx,
                          struct sss_nss_homedir_ctx *homedir_ctx,
                          const char *template, const char *exp_val)
{
    char *homedir;

    homedir = expand_homedir_template(tmp_ctx, template, homedir_ctx);
    if (exp_val != NULL) {
        assert_string_equal(homedir, exp_val);
    } else {
        assert_null(homedir);
    }

    talloc_free(homedir);
}

void setup_homedir_ctx(void **state)
{
    struct sss_nss_homedir_ctx *homedir_ctx;

    assert_true(leak_check_setup());

    homedir_ctx= talloc_zero(global_talloc_context,
                             struct sss_nss_homedir_ctx);
    assert_non_null(homedir_ctx);

    homedir_ctx->username = USERNAME;
    homedir_ctx->uid = UID;
    homedir_ctx->original = ORIGINAL_HOME;
    homedir_ctx->domain = DOMAIN;
    homedir_ctx->flatname = FLATNAME;
    homedir_ctx->config_homedir_substr = HOMEDIR_SUBSTR;

    check_leaks_push(homedir_ctx);
    *state = homedir_ctx;
}

void teardown_homedir_ctx(void **state)
{
    struct sss_nss_homedir_ctx *homedir_ctx = talloc_get_type(*state,
                                                 struct sss_nss_homedir_ctx);
    if (homedir_ctx == NULL) {
        DEBUG(SSSDBG_CRIT_FAILURE, "Type mismatch\n");
        return;
    }

    assert_true(check_leaks_pop(homedir_ctx) == true);
    talloc_free(homedir_ctx);
    assert_true(leak_check_teardown());
}

void test_expand_homedir_template_NULL(void **state)
{
    TALLOC_CTX *tmp_ctx;
    char *homedir;
    struct sss_nss_homedir_ctx *homedir_ctx;

    /* following format strings requires data in homedir_ctx */
    const char *format_strings[] = { "%u", "%U", "%d", "%f", "%F", "%H",
                                     NULL };
    int i;

    tmp_ctx = talloc_new(NULL);
    assert_non_null(tmp_ctx);

    homedir_ctx = talloc_zero(tmp_ctx, struct sss_nss_homedir_ctx);
    assert_non_null(homedir_ctx);

    homedir = expand_homedir_template(tmp_ctx, NULL, NULL);
    assert_null(homedir);

    homedir = expand_homedir_template(tmp_ctx, "template", NULL);
    assert_null(homedir);

    /* missing data in homedir_ctx */
    check_expanded_value(tmp_ctx, homedir_ctx, "%%", "%");
    check_expanded_value(tmp_ctx, homedir_ctx, "%o", "");

    for (i = 0; format_strings[i] != NULL; ++i) {
        check_expanded_value(tmp_ctx, homedir_ctx, format_strings[i], NULL);
    }

    /* flatname requires domain and username */
    homedir_ctx->username = DUMMY;
    check_expanded_value(tmp_ctx, homedir_ctx, "%f", NULL);

    homedir_ctx->username = NULL;
    homedir_ctx->domain = DUMMY;
    check_expanded_value(tmp_ctx, homedir_ctx, "%f", NULL);

    /* test unknown format string */
    check_expanded_value(tmp_ctx, homedir_ctx, "%x", NULL);

    /* test malformed format string */
    check_expanded_value(tmp_ctx, homedir_ctx, "%", NULL);

    talloc_free(tmp_ctx);
}

void test_expand_homedir_template(void **state)
{
    struct sss_nss_homedir_ctx *homedir_ctx = talloc_get_type(*state,
                                                 struct sss_nss_homedir_ctx);
    TALLOC_CTX *tmp_ctx;

    tmp_ctx = talloc_new(NULL);
    assert_non_null(tmp_ctx);

    /* string without template */
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY, DUMMY);

    check_expanded_value(tmp_ctx, homedir_ctx, "%u", USERNAME);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%u", DUMMY USERNAME);
    check_expanded_value(tmp_ctx, homedir_ctx, "%u"DUMMY, USERNAME DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%u"DUMMY2,
                                               DUMMY USERNAME DUMMY2);

    check_expanded_value(tmp_ctx, homedir_ctx, "%U", STR(UID));
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%U", DUMMY STR(UID));
    check_expanded_value(tmp_ctx, homedir_ctx, "%U"DUMMY, STR(UID) DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%U"DUMMY2,
                                               DUMMY STR(UID) DUMMY2);

    check_expanded_value(tmp_ctx, homedir_ctx, "%d", DOMAIN);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%d", DUMMY DOMAIN);
    check_expanded_value(tmp_ctx, homedir_ctx, "%d"DUMMY, DOMAIN DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%d"DUMMY2,
                                               DUMMY DOMAIN DUMMY2);

    check_expanded_value(tmp_ctx, homedir_ctx, "%f", USERNAME"@"DOMAIN);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%f",
                                               DUMMY USERNAME"@"DOMAIN);
    check_expanded_value(tmp_ctx, homedir_ctx, "%f"DUMMY,
                                               USERNAME"@"DOMAIN DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%f"DUMMY2,
                                               DUMMY USERNAME"@"DOMAIN DUMMY2);

    check_expanded_value(tmp_ctx, homedir_ctx, "%o", ORIGINAL_HOME);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%o", DUMMY ORIGINAL_HOME);
    check_expanded_value(tmp_ctx, homedir_ctx, "%o"DUMMY, ORIGINAL_HOME DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%o"DUMMY2,
                                               DUMMY ORIGINAL_HOME DUMMY2);

    check_expanded_value(tmp_ctx, homedir_ctx, "%F", FLATNAME);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%F", DUMMY FLATNAME);
    check_expanded_value(tmp_ctx, homedir_ctx, "%F"DUMMY, FLATNAME DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%F"DUMMY2,
                                               DUMMY FLATNAME DUMMY2);

    check_expanded_value(tmp_ctx, homedir_ctx, "%H", HOMEDIR_SUBSTR);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%H",
                                               DUMMY HOMEDIR_SUBSTR);
    check_expanded_value(tmp_ctx, homedir_ctx, "%H"DUMMY,
                                               HOMEDIR_SUBSTR DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%H"DUMMY2,
                                               DUMMY HOMEDIR_SUBSTR DUMMY2);

    check_expanded_value(tmp_ctx, homedir_ctx, "%%", "%");
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%%", DUMMY"%");
    check_expanded_value(tmp_ctx, homedir_ctx, "%%"DUMMY, "%"DUMMY);
    check_expanded_value(tmp_ctx, homedir_ctx, DUMMY"%%"DUMMY2,
                                               DUMMY"%"DUMMY2);

    /* test all format strings */
    check_expanded_value(tmp_ctx, homedir_ctx,
                         DUMMY"/%u/%U/%d/%f/%o/%F/%%/%H/"DUMMY2,
                         DUMMY"/"USERNAME"/" STR(UID) "/"DOMAIN"/"
                         USERNAME"@"DOMAIN"/"ORIGINAL_HOME"/"FLATNAME"/%/"
                         HOMEDIR_SUBSTR"/"DUMMY2);
    talloc_free(tmp_ctx);
}

int main(int argc, const char *argv[])
{
    poptContext pc;
    int opt;
    struct poptOption long_options[] = {
        POPT_AUTOHELP
        SSSD_DEBUG_OPTS
        POPT_TABLEEND
    };

    const UnitTest tests[] = {
        unit_test_setup_teardown(test_find_subdomain_by_sid_null,
                                 setup_dom_list, teardown_dom_list),
        unit_test_setup_teardown(test_find_subdomain_by_sid,
                                 setup_dom_list, teardown_dom_list),
        unit_test_setup_teardown(test_find_subdomain_by_sid_missing_sid,
                                 setup_dom_list, teardown_dom_list),

        unit_test_setup_teardown(test_sss_filter_sanitize_for_dom,
                                 setup_dom_list, teardown_dom_list),

        unit_test(test_expand_homedir_template_NULL),
        unit_test_setup_teardown(test_expand_homedir_template,
                                 setup_homedir_ctx, teardown_homedir_ctx),
    };

    /* Set debug level to invalid value so we can deside if -d 0 was used. */
    debug_level = SSSDBG_INVALID;

    pc = poptGetContext(argv[0], argc, argv, long_options, 0);
    while((opt = poptGetNextOpt(pc)) != -1) {
        switch(opt) {
        default:
            fprintf(stderr, "\nInvalid option %s: %s\n\n",
                    poptBadOption(pc, 0), poptStrerror(opt));
            poptPrintUsage(pc, stderr, 0);
            return 1;
        }
    }
    poptFreeContext(pc);

    DEBUG_INIT(debug_level);

    /* Even though normally the tests should clean up after themselves
     * they might not after a failed run. Remove the old db to be sure */
    tests_set_cwd();

    return run_tests(tests);
}
