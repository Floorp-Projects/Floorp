/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_gtk_MozContainerSurfaceLock_h
#define widget_gtk_MozContainerSurfaceLock_h

struct wl_surface;
struct _MozContainer;
typedef struct _MozContainer MozContainer;

class MozContainerSurfaceLock {
 public:
  explicit MozContainerSurfaceLock(MozContainer* aContainer);
  ~MozContainerSurfaceLock();

  // wl_surface can be nullptr if we lock hidden MozContainer.
  struct wl_surface* GetSurface();

 private:
#ifdef MOZ_WAYLAND
  MozContainer* mContainer = nullptr;
#endif
  struct wl_surface* mSurface = nullptr;
};

#endif  // widget_gtk_MozContainerSurfaceLock_h
