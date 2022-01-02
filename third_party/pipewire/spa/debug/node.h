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

#ifndef SPA_DEBUG_NODE_H
#define SPA_DEBUG_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/node/node.h>
#include <spa/debug/dict.h>

#ifndef spa_debug
#define spa_debug(...)	({ fprintf(stderr, __VA_ARGS__);fputc('\n', stderr); })
#endif

static inline int spa_debug_port_info(int indent, const struct spa_port_info *info)
{
        spa_debug("%*s" "struct spa_port_info %p:", indent, "", info);
        spa_debug("%*s" " flags: \t%08" PRIx64, indent, "", info->flags);
        spa_debug("%*s" " rate: \t%d/%d", indent, "", info->rate.num, info->rate.denom);
        spa_debug("%*s" " props:", indent, "");
        if (info->props)
                spa_debug_dict(indent + 2, info->props);
        else
                spa_debug("%*s" "  none", indent, "");
        return 0;
}


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* SPA_DEBUG_NODE_H */
