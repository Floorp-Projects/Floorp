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

#ifndef PIPEWIRE_DATA_LOOP_H
#define PIPEWIRE_DATA_LOOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/hook.h>

/** \class pw_data_loop
 *
 * PipeWire rt-loop object. This loop starts a new real-time thread that
 * is designed to run the processing graph.
 */
struct pw_data_loop;

#include <pipewire/loop.h>
#include <pipewire/properties.h>

/** Loop events, use \ref pw_data_loop_add_listener to add a listener */
struct pw_data_loop_events {
#define PW_VERSION_DATA_LOOP_EVENTS		0
	uint32_t version;
	/** The loop is destroyed */
	void (*destroy) (void *data);
};

/** Make a new loop. */
struct pw_data_loop *
pw_data_loop_new(const struct spa_dict *props);

/** Add an event listener to loop */
void pw_data_loop_add_listener(struct pw_data_loop *loop,
			       struct spa_hook *listener,
			       const struct pw_data_loop_events *events,
			       void *data);

/** wait for activity on the loop up to \a timeout milliseconds.
 * Should be called from the loop function */
int pw_data_loop_wait(struct pw_data_loop *loop, int timeout);

/** make sure the thread will exit. Can be called from a loop callback */
void pw_data_loop_exit(struct pw_data_loop *loop);

/** Get the loop implementation of this data loop */
struct pw_loop *
pw_data_loop_get_loop(struct pw_data_loop *loop);

/** Destroy the loop */
void pw_data_loop_destroy(struct pw_data_loop *loop);

/** Start the processing thread */
int pw_data_loop_start(struct pw_data_loop *loop);

/** Stop the processing thread */
int pw_data_loop_stop(struct pw_data_loop *loop);

/** Check if the current thread is the processing thread */
bool pw_data_loop_in_thread(struct pw_data_loop *loop);

/** invoke func in the context of the thread or in the caller thread when
 * the loop is not running. Since 0.3.3 */
int pw_data_loop_invoke(struct pw_data_loop *loop,
		spa_invoke_func_t func, uint32_t seq, const void *data, size_t size,
		bool block, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_DATA_LOOP_H */
