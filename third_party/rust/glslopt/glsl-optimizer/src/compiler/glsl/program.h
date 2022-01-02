/*
 * Copyright (C) 1999-2008  Brian Paul   All Rights Reserved.
 * Copyright (C) 2009  VMware, Inc.  All Rights Reserved.
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef GLSL_PROGRAM_H
#define GLSL_PROGRAM_H

#ifdef __cplusplus
extern "C" {
#endif

struct gl_context;
struct gl_shader;
struct gl_shader_program;

extern void
_mesa_glsl_compile_shader(struct gl_context *ctx, struct gl_shader *shader,
			  bool dump_ast, bool dump_hir, bool force_recompile);

#ifdef __cplusplus
} /* extern "C" */
#endif

extern void
link_shaders(struct gl_context *ctx, struct gl_shader_program *prog);

extern void
build_program_resource_list(struct gl_context *ctx,
                            struct gl_shader_program *shProg,
                            bool add_packed_varyings_only);

extern long
parse_program_resource_name(const GLchar *name,
                            const GLchar **out_base_name_end);

#endif /* GLSL_PROGRAM_H */
