/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGtkUtils_h__
#define nsGtkUtils_h__

#include <glib.h>

// Some gobject functions expect functions for gpointer arguments.
// gpointer is void* but C++ doesn't like casting functions to void*.
template <class T>
static inline gpointer FuncToGpointer(T aFunction) {
  return reinterpret_cast<gpointer>(
      reinterpret_cast<uintptr_t>
      // This cast just provides a warning if T is not a function.
      (reinterpret_cast<void (*)()>(aFunction)));
}

// Type-safe alternative to glib's g_clear_pointer.
//
// Using `g_clear_pointer` itself causes UBSan to report undefined
// behavior. The function-based definition of `g_clear_pointer` (as
// opposed to the older preprocessor macro) treats the `destroy`
// function as a `void (*)(void *)`, but the actual destroy functions
// that are used (say `wl_buffer_destroy`) usually have more specific
// pointer types.
//
// C++ draft n4901 [expr.call] para 6:
//
//     Calling a function through an expression whose function type E
//     is different from the function type F of the called function’s
//     definition results in undefined behavior unless the type
//     “pointer to F” can be converted to the type “pointer to E” via
//     a function pointer conversion (7.3.14).
//
// §7.3.14 only talks about converting between noexcept and ordinary
// function pointers.
template <class T>
static inline void MozClearPointer(T*& pointer, void (*destroy)(T*)) {
  T* hold = pointer;
  pointer = nullptr;
  if (hold) {
    destroy(hold);
  }
}

template <class T>
static inline void MozClearHandleID(T& handle, gboolean (*destroy)(T)) {
  if (handle) {
    destroy(handle);
    handle = 0;
  }
}

#endif  // nsGtkUtils_h__
