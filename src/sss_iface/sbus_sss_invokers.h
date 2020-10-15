/*
    Generated by sbus code generator

    Copyright (C) 2017 Red Hat

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

#ifndef _SBUS_SSS_INVOKERS_H_
#define _SBUS_SSS_INVOKERS_H_

#include <talloc.h>
#include <tevent.h>
#include <dbus/dbus.h>

#include "sbus/sbus_interface_declarations.h"
#include "sbus/sbus_request.h"

#define _sbus_sss_declare_invoker(input, output)                               \
    struct tevent_req *                                                   \
    _sbus_sss_invoke_in_ ## input ## _out_ ## output ## _send                 \
        (TALLOC_CTX *mem_ctx,                                             \
         struct tevent_context *ev,                                       \
         struct sbus_request *sbus_req,                                   \
         sbus_invoker_keygen keygen,                                      \
         const struct sbus_handler *handler,                              \
         DBusMessageIter *read_iterator,                                  \
         DBusMessageIter *write_iterator,                                 \
         const char **_key)

_sbus_sss_declare_invoker(, );
_sbus_sss_declare_invoker(pam_data, pam_response);
_sbus_sss_declare_invoker(raw, qus);
_sbus_sss_declare_invoker(s, );
_sbus_sss_declare_invoker(s, as);
_sbus_sss_declare_invoker(s, b);
_sbus_sss_declare_invoker(s, qus);
_sbus_sss_declare_invoker(s, s);
_sbus_sss_declare_invoker(sqq, q);
_sbus_sss_declare_invoker(ss, o);
_sbus_sss_declare_invoker(ssau, );
_sbus_sss_declare_invoker(u, );
_sbus_sss_declare_invoker(us, );
_sbus_sss_declare_invoker(usq, );
_sbus_sss_declare_invoker(uss, );
_sbus_sss_declare_invoker(uss, qus);
_sbus_sss_declare_invoker(uus, qus);
_sbus_sss_declare_invoker(uusss, qus);
_sbus_sss_declare_invoker(uuus, qus);

#endif /* _SBUS_SSS_INVOKERS_H_ */
