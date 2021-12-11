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

#ifndef PIPEWIRE_LINK_H
#define PIPEWIRE_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/defs.h>
#include <spa/utils/hook.h>

#include <pipewire/proxy.h>

#define PW_TYPE_INTERFACE_Link	PW_TYPE_INFO_INTERFACE_BASE "Link"

#define PW_VERSION_LINK		3
struct pw_link;

/** \enum pw_link_state The different link states \memberof pw_link */
enum pw_link_state {
	PW_LINK_STATE_ERROR = -2,	/**< the link is in error */
	PW_LINK_STATE_UNLINKED = -1,	/**< the link is unlinked */
	PW_LINK_STATE_INIT = 0,		/**< the link is initialized */
	PW_LINK_STATE_NEGOTIATING = 1,	/**< the link is negotiating formats */
	PW_LINK_STATE_ALLOCATING = 2,	/**< the link is allocating buffers */
	PW_LINK_STATE_PAUSED = 3,	/**< the link is paused */
};

/** Convert a \ref pw_link_state to a readable string \memberof pw_link */
const char * pw_link_state_as_string(enum pw_link_state state);
/** The link information. Extra information can be added in later versions \memberof pw_introspect */
struct pw_link_info {
	uint32_t id;			/**< id of the global */
	uint32_t output_node_id;	/**< server side output node id */
	uint32_t output_port_id;	/**< output port id */
	uint32_t input_node_id;		/**< server side input node id */
	uint32_t input_port_id;		/**< input port id */
#define PW_LINK_CHANGE_MASK_STATE	(1 << 0)
#define PW_LINK_CHANGE_MASK_FORMAT	(1 << 1)
#define PW_LINK_CHANGE_MASK_PROPS	(1 << 2)
#define PW_LINK_CHANGE_MASK_ALL		((1 << 3)-1)
	uint64_t change_mask;		/**< bitfield of changed fields since last call */
	enum pw_link_state state;	/**< the current state of the link */
	const char *error;		/**< an error reason if \a state is error */
	struct spa_pod *format;		/**< format over link */
	struct spa_dict *props;		/**< the properties of the link */
};

struct pw_link_info *
pw_link_info_update(struct pw_link_info *info,
		    const struct pw_link_info *update);

void
pw_link_info_free(struct pw_link_info *info);


#define PW_LINK_EVENT_INFO	0
#define PW_LINK_EVENT_NUM	1

/** Link events */
struct pw_link_events {
#define PW_VERSION_LINK_EVENTS	0
	uint32_t version;
	/**
	 * Notify link info
	 *
	 * \param info info about the link
	 */
	void (*info) (void *object, const struct pw_link_info *info);
};

#define PW_LINK_METHOD_ADD_LISTENER	0
#define PW_LINK_METHOD_NUM		1

/** Link methods */
struct pw_link_methods {
#define PW_VERSION_LINK_METHODS		0
	uint32_t version;

	int (*add_listener) (void *object,
			struct spa_hook *listener,
			const struct pw_link_events *events,
			void *data);
};

#define pw_link_method(o,method,version,...)				\
({									\
	int _res = -ENOTSUP;						\
	spa_interface_call_res((struct spa_interface*)o,		\
			struct pw_link_methods, _res,			\
			method, version, ##__VA_ARGS__);		\
	_res;								\
})

#define pw_link_add_listener(c,...)		pw_link_method(c,add_listener,0,__VA_ARGS__)

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* PIPEWIRE_LINK_H */
