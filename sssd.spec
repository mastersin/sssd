
Name: sssd
Version: 1.11.4
Release: alt1
Group: System/Servers
Summary: System Security Services Daemon
License: GPLv3+
Url: http://fedorahosted.org/sssd/
Source: %name-%version.tar
Source2: %name.init
Source3: system-auth-sss.pam
Source4: system-auth-use_first_pass-sss.pam

Patch: %name-%version-%release.patch

### Patches ###
# Patch0602: 0602-FEDORA-Add-CIFS-idmap-plugin.patch

# Determine the location of the LDB modules directory
%define ldb_modulesdir %(pkg-config --variable=modulesdir ldb)

%define _localstatedir /var
%define _libexecdir /usr/libexec

%define sssdstatedir %_localstatedir/lib/sss
%define dbpath %sssdstatedir/db
%define pipepath %sssdstatedir/pipes
%define mcpath %sssdstatedir/mc
%define pubconfpath %sssdstatedir/pubconf

Requires: %name-client = %version-%release
Requires: libsss_idmap = %version-%release

### Build Dependencies ###
BuildRequires: /proc
BuildRequires: libpopt-devel
BuildRequires: libtalloc-devel
BuildRequires: libtevent-devel
BuildRequires: libtdb-devel >= 1.1.3
BuildRequires: libldb-devel >= 0.9.2
BuildRequires: libdhash-devel >= 0.4.2
BuildRequires: libcollection-devel >= 0.5.1
BuildRequires: libini_config-devel >= 1.0.0
BuildRequires: libdbus-devel
BuildRequires: libldap-devel
BuildRequires: libpam-devel
BuildRequires: libnss-devel
BuildRequires: libnspr-devel
BuildRequires: libpcre-devel >= 7
BuildRequires: libxslt
BuildRequires: libxml2-devel
BuildRequires: docbook-dtds docbook-style-xsl xsltproc xml-utils
BuildRequires: libkrb5-devel
BuildRequires: libcares-devel
BuildRequires: python-devel
BuildRequires: libcheck-devel
BuildRequires: doxygen
BuildRequires: libselinux-devel
BuildRequires: libsemanage-devel
BuildRequires: bind-utils
BuildRequires: libkeyutils-devel
BuildRequires: libnl3-devel
BuildRequires: glib2-devel
BuildRequires: diffstat
BuildRequires: findutils
BuildRequires: samba-devel
BuildRequires: systemd-devel
BuildRequires: selinux-policy-targeted
BuildRequires: cifs-utils-devel
BuildRequires: libsasl2-devel
BuildRequires: libcmocka-devel
BuildRequires: nscd

#%%ifarch %ix86 x86_64 %arm
#BuildRequires: libcmocka-devel
#%%endif

%description
Provides a set of daemons to manage access to remote directories and
authentication mechanisms. It provides an NSS and PAM interface toward
the system and a pluggable backend system to connect to multiple different
account sources. It is also the basis to provide client auditing and policy
services for projects like FreeIPA.

The sssd subpackage is a meta-package that contains the deamon as well as all
the existing back ends.

%package client
Summary: SSSD Client libraries for NSS and PAM
Group: Networking/Other
License: LGPLv3+
Provides: libnss_sss
Provides: pam_sss

%description client
Provides the libraries needed by the PAM and NSS stacks to connect to the SSSD
service.

%package tools
Summary: Userspace tools for use with the SSSD
Group: System/Configuration/Networking
License: GPLv3+
Requires: %name = %version-%release

%description tools
Provides userspace tools for manipulating users, groups, and nested groups in
SSSD when using id_provider = local in /etc/sssd/sssd.conf.

Also provides several other administrative tools:
    * sss_debuglevel to change the debug level on the fly
    * sss_seed which pre-creates a user entry for use in kickstarts
    * sss_obfuscate for generating an obfuscated LDAP password

%package -n python-module-sssdconfig
Summary: SSSD and IPA configuration file manipulation classes and functions
Group: Development/Python
License: GPLv3+
BuildArch: noarch

%description -n python-module-sssdconfig
Provides python files for manipulation SSSD and IPA configuration files.

%package ldap
Summary: The LDAP back end of the SSSD
Group: System/Servers
License: GPLv3+
Requires: %name-krb5-common = %version-%release

%description ldap
Provides the LDAP back end that the SSSD can utilize to fetch identity data
from and authenticate against an LDAP server.

%package krb5-common
Summary: SSSD helpers needed for Kerberos and GSSAPI authentication
Group: System/Servers
License: GPLv3+
Requires: %name = %version-%release
Requires: libsasl2-plugin-gssapi

%description krb5-common
Provides helper processes that the LDAP and Kerberos back ends can use for
Kerberos user or host authentication.

%package krb5
Summary: The Kerberos authentication back end for the SSSD
Group: System/Servers
License: GPLv3+
Requires: %name-krb5-common = %version-%release

%description krb5
Provides the Kerberos back end that the SSSD can utilize authenticate
against a Kerberos server.

%package pac
Summary: Common files needed for supporting PAC processing
Group: System/Servers
License: GPLv3+
Requires: %name = %version-%release

%description pac
Provides common files needed by SSSD providers such as IPA and Active Directory
for handling Kerberos PACs.

%package ipa
Summary: The IPA back end of the SSSD
Group: System/Servers
License: GPLv3+
Requires: %name-krb5-common = %version-%release
Requires: %name-pac = %version-%release

%description ipa
Provides the IPA back end that the SSSD can utilize to fetch identity data
from and authenticate against an IPA server.

%package ad
Summary: The AD back end of the SSSD
Group: System/Servers
License: GPLv3+
Requires: %name-krb5-common = %version-%release
Requires: %name-pac = %version-%release

%description ad
Provides the Active Directory back end that the SSSD can utilize to fetch
identity data from and authenticate against an Active Directory server.

%package proxy
Summary: The proxy back end of the SSSD
Group: System/Servers
License: GPLv3+
Requires: %name = %version-%release

%description proxy
Provides the proxy back end which can be used to wrap an existing NSS and/or
PAM modules to leverage SSSD caching.

%package -n libsss_idmap
Summary: FreeIPA Idmap library
Group: System/Libraries
License: LGPLv3+

%description -n libsss_idmap
Utility library to convert SIDs to Unix uids and gids

%package -n libsss_idmap-devel
Summary: FreeIPA Idmap library
Group: Development/C
License: LGPLv3+
Requires: libsss_idmap = %version-%release

%description -n libsss_idmap-devel
Utility library to SIDs to Unix uids and gids

%package -n libipa_hbac
Summary: FreeIPA HBAC Evaluator library
Group: System/Libraries
License: LGPLv3+

%description -n libipa_hbac
Utility library to validate FreeIPA HBAC rules for authorization requests

%package -n libipa_hbac-devel
Summary: FreeIPA HBAC Evaluator library
Group: Development/C
License: LGPLv3+
Requires: libipa_hbac = %version-%release

%description -n libipa_hbac-devel
Utility library to validate FreeIPA HBAC rules for authorization requests

%package -n python-module-ipa_hbac
Summary: Python bindings for the FreeIPA HBAC Evaluator library
Group: Development/Python
License: LGPLv3+
Requires: libipa_hbac = %version-%release

%description -n python-module-ipa_hbac
The python-module-libipa_hbac contains the bindings so that libipa_hbac can be
used by Python applications.

%package -n libsss_nss_idmap
Summary: Library for SID based lookups
Group: System/Libraries
License: LGPLv3+

%description -n libsss_nss_idmap
Utility library for SID based lookups

%package -n libsss_nss_idmap-devel
Summary: Library for SID based lookups
Group: Development/C
License: LGPLv3+
Requires: libsss_nss_idmap = %version-%release

%description -n libsss_nss_idmap-devel
Utility library for SID based lookups

%package -n python-module-sss_nss_idmap
Summary: Python bindings for libsss_nss_idmap
Group: Development/Python
License: LGPLv3+
Requires: libsss_nss_idmap = %version-%release

%description -n python-module-sss_nss_idmap
The python-module-libsss_nss_idmap contains the bindings so that libsss_nss_idmap can
be used by Python applications.

%package -n python-module-sss
Summary: Python bindings for sss
Group: Development/Python
License: LGPLv3+

%description -n python-module-sss
The python-module-sss contains the bindings so that sss can
be used by Python applications.

%prep
%setup
%patch -p1

%build
%autoreconf
%configure \
    --with-db-path=%dbpath \
    --with-pipe-path=%pipepath \
    --with-pubconf-path=%pubconfpath \
    --with-mcache-path=%mcpath \
    --with-init-dir=%_initdir \
    --with-krb5-rcache-dir=%_localstatedir/cache/krb5rcache \
    --enable-nsslibdir=/%_lib \
    --enable-pammoddir=/%_lib/security \
    --enable-ldb-version-check \
    --with-test-dir=/dev/shm \
    --enable-krb5-locator-plugin \
    --enable-pac-responder \
    --disable-rpath \
    --disable-static

# %%make_build all docs
%make all docs

%install
%make install DESTDIR=%buildroot

%find_lang sssd

# Prepare empty config file
install -D -m600 src/examples/sssd-example.conf %buildroot%_sysconfdir/%name/%name.conf

# Copy default logrotate file
install -D -m644 src/examples/logrotate %buildroot%_sysconfdir/logrotate.d/%name

touch %buildroot%mcpath/passwd
touch %buildroot%mcpath/group

install -D -m644 src/sysv/systemd/sssd.service %buildroot%_unitdir/%name.service
install -D -m755 %SOURCE2 %buildroot%_initdir/%name
install -D -m644 %SOURCE3 %buildroot%_pamdir/system-auth-sss
install -D -m644 %SOURCE4 %buildroot%_pamdir/system-auth-use_first_pass-sss

# Remove .la files created by libtool
find %buildroot -name "*.la" -exec rm -f {} \;

# Suppress developer-only documentation
rm -Rf %buildroot%_docdir/%name

%check
export CK_TIMEOUT_MULTIPLIER=10
%make check
unset CK_TIMEOUT_MULTIPLIER

%post
%post_service %name

%preun
%preun_service %name

%files -f sssd.lang
%doc COPYING
%doc src/examples/sssd-example.conf
%_sbindir/%name
%_initdir/%name
%_unitdir/%name.service

%dir %_libexecdir/%name
%_libexecdir/%name/sssd_be
%_libexecdir/%name/sssd_nss
%_libexecdir/%name/sssd_pam
%_libexecdir/%name/sssd_autofs
%_libexecdir/%name/sssd_ssh
%_libexecdir/%name/sssd_sudo

%dir %_libdir/%name
%_libdir/%name/libsss_simple.so

#Internal shared libraries
%_libdir/%name/libsss_child.so
%_libdir/%name/libsss_crypt.so
%_libdir/%name/libsss_debug.so
%_libdir/%name/libsss_ldap_common.so
%_libdir/%name/libsss_util.so

# 3rd party application libraries
%dir %_libdir/%name/modules
%_libdir/%name/modules/libsss_autofs.so
%_libdir/libsss_sudo.so

%ldb_modulesdir/memberof.so
%_bindir/sss_ssh_authorizedkeys
%_bindir/sss_ssh_knownhostsproxy
%_sbindir/sss_cache

%dir %sssdstatedir
%dir %_localstatedir/cache/krb5rcache
%attr(700,root,root) %dir %dbpath
%attr(755,root,root) %dir %mcpath
%ghost %attr(0644,root,root) %verify(not md5 size mtime) %mcpath/passwd
%ghost %attr(0644,root,root) %verify(not md5 size mtime) %mcpath/group
%attr(755,root,root) %dir %pipepath
%attr(755,root,root) %dir %pubconfpath
%attr(700,root,root) %dir %pipepath/private
%attr(750,root,root) %dir %_var/log/%name
%attr(700,root,root) %dir %_sysconfdir/sssd
%ghost %attr(0600,root,root) %config(noreplace) %_sysconfdir/sssd/sssd.conf
%config(noreplace) %_sysconfdir/logrotate.d/sssd
%dir %_datadir/sssd
%_datadir/%name/sssd.api.conf
%dir %_datadir/%name/sssd.api.d
%_datadir/%name/sssd.api.d/sssd-local.conf
%_datadir/%name/sssd.api.d/sssd-simple.conf
%_man1dir/sss_ssh_*
%_man5dir/sssd.conf.5*
%_man5dir/sssd-simple.5*
%_man5dir/sssd-sudo.5*
%_man8dir/sssd.8*
%_man8dir/sss_cache.8*

%files -n python-module-sss
%python_sitelibdir/pysss.so
%python_sitelibdir/pysss_murmur.so

%files ldap
%_libdir/%name/libsss_ldap.so
%_man5dir/sssd-ldap*
%_datadir/%name/sssd.api.d/sssd-ldap.conf

%files krb5-common
%_libdir/%name/libsss_krb5_common.so
%_libexecdir/%name/ldap_child
%_libexecdir/%name/krb5_child

%files krb5
%_libdir/%name/libsss_krb5.so
%_man5dir/sssd-krb5*
%_datadir/%name/sssd.api.d/sssd-krb5.conf

%files pac
%_libexecdir/%name/sssd_pac

%files ipa
%dir %pubconfpath/krb5.include.d
%_libdir/%name/libsss_ipa.so
%_man5dir/sssd-ipa*
%_datadir/%name/sssd.api.d/sssd-ipa.conf

%files ad
%_libdir/%name/libsss_ad.so
%_man5dir/sssd-ad*
%_datadir/%name/sssd.api.d/sssd-ad.conf

%files proxy
%_libexecdir/%name/proxy_child
%_libdir/%name/libsss_proxy.so
%_datadir/%name/sssd.api.d/sssd-proxy.conf

%files client
%config(noreplace) %_pamdir/*-sss
/%_lib/libnss_sss.so.2
/%_lib/security/pam_sss.so
%_libdir/krb5/plugins/libkrb5/sssd_krb5_locator_plugin.so
%_libdir/krb5/plugins/authdata/sssd_pac_plugin.so
# %_libdir/cifs-utils/cifs_idmap_sss.so
%_man8dir/pam_sss*
%_man8dir/sssd_krb5_locator_plugin*

%files tools
%_sbindir/sss_*
%_man8dir/sss_*
%exclude %_sbindir/sss_cache
%exclude %_man8dir/sss_cache*

%files -n python-module-sssdconfig
%dir %python_sitelibdir_noarch/SSSDConfig
%python_sitelibdir_noarch/SSSDConfig/*.py

%files -n libsss_idmap
%_libdir/libsss_idmap.so.*

%files -n libsss_idmap-devel
%doc idmap_doc/html
%_includedir/sss_idmap.h
%_libdir/libsss_idmap.so
%_pkgconfigdir/sss_idmap.pc

%files -n libipa_hbac
%_libdir/libipa_hbac.so.*

%files -n libipa_hbac-devel
%doc hbac_doc/html
%_includedir/ipa_hbac.h
%_libdir/libipa_hbac.so
%_pkgconfigdir/ipa_hbac.pc

%files -n python-module-ipa_hbac
%python_sitelibdir/pyhbac.so

%files -n libsss_nss_idmap
%_libdir/libsss_nss_idmap.so.*

%files -n libsss_nss_idmap-devel
%doc nss_idmap_doc/html
%_includedir/sss_nss_idmap.h
%_libdir/libsss_nss_idmap.so
%_pkgconfigdir/sss_nss_idmap.pc

%files -n python-module-sss_nss_idmap
%python_sitelibdir/pysss_nss_idmap.so

%changelog
* Tue Feb 18 2014 Alexey Shabalin <shaba@altlinux.ru> 1.11.4-alt1
- 1.11.4

* Tue Feb 04 2014 Alexey Shabalin <shaba@altlinux.ru> 1.11.3-alt1
- initial build
