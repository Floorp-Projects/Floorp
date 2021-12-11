/* PipeWire
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

#ifndef PIPEWIRE_GLOBAL_H
#define PIPEWIRE_GLOBAL_H

#ifdef __cplusplus
extern "C" {
#endif

/** \page page_global Global
 *
 * Global objects represent resources that are available on the PipeWire
 * context and are accessible to remote clients.
 * Globals come and go when devices or other resources become available for
 * clients.
 *
 * Remote clients receives a list of globals when it binds to the registry
 * object. See \ref page_registry.
 *
 * A client can bind to a global to send methods or receive events from
 * the global.
 */
/** \class pw_global
 *
 * \brief A global object visible to remote clients
 *
 * A global object is visible to remote clients and represents a resource
 * that can be used or inspected.
 *
 * See \ref page_remote_api
 */
struct pw_global;

#include <pipewire/impl.h>

typedef int (*pw_global_bind_func_t) (void *object,
		      struct pw_impl_client *client,	/**< client that binds */
		      uint32_t permissions,	/**< permissions for the bind */
		      uint32_t version,		/**< client interface version */
		      uint32_t id		/**< client proxy id */);

/** Global events, use \ref pw_global_add_listener */
struct pw_global_events {
#define PW_VERSION_GLOBAL_EVENTS 0
	uint32_t version;

	/** The global is destroyed */
	void (*destroy) (void *data);
	/** The global is freed */
	void (*free) (void *data);
	/** The permissions changed for a client */
	void (*permissions_changed) (void *data,
			struct pw_impl_client *client,
			uint32_t old_permissions,
			uint32_t new_permissions);
};

/** Create a new global object */
struct pw_global *
pw_global_new(struct pw_context *context,	/**< the context */
	      const char *type,			/**< the interface type of the global */
	      uint32_t version,			/**< the interface version of the global */
	      struct pw_properties *properties,	/**< extra properties */
	      pw_global_bind_func_t func,	/**< function to bind */
	      void *object			/**< global object */);

/** Register a global object to the context registry */
int pw_global_register(struct pw_global *global);

/** Add an event listener on the global */
void pw_global_add_listener(struct pw_global *global,
			    struct spa_hook *listener,
			    const struct pw_global_events *events,
			    void *data);

/** Get the permissions of the global for a given client */
uint32_t pw_global_get_permissions(struct pw_global *global, struct pw_impl_client *client);

/** Get the context object of this global */
struct pw_context *pw_global_get_context(struct pw_global *global);

/** Get the global type */
const char *pw_global_get_type(struct pw_global *global);

/** Check a global type */
bool pw_global_is_type(struct pw_global *global, const char *type);

/** Get the global version */
uint32_t pw_global_get_version(struct pw_global *global);

/** Get the global properties */
const struct pw_properties *pw_global_get_properties(struct pw_global *global);

/** Update the global properties, must be done when unregistered */
int pw_global_update_keys(struct pw_global *global,
		     const struct spa_dict *dict, const char *keys[]);

/** Get the object associated with the global. This depends on the type of the
  * global */
void *pw_global_get_object(struct pw_global *global);

/** Get the unique id of the global */
uint32_t pw_global_get_id(struct pw_global *global);

/** Add a resource to a global */
int pw_global_add_resource(struct pw_global *global, struct pw_resource *resource);

/** Iterate all resources added to the global The callback should return
 * 0 to fetch the next item, any other value stops the iteration and returns
 * the value. When all callbacks return 0, this function returns 0 when all
 * items are iterated. */
int pw_global_for_each_resource(struct pw_global *global,
			   int (*callback) (void *data, struct pw_resource *resource),
			   void *data);

/** Let a client bind to a global */
int pw_global_bind(struct pw_global *global,
		   struct pw_impl_client *client,
		   uint32_t permissions,
		   uint32_t version,
		   uint32_t id);

int pw_global_update_permissions(struct pw_global *global, struct pw_impl_client *client,
		uint32_t old_permissions, uint32_t new_permissions);

/** Destroy a global */
void pw_global_destroy(struct pw_global *global);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_GLOBAL_H */
