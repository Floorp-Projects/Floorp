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

#ifndef PIPEWIRE_IMPL_DEVICE_H
#define PIPEWIRE_IMPL_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/** \class pw_impl_device
 *
 * \brief PipeWire device interface.
 *
 * The device is an object that manages nodes. It typically
 * corresponds to a physical hardware device but it does not
 * have to be.
 *
 * The purpose of the device is to provide an interface to
 * dynamically create/remove/configure the nodes it manages.
 */
struct pw_impl_device;

#include <spa/monitor/device.h>

#include <pipewire/context.h>
#include <pipewire/global.h>
#include <pipewire/properties.h>
#include <pipewire/resource.h>

/** Device events, listen to them with \ref pw_impl_device_add_listener */
struct pw_impl_device_events {
#define PW_VERSION_IMPL_DEVICE_EVENTS	0
	uint32_t version;

	/** the device is destroyed */
        void (*destroy) (void *data);
	/** the device is freed */
        void (*free) (void *data);
	/** the device is initialized */
        void (*initialized) (void *data);

	/** the device info changed */
	void (*info_changed) (void *data, const struct pw_device_info *info);
};

struct pw_impl_device *pw_context_create_device(struct pw_context *context,
				struct pw_properties *properties,
				size_t user_data_size);

int pw_impl_device_register(struct pw_impl_device *device,
		       struct pw_properties *properties);

void pw_impl_device_destroy(struct pw_impl_device *device);

void *pw_impl_device_get_user_data(struct pw_impl_device *device);

/** Set the device implementation */
int pw_impl_device_set_implementation(struct pw_impl_device *device, struct spa_device *spa_device);
/** Get the device implementation */
struct spa_device *pw_impl_device_get_implementation(struct pw_impl_device *device);

/** Get the global of this device */
struct pw_global *pw_impl_device_get_global(struct pw_impl_device *device);

/** Add an event listener */
void pw_impl_device_add_listener(struct pw_impl_device *device,
			    struct spa_hook *listener,
			    const struct pw_impl_device_events *events,
			    void *data);

int pw_impl_device_update_properties(struct pw_impl_device *device, const struct spa_dict *dict);

const struct pw_properties *pw_impl_device_get_properties(struct pw_impl_device *device);

int pw_impl_device_for_each_param(struct pw_impl_device *device,
			     int seq, uint32_t param_id,
			     uint32_t index, uint32_t max,
			     const struct spa_pod *filter,
			     int (*callback) (void *data, int seq,
					      uint32_t id, uint32_t index, uint32_t next,
					      struct spa_pod *param),
			     void *data);
#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_IMPL_DEVICE_H */
