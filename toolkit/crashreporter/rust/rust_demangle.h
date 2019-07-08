/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __RUST_DEMANGLE_H__
#define __RUST_DEMANGLE_H__

#ifdef __cplusplus
extern "C" {
#endif

extern char* rust_demangle(const char*);
extern void free_rust_demangled_name(char*);

#ifdef __cplusplus
}
#endif

#endif /* __RUST_DEMANGLE_H__ */
