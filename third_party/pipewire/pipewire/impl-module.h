/* PipeWire
 * Copyright © 2016 Axis Communications <dev-gstreamer@axis.com>
 *	@author Linus Svensson <linus.svensson@axis.com>
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

#ifndef PIPEWIRE_IMPL_MODULE_H
#define PIPEWIRE_IMPL_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/hook.h>

#include <pipewire/context.h>

#define PIPEWIRE_SYMBOL_MODULE_INIT "pipewire__module_init"
#define PIPEWIRE_MODULE_PREFIX "libpipewire-"

/** \class pw_impl_module
 *
 * A dynamically loadable module
 */
struct pw_impl_module;

/** Module init function signature
 *
 * \param module A \ref pw_impl_module
 * \param args Arguments to the module
 * \return 0 on success, < 0 otherwise with an errno style error
 *
 * A module should provide an init function with this signature. This function
 * will be called when a module is loaded.
 *
 * \memberof pw_impl_module
 */
typedef int (*pw_impl_module_init_func_t) (struct pw_impl_module *module, const char *args);

/** Module events added with \ref pw_impl_module_add_listener */
struct pw_impl_module_events {
#define PW_VERSION_IMPL_MODULE_EVENTS	0
	uint32_t version;

	/** The module is destroyed */
	void (*destroy) (void *data);
	/** The module is freed */
	void (*free) (void *data);
	/** The module is initialized */
	void (*initialized) (void *data);

	/** The module is registered. This is a good time to register
	 * objectes created from the module. */
	void (*registered) (void *data);
};

struct pw_impl_module *
pw_context_load_module(struct pw_context *context,
	       const char *name,		/**< name of the module */
	       const char *args			/**< arguments of the module */,
	       struct pw_properties *properties	/**< extra global properties */);

/** Get the context of a module */
struct pw_context * pw_impl_module_get_context(struct pw_impl_module *module);

/** Get the global of a module */
struct pw_global * pw_impl_module_get_global(struct pw_impl_module *module);

/** Get the node properties */
const struct pw_properties *pw_impl_module_get_properties(struct pw_impl_module *module);

/** Update the module properties */
int pw_impl_module_update_properties(struct pw_impl_module *module, const struct spa_dict *dict);

/** Get the module info */
const struct pw_module_info *pw_impl_module_get_info(struct pw_impl_module *module);

/** Add an event listener to a module */
void pw_impl_module_add_listener(struct pw_impl_module *module,
			    struct spa_hook *listener,
			    const struct pw_impl_module_events *events,
			    void *data);

/** Destroy a module */
void pw_impl_module_destroy(struct pw_impl_module *module);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_IMPL_MODULE_H */
