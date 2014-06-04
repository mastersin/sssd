/* The following declarations are auto-generated from ifp_iface.xml */

#ifndef __IFP_IFACE_XML__
#define __IFP_IFACE_XML__

#include "sbus/sssd_dbus.h"

/* ------------------------------------------------------------------------
 * DBus Constants
 *
 * Various constants of interface and method names mostly for use by clients
 */

/* constants for org.freedesktop.sssd.infopipe */
#define INFOPIPE_IFACE "org.freedesktop.sssd.infopipe"
#define INFOPIPE_IFACE_PING "Ping"
#define INFOPIPE_IFACE_GETUSERATTR "GetUserAttr"
#define INFOPIPE_IFACE_GETUSERGROUPS "GetUserGroups"

/* ------------------------------------------------------------------------
 * DBus handlers
 *
 * These structures are filled in by implementors of the different
 * dbus interfaces to handle method calls.
 *
 * Handler functions of type sbus_msg_handler_fn accept raw messages,
 * other handlers are typed appropriately. If a handler that is
 * set to NULL is invoked it will result in a
 * org.freedesktop.DBus.Error.NotSupported error for the caller.
 *
 * Handlers have a matching xxx_finish() function (unless the method has
 * accepts raw messages). These finish functions the
 * sbus_request_return_and_finish() with the appropriate arguments to
 * construct a valid reply. Once a finish function has been called, the
 * @dbus_req it was called with is freed and no longer valid.
 */

/* vtable for org.freedesktop.sssd.infopipe */
struct infopipe_iface {
    struct sbus_vtable vtable; /* derive from sbus_vtable */
    sbus_msg_handler_fn Ping;
    sbus_msg_handler_fn GetUserAttr;
    int (*GetUserGroups)(struct sbus_request *req, void *data, const char *arg_user);
};

/* finish function for GetUserGroups */
int infopipe_iface_GetUserGroups_finish(struct sbus_request *req, const char *arg_values[], int len_values);

/* ------------------------------------------------------------------------
 * DBus Interface Metadata
 *
 * These structure definitions are filled in with the information about
 * the interfaces, methods, properties and so on.
 *
 * The actual definitions are found in the accompanying C file next
 * to this header.
 */

/* interface info for org.freedesktop.sssd.infopipe */
extern const struct sbus_interface_meta infopipe_iface_meta;

#endif /* __IFP_IFACE_XML__ */
