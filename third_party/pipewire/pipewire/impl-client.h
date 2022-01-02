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

#ifndef PIPEWIRE_IMPL_CLIENT_H
#define PIPEWIRE_IMPL_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/hook.h>

/** \class pw_impl_client
 *
 * \brief PipeWire client object class.
 *
 * The client object represents a client connection with the PipeWire
 * server.
 *
 * Each client has its own list of resources it is bound to along with
 * a mapping between the client types and server types.
 */
struct pw_impl_client;

#include <pipewire/context.h>
#include <pipewire/global.h>
#include <pipewire/properties.h>
#include <pipewire/resource.h>
#include <pipewire/permission.h>

/** \page page_client Client
 *
 * \section sec_page_client_overview Overview
 *
 * The \ref pw_impl_client object is created by a protocol implementation when
 * a new client connects.
 *
 * The client is used to keep track of all resources belonging to one
 * connection with the PipeWire server.
 *
 * \section sec_page_client_credentials Credentials
 *
 * The client object will have its credentials filled in by the protocol.
 * This information is used to check if a resource or action is available
 * for this client. See also \ref page_access
 *
 * \section sec_page_client_types Types
 *
 * The client and server maintain a mapping between the client and server
 * types. All type ids that are in messages exchanged between the client
 * and server will automatically be remapped. See also \ref page_types.
 *
 * \section sec_page_client_resources Resources
 *
 * When a client binds to context global object, a resource is made for this
 * binding and a unique id is assigned to the resources. The client and
 * server will use this id as the destination when exchanging messages.
 * See also \ref page_resource
 */

/** The events that a client can emit */
struct pw_impl_client_events {
#define PW_VERSION_IMPL_CLIENT_EVENTS	0
        uint32_t version;

	/** emitted when the client is destroyed */
	void (*destroy) (void *data);

	/** emitted right before the client is freed */
	void (*free) (void *data);

	/** the client is initialized */
	void (*initialized) (void *data);

	/** emitted when the client info changed */
	void (*info_changed) (void *data, const struct pw_client_info *info);

	/** emitted when a new resource is added for client */
	void (*resource_added) (void *data, struct pw_resource *resource);

	/** emitted when a resource is removed */
	void (*resource_removed) (void *data, struct pw_resource *resource);

	/** emitted when the client becomes busy processing an asynchronous
	 * message. In the busy state no messages should be processed.
	 * Processing should resume when the client becomes not busy */
	void (*busy_changed) (void *data, bool busy);
};

/** Create a new client. This is mainly used by protocols. */
struct pw_impl_client *
pw_context_create_client(struct pw_impl_core *core,		/**< the core object */
			struct pw_protocol *prototol,		/**< the client protocol */
			struct pw_properties *properties,	/**< client properties */
			size_t user_data_size			/**< extra user data size */);

/** Destroy a previously created client */
void pw_impl_client_destroy(struct pw_impl_client *client);

/** Finish configuration and register a client */
int pw_impl_client_register(struct pw_impl_client *client,	/**< the client to register */
		       struct pw_properties *properties/**< extra properties */);

/** Get the client user data */
void *pw_impl_client_get_user_data(struct pw_impl_client *client);

/** Get the client information */
const struct pw_client_info *pw_impl_client_get_info(struct pw_impl_client *client);

/** Update the client properties */
int pw_impl_client_update_properties(struct pw_impl_client *client, const struct spa_dict *dict);

/** Update the client permissions */
int pw_impl_client_update_permissions(struct pw_impl_client *client, uint32_t n_permissions,
		const struct pw_permission *permissions);

/** check if a client has permissions for global_id, Since 0.3.9 */
int pw_impl_client_check_permissions(struct pw_impl_client *client,
		uint32_t global_id, uint32_t permissions);

/** Get the client properties */
const struct pw_properties *pw_impl_client_get_properties(struct pw_impl_client *client);

/** Get the context used to create this client */
struct pw_context *pw_impl_client_get_context(struct pw_impl_client *client);
/** Get the protocol used to create this client */
struct pw_protocol *pw_impl_client_get_protocol(struct pw_impl_client *client);

/** Get the client core resource */
struct pw_resource *pw_impl_client_get_core_resource(struct pw_impl_client *client);

/** Get a resource with the given id */
struct pw_resource *pw_impl_client_find_resource(struct pw_impl_client *client, uint32_t id);

/** Get the global associated with this client */
struct pw_global *pw_impl_client_get_global(struct pw_impl_client *client);

/** listen to events from this client */
void pw_impl_client_add_listener(struct pw_impl_client *client,
			    struct spa_hook *listener,
			    const struct pw_impl_client_events *events,
			    void *data);


/** Mark the client busy. This can be used when an asynchronous operation is
  * started and no further processing is allowed to happen for the client */
void pw_impl_client_set_busy(struct pw_impl_client *client, bool busy);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_IMPL_CLIENT_H */
