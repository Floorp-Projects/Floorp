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

#ifndef SPA_HOOK_H
#define SPA_HOOK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/defs.h>
#include <spa/utils/list.h>

/** \class spa_hook
 *
 * \brief a list of hooks
 *
 * The hook list provides a way to keep track of hooks.
 */
/** A list of hooks */
struct spa_hook_list {
	struct spa_list list;
};

/** Callbacks, contains the structure with functions and the data passed
 * to the functions.  The structure should also contain a version field that
 * is checked. */
struct spa_callbacks {
	const void *funcs;
	void *data;
};

/** Check if a callback \c has method \m of version \v */
#define SPA_CALLBACK_CHECK(c,m,v) ((c) && ((v) == 0 || (c)->version > (v)-1) && (c)->m)

#define SPA_CALLBACKS_INIT(_funcs,_data) (struct spa_callbacks){ _funcs, _data, }

struct spa_interface {
	const char *type;
	uint32_t version;
	struct spa_callbacks cb;
};

#define SPA_INTERFACE_INIT(_type,_version,_funcs,_data) \
	(struct spa_interface){ _type, _version, SPA_CALLBACKS_INIT(_funcs,_data), }

/** A hook, contains the structure with functions and the data passed
 * to the functions. */
struct spa_hook {
	struct spa_list link;
	struct spa_callbacks cb;
	/** callback and data for the hook list */
	void (*removed) (struct spa_hook *hook);
	void *priv;
};

/** Initialize a hook list */
static inline void spa_hook_list_init(struct spa_hook_list *list)
{
	spa_list_init(&list->list);
}

static inline bool spa_hook_list_is_empty(struct spa_hook_list *list)
{
	return spa_list_is_empty(&list->list);
}

/** Append a hook \memberof spa_hook */
static inline void spa_hook_list_append(struct spa_hook_list *list,
					struct spa_hook *hook,
					const void *funcs, void *data)
{
	hook->cb = SPA_CALLBACKS_INIT(funcs, data);
	spa_list_append(&list->list, &hook->link);
}

/** Prepend a hook \memberof spa_hook */
static inline void spa_hook_list_prepend(struct spa_hook_list *list,
					 struct spa_hook *hook,
					 const void *funcs, void *data)
{
	hook->cb = SPA_CALLBACKS_INIT(funcs, data);
	spa_list_prepend(&list->list, &hook->link);
}

/** Remove a hook \memberof spa_hook */
static inline void spa_hook_remove(struct spa_hook *hook)
{
        spa_list_remove(&hook->link);
	if (hook->removed)
		hook->removed(hook);
}

static inline void
spa_hook_list_isolate(struct spa_hook_list *list,
		struct spa_hook_list *save,
		struct spa_hook *hook,
		const void *funcs, void *data)
{
	/* init save list and move hooks to it */
	spa_hook_list_init(save);
	spa_list_insert_list(&save->list, &list->list);
	/* init hooks and add single hook */
	spa_hook_list_init(list);
	spa_hook_list_append(list, hook, funcs, data);
}

static inline void
spa_hook_list_join(struct spa_hook_list *list,
		struct spa_hook_list *save)
{
	spa_list_insert_list(&list->list, &save->list);
}

#define spa_callbacks_call(callbacks,type,method,vers,...)			\
({										\
	const type *_f = (const type *) (callbacks)->funcs;			\
	if (SPA_LIKELY(SPA_CALLBACK_CHECK(_f,method,vers)))			\
		_f->method((callbacks)->data, ## __VA_ARGS__);			\
})

#define spa_callbacks_call_res(callbacks,type,res,method,vers,...)		\
({										\
	const type *_f = (const type *) (callbacks)->funcs;			\
	if (SPA_LIKELY(SPA_CALLBACK_CHECK(_f,method,vers)))			\
		res = _f->method((callbacks)->data, ## __VA_ARGS__);		\
	res;									\
})

#define spa_interface_call(iface,type,method,vers,...)				\
	spa_callbacks_call(&(iface)->cb,type,method,vers,##__VA_ARGS__)

#define spa_interface_call_res(iface,type,res,method,vers,...)			\
	spa_callbacks_call_res(&(iface)->cb,type,res,method,vers,##__VA_ARGS__)

#define spa_hook_list_call_simple(l,type,method,vers,...)			\
({										\
	struct spa_hook_list *_l = l;						\
	struct spa_hook *_h, *_t;						\
	spa_list_for_each_safe(_h, _t, &_l->list, link)				\
		spa_callbacks_call(&_h->cb,type,method,vers, ## __VA_ARGS__);	\
})

/** Call all hooks in a list, starting from the given one and optionally stopping
 * after calling the first non-NULL function, returns the number of methods
 * called */
#define spa_hook_list_do_call(l,start,type,method,vers,once,...)		\
({										\
	struct spa_hook_list *list = l;						\
	struct spa_list *s = start ? (struct spa_list *)start : &list->list;	\
	struct spa_hook cursor = { 0 }, *ci;					\
	int count = 0;								\
	spa_list_cursor_start(cursor, s, link);					\
	spa_list_for_each_cursor(ci, cursor, &list->list, link) {		\
		const type *_f = (const type *)ci->cb.funcs;			\
		if (SPA_LIKELY(SPA_CALLBACK_CHECK(_f,method,vers))) {		\
			_f->method(ci->cb.data, ## __VA_ARGS__);		\
			count++;						\
			if (once)						\
				break;						\
		}								\
	}									\
	spa_list_cursor_end(cursor, link);					\
	count;									\
})

#define spa_hook_list_call(l,t,m,v,...)			spa_hook_list_do_call(l,NULL,t,m,v,false,##__VA_ARGS__)
#define spa_hook_list_call_once(l,t,m,v,...)		spa_hook_list_do_call(l,NULL,t,m,v,true,##__VA_ARGS__)

#define spa_hook_list_call_start(l,s,t,m,v,...)		spa_hook_list_do_call(l,s,t,m,v,false,##__VA_ARGS__)
#define spa_hook_list_call_once_start(l,s,t,m,v,...)	spa_hook_list_do_call(l,s,t,m,v,true,##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* SPA_HOOK_H */
