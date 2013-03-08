/**
 * Copyright (c) 2013 Mozilla Foundation and Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#ifndef __WEBVTT_NODE_INTERNAL_H__
# define __WEBVTT_NODE_INTERNAL_H__
# include <webvtt/node.h>

/**
 * Routines for creating nodes.
 */
WEBVTT_INTERN webvtt_status webvtt_create_node( webvtt_node **node, webvtt_node_kind kind, webvtt_node *parent );
WEBVTT_INTERN webvtt_status webvtt_create_internal_node( webvtt_node **node, webvtt_node *parent, webvtt_node_kind kind, webvtt_stringlist *css_classes, webvtt_string *annotation );
/**
 * We probably shouldn't have a 'head node' type.
 * We should just return a list of node trees...
 */
WEBVTT_INTERN webvtt_status webvtt_create_head_node( webvtt_node **node );
WEBVTT_INTERN webvtt_status webvtt_create_timestamp_node( webvtt_node **node, webvtt_node *parent, webvtt_timestamp time_stamp );
WEBVTT_INTERN webvtt_status webvtt_create_text_node( webvtt_node **node, webvtt_node *parent, webvtt_string *text );

/**
 * Attaches a node to the internal node list of another node.
 */
WEBVTT_INTERN webvtt_status webvtt_attach_node( webvtt_node *parent, webvtt_node *to_attach );

#endif
