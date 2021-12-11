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

#ifndef PIPEWIRE_IMPL_LINK_H
#define PIPEWIRE_IMPL_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

/** \class pw_impl_link
 *
 * PipeWire link object.
 */
struct pw_impl_link;
struct pw_impl_port;

#include <pipewire/impl.h>

/** \page page_link Link
 *
 * \section page_link_overview Overview
 *
 * A link is the connection between 2 nodes (\ref page_node). Nodes are
 * linked together on ports.
 *
 * The link is responsible for negotiating the format and buffers for
 * the nodes.
 */

/** link events added with \ref pw_impl_link_add_listener */
struct pw_impl_link_events {
#define PW_VERSION_IMPL_LINK_EVENTS	0
	uint32_t version;

	/** A link is destroyed */
	void (*destroy) (void *data);

	/** A link is freed */
	void (*free) (void *data);

	/** a Link is initialized */
	void (*initialized) (void *data);

	/** The info changed on a link */
	void (*info_changed) (void *data, const struct pw_link_info *info);

	/** The link state changed, \a error is only valid when the state is
	  * in error. */
	void (*state_changed) (void *data, enum pw_link_state old,
					   enum pw_link_state state, const char *error);

	/** A port is unlinked */
	void (*port_unlinked) (void *data, struct pw_impl_port *port);
};


/** Make a new link between two ports \memberof pw_impl_link
 * \return a newly allocated link */
struct pw_impl_link *
pw_context_create_link(struct pw_context *context,		/**< the context object */
	    struct pw_impl_port *output,		/**< an output port */
	    struct pw_impl_port *input,		/**< an input port */
	    struct spa_pod *format_filter,	/**< an optional format filter */
	    struct pw_properties *properties	/**< extra properties */,
	    size_t user_data_size		/**< extra user data size */);

/** Destroy a link \memberof pw_impl_link */
void pw_impl_link_destroy(struct pw_impl_link *link);

/** Add an event listener to \a link */
void pw_impl_link_add_listener(struct pw_impl_link *link,
			  struct spa_hook *listener,
			  const struct pw_impl_link_events *events,
			  void *data);

/** Finish link configuration and register */
int pw_impl_link_register(struct pw_impl_link *link,		/**< the link to register */
		     struct pw_properties *properties	/**< extra properties */);

/** Get the context of a link */
struct pw_context *pw_impl_link_get_context(struct pw_impl_link *link);

/** Get the user_data of a link, the size of the memory is given when
  * constructing the link */
void *pw_impl_link_get_user_data(struct pw_impl_link *link);

/** Get the link info */
const struct pw_link_info *pw_impl_link_get_info(struct pw_impl_link *link);

/** Get the global of the link */
struct pw_global *pw_impl_link_get_global(struct pw_impl_link *link);

/** Get the output port of the link */
struct pw_impl_port *pw_impl_link_get_output(struct pw_impl_link *link);

/** Get the input port of the link */
struct pw_impl_port *pw_impl_link_get_input(struct pw_impl_link *link);

/** Find the link between 2 ports \memberof pw_impl_link */
struct pw_impl_link *pw_impl_link_find(struct pw_impl_port *output, struct pw_impl_port *input);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_IMPL_LINK_H */
