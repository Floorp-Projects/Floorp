/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GRefPtr_h_
#define GRefPtr_h_

// Allows to use RefPtr<T> with various kinds of GObjects

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "mozilla/RefPtr.h"

namespace mozilla {

template <typename T>
struct GObjectRefPtrTraits {
  static void AddRef(T* aObject) { g_object_ref(aObject); }
  static void Release(T* aObject) { g_object_unref(aObject); }
};

template <>
struct RefPtrTraits<GtkWidget> : public GObjectRefPtrTraits<GtkWidget> {};

template <>
struct RefPtrTraits<GdkDragContext>
    : public GObjectRefPtrTraits<GdkDragContext> {};

}  // namespace mozilla

#endif
