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

#ifndef PIPEWIRE_H
#define PIPEWIRE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/support/plugin.h>

#include <pipewire/array.h>
#include <pipewire/client.h>
#include <pipewire/context.h>
#include <pipewire/device.h>
#include <pipewire/buffers.h>
#include <pipewire/core.h>
#include <pipewire/factory.h>
#include <pipewire/keys.h>
#include <pipewire/log.h>
#include <pipewire/loop.h>
#include <pipewire/link.h>
#include <pipewire/main-loop.h>
#include <pipewire/map.h>
#include <pipewire/mem.h>
#include <pipewire/module.h>
#include <pipewire/node.h>
#include <pipewire/properties.h>
#include <pipewire/proxy.h>
#include <pipewire/permission.h>
#include <pipewire/protocol.h>
#include <pipewire/port.h>
#include <pipewire/stream.h>
#include <pipewire/filter.h>
#include <pipewire/thread-loop.h>
#include <pipewire/data-loop.h>
#include <pipewire/type.h>
#include <pipewire/utils.h>
#include <pipewire/version.h>

/** \mainpage
 *
 * \section sec_intro Introduction
 *
 * This document describes the API for the PipeWire multimedia framework.
 * The API consists of two parts:
 *
 * \li The core API to access a PipeWire instance.
 * (See \subpage page_core_api)
 * \li The implementation API and tools to build new objects and
 * modules (See \subpage page_implementation_api)
 *
 * \section sec_errors Error reporting
 *
 * Functions return either NULL with errno set or a negative int error
 * code when an error occurs. Error codes are used from the SPA plugin
 * library on which PipeWire is built.
 *
 * Some functions might return asynchronously. The error code for such
 * functions is positive and SPA_RESULT_IS_ASYNC() will return true.
 * SPA_RESULT_ASYNC_SEQ() can be used to get the unique sequence number
 * associated with the async operation.
 *
 * The object returning the async result code will have some way to
 * signal the completion of the async operation (with, for example, a
 * callback). The sequence number can be used to see which operation
 * completed.
 *
 * \section sec_logging Logging
 *
 * The 'PIPEWIRE_DEBUG' environment variable can be used to enable
 * more debugging. The format is:
 *
 *    &lt;level&gt;[:&lt;category&gt;,...]
 *
 * - &lt;level&gt;: specifies the log level:
 *   + `0`: no logging is enabled
 *   + `1`: Error logging is enabled
 *   + `2`: Warnings are enabled
 *   + `3`: Informational messages are enabled
 *   + `4`: Debug messages are enabled
 *   + `5`: Trace messages are enabled. These messages can be logged
 *		from the realtime threads.
 *
 * - &lt;category&gt;:  Specifies a string category to enable. Many categories
 *		  can be separated by commas. Current categories are:
 *   + `connection`: to log connection messages
 */

/** \class pw_pipewire
 *
 * \brief PipeWire initialization and infrastructure functions
 */
void
pw_init(int *argc, char **argv[]);

void pw_deinit(void);

bool
pw_debug_is_category_enabled(const char *name);

const char *
pw_get_application_name(void);

const char *
pw_get_prgname(void);

const char *
pw_get_user_name(void);

const char *
pw_get_host_name(void);

const char *
pw_get_client_name(void);

bool pw_in_valgrind(void);

enum pw_direction
pw_direction_reverse(enum pw_direction direction);

uint32_t pw_get_support(struct spa_support *support, uint32_t max_support);

struct spa_handle *pw_load_spa_handle(const char *lib,
		const char *factory_name,
		const struct spa_dict *info,
		uint32_t n_support,
		const struct spa_support support[]);

int pw_unload_spa_handle(struct spa_handle *handle);

#ifdef __cplusplus
}
#endif

#endif /* PIPEWIRE_H */
