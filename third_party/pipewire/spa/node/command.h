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

#ifndef SPA_COMMAND_NODE_H
#define SPA_COMMAND_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/pod/command.h>

/* object id of SPA_TYPE_COMMAND_Node */
enum spa_node_command {
	SPA_NODE_COMMAND_Suspend,
	SPA_NODE_COMMAND_Pause,
	SPA_NODE_COMMAND_Start,
	SPA_NODE_COMMAND_Enable,
	SPA_NODE_COMMAND_Disable,
	SPA_NODE_COMMAND_Flush,
	SPA_NODE_COMMAND_Drain,
	SPA_NODE_COMMAND_Marker,
};

#define SPA_NODE_COMMAND_ID(cmd)	SPA_COMMAND_ID(cmd, SPA_TYPE_COMMAND_Node)
#define SPA_NODE_COMMAND_INIT(id)	SPA_COMMAND_INIT(SPA_TYPE_COMMAND_Node, id)


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* SPA_COMMAND_NODE_H */
