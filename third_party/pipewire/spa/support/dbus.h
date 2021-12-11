/* Simple Plugin API
 *
 * Copyright © 2018 Wim Taymans
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef SPA_DBUS_H
#define SPA_DBUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/support/loop.h>

#define SPA_TYPE_INTERFACE_DBus		SPA_TYPE_INFO_INTERFACE_BASE "DBus"

#define SPA_VERSION_DBUS		0
struct spa_dbus { struct spa_interface iface; };

enum spa_dbus_type {
	SPA_DBUS_TYPE_SESSION,	/**< The login session bus */
	SPA_DBUS_TYPE_SYSTEM,	/**< The systemwide bus */
	SPA_DBUS_TYPE_STARTER	/**< The bus that started us, if any */
};

struct spa_dbus_connection {
#define SPA_VERSION_DBUS_CONNECTION	0
        uint32_t version;
	/**
	 * Get the DBusConnection from a wrapper
	 *
	 * \param conn the spa_dbus_connection wrapper
	 * \return a pointer of type DBusConnection
	 */
	void *(*get) (struct spa_dbus_connection *conn);
	/**
	 * Destroy a dbus connection wrapper
	 *
	 * \param conn the wrapper to destroy
	 */
	void (*destroy) (struct spa_dbus_connection *conn);
};

#define spa_dbus_connection_get(c)	(c)->get((c))
#define spa_dbus_connection_destroy(c)	(c)->destroy((c))

struct spa_dbus_methods {
#define SPA_VERSION_DBUS_METHODS	0
        uint32_t version;

	/**
	 * Get a new connection wrapper for the given bus type.
	 *
	 * The connection wrapper is completely configured to operate
	 * in the main context of the handle that manages the spa_dbus
	 * interface.
	 *
	 * \param dbus the dbus manager
	 * \param type the bus type to wrap
	 * \param error location for the DBusError
	 * \return a new dbus connection wrapper or NULL on error
	 */
	struct spa_dbus_connection * (*get_connection) (void *object,
							enum spa_dbus_type type);
};

static inline struct spa_dbus_connection *
spa_dbus_get_connection(struct spa_dbus *dbus, enum spa_dbus_type type)
{
	struct spa_dbus_connection *res = NULL;
	spa_interface_call_res(&dbus->iface,
                        struct spa_dbus_methods, res,
			get_connection, 0, type);
	return res;
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* SPA_DBUS_H */
