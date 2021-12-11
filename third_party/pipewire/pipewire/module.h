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

#ifndef PIPEWIRE_MODULE_H
#define PIPEWIRE_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/defs.h>
#include <spa/utils/hook.h>

#include <pipewire/proxy.h>

#define PW_TYPE_INTERFACE_Module	PW_TYPE_INFO_INTERFACE_BASE "Module"

#define PW_VERSION_MODULE		3
struct pw_module;

/** The module information. Extra information can be added in later versions \memberof pw_introspect */
struct pw_module_info {
	uint32_t id;		/**< id of the global */
	const char *name;	/**< name of the module */
	const char *filename;	/**< filename of the module */
	const char *args;	/**< arguments passed to the module */
#define PW_MODULE_CHANGE_MASK_PROPS	(1 << 0)
#define PW_MODULE_CHANGE_MASK_ALL	((1 << 1)-1)
	uint64_t change_mask;	/**< bitfield of changed fields since last call */
	struct spa_dict *props;	/**< extra properties */
};

/** Update and existing \ref pw_module_info with \a update \memberof pw_introspect */
struct pw_module_info *
pw_module_info_update(struct pw_module_info *info,
		      const struct pw_module_info *update);

/** Free a \ref pw_module_info \memberof pw_introspect */
void pw_module_info_free(struct pw_module_info *info);

#define PW_MODULE_EVENT_INFO		0
#define PW_MODULE_EVENT_NUM		1

/** Module events */
struct pw_module_events {
#define PW_VERSION_MODULE_EVENTS	0
	uint32_t version;
	/**
	 * Notify module info
	 *
	 * \param info info about the module
	 */
	void (*info) (void *object, const struct pw_module_info *info);
};

#define PW_MODULE_METHOD_ADD_LISTENER	0
#define PW_MODULE_METHOD_NUM		1

/** Module methods */
struct pw_module_methods {
#define PW_VERSION_MODULE_METHODS	0
	uint32_t version;

	int (*add_listener) (void *object,
			struct spa_hook *listener,
			const struct pw_module_events *events,
			void *data);
};

#define pw_module_method(o,method,version,...)				\
({									\
	int _res = -ENOTSUP;						\
	spa_interface_call_res((struct spa_interface*)o,		\
			struct pw_module_methods, _res,			\
			method, version, ##__VA_ARGS__);		\
	_res;								\
})

#define pw_module_add_listener(c,...)	pw_module_method(c,add_listener,0,__VA_ARGS__)

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* PIPEWIRE_MODULE_H */
