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

#ifndef PIPEWIRE_IMPL_CORE_H
#define PIPEWIRE_IMPL_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/** \class pw_impl_core
 *
 * \brief PipeWire core interface.
 *
 * The core is used to make objects on demand.
 */
struct pw_impl_core;

#include <pipewire/context.h>
#include <pipewire/global.h>
#include <pipewire/properties.h>
#include <pipewire/resource.h>

/** Factory events, listen to them with \ref pw_impl_core_add_listener */
struct pw_impl_core_events {
#define PW_VERSION_IMPL_CORE_EVENTS	0
	uint32_t version;

	/** the core is destroyed */
        void (*destroy) (void *data);
	/** the core is freed */
        void (*free) (void *data);
	/** the core is initialized */
        void (*initialized) (void *data);
};

struct pw_impl_core *pw_context_create_core(struct pw_context *context,
				  struct pw_properties *properties,
				  size_t user_data_size);

/* get the default core in a context */
struct pw_impl_core *pw_context_get_default_core(struct pw_context *context);

/** Get the core properties */
const struct pw_properties *pw_impl_core_get_properties(struct pw_impl_core *core);

/** Get the core information */
const struct pw_core_info *pw_impl_core_get_info(struct pw_impl_core *core);

/** Update the core properties */
int pw_impl_core_update_properties(struct pw_impl_core *core, const struct spa_dict *dict);

int pw_impl_core_register(struct pw_impl_core *core,
			struct pw_properties *properties);

void pw_impl_core_destroy(struct pw_impl_core *core);

void *pw_impl_core_get_user_data(struct pw_impl_core *core);

/** Get the global of this core */
struct pw_global *pw_impl_core_get_global(struct pw_impl_core *core);

/** Add an event listener */
void pw_impl_core_add_listener(struct pw_impl_core *core,
			     struct spa_hook *listener,
			     const struct pw_impl_core_events *events,
			     void *data);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_IMPL_CORE_H */
