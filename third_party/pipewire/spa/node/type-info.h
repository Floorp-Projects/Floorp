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

#ifndef SPA_NODE_TYPES_H
#define SPA_NODE_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/utils/type-info.h>

#include <spa/node/command.h>
#include <spa/node/event.h>
#include <spa/node/io.h>

#define SPA_TYPE_INFO_IO			SPA_TYPE_INFO_ENUM_BASE "IO"
#define SPA_TYPE_INFO_IO_BASE		SPA_TYPE_INFO_IO ":"

static const struct spa_type_info spa_type_io[] = {
	{ SPA_IO_Invalid, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Invalid", NULL },
	{ SPA_IO_Buffers, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Buffers", NULL },
	{ SPA_IO_Range, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Range", NULL },
	{ SPA_IO_Clock, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Clock", NULL },
	{ SPA_IO_Latency, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Latency", NULL },
	{ SPA_IO_Control, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Control", NULL },
	{ SPA_IO_Notify, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Notify", NULL },
	{ SPA_IO_Position, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Position", NULL },
	{ SPA_IO_RateMatch, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "RateMatch", NULL },
	{ SPA_IO_Memory, SPA_TYPE_Int, SPA_TYPE_INFO_IO_BASE "Memory", NULL },
	{ 0, 0, NULL, NULL },
};

#define SPA_TYPE_INFO_NodeEvent			SPA_TYPE_INFO_EVENT_BASE "Node"
#define SPA_TYPE_INFO_NODE_EVENT_BASE		SPA_TYPE_INFO_NodeEvent ":"

static const struct spa_type_info spa_type_node_event_id[] = {
	{ SPA_NODE_EVENT_Error,		 SPA_TYPE_Int, SPA_TYPE_INFO_NODE_EVENT_BASE "Error",   NULL },
	{ SPA_NODE_EVENT_Buffering,	 SPA_TYPE_Int, SPA_TYPE_INFO_NODE_EVENT_BASE "Buffering", NULL },
	{ SPA_NODE_EVENT_RequestRefresh, SPA_TYPE_Int, SPA_TYPE_INFO_NODE_EVENT_BASE "RequestRefresh", NULL },
	{ 0, 0, NULL, NULL },
};

static const struct spa_type_info spa_type_node_event[] = {
	{ 0, SPA_TYPE_Id, SPA_TYPE_INFO_NODE_EVENT_BASE, spa_type_node_event_id },
	{ 0, 0, NULL, NULL },
};

#define SPA_TYPE_INFO_NodeCommand			SPA_TYPE_INFO_COMMAND_BASE "Node"
#define SPA_TYPE_INFO_NODE_COMMAND_BASE		SPA_TYPE_INFO_NodeCommand ":"

static const struct spa_type_info spa_type_node_command_id[] = {
	{ SPA_NODE_COMMAND_Suspend,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Suspend", NULL },
	{ SPA_NODE_COMMAND_Pause,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Pause",   NULL },
	{ SPA_NODE_COMMAND_Start,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Start",   NULL },
	{ SPA_NODE_COMMAND_Enable,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Enable",  NULL },
	{ SPA_NODE_COMMAND_Disable,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Disable", NULL },
	{ SPA_NODE_COMMAND_Flush,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Flush",   NULL },
	{ SPA_NODE_COMMAND_Drain,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Drain",   NULL },
	{ SPA_NODE_COMMAND_Marker,	SPA_TYPE_Int, SPA_TYPE_INFO_NODE_COMMAND_BASE "Marker",  NULL },
	{ 0, 0, NULL, NULL },
};

static const struct spa_type_info spa_type_node_command[] = {
	{ 0, SPA_TYPE_Id, SPA_TYPE_INFO_NODE_COMMAND_BASE, spa_type_node_command_id },
	{ 0, 0, NULL, NULL },
};

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* SPA_NODE_TYPES_H */
