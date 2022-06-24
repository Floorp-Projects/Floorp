/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentProcessWidgetFactory_h
#define nsContentProcessWidgetFactory_h

#include "nsISupports.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"

#define MAKE_COMPONENT_CHOOSER(name_, parent_, content_, constructor_) \
  static already_AddRefed<nsISupports> name_() {                       \
    nsCOMPtr<nsISupports> inst;                                        \
    if (XRE_IsContentProcess()) {                                      \
      inst = constructor_(content_);                                   \
    } else {                                                           \
      inst = constructor_(parent_);                                    \
    }                                                                  \
    return inst.forget();                                              \
  }

MAKE_COMPONENT_CHOOSER(nsClipboardSelector,
                       "@mozilla.org/widget/parent/clipboard;1",
                       "@mozilla.org/widget/content/clipboard;1", do_GetService)
MAKE_COMPONENT_CHOOSER(nsColorPickerSelector,
                       "@mozilla.org/parent/colorpicker;1",
                       "@mozilla.org/content/colorpicker;1", do_CreateInstance)
MAKE_COMPONENT_CHOOSER(nsFilePickerSelector, "@mozilla.org/parent/filepicker;1",
                       "@mozilla.org/content/filepicker;1", do_CreateInstance)
MAKE_COMPONENT_CHOOSER(nsScreenManagerSelector,
                       "@mozilla.org/gfx/parent/screenmanager;1",
                       "@mozilla.org/gfx/content/screenmanager;1",
                       do_GetService)
MAKE_COMPONENT_CHOOSER(nsSoundSelector, "@mozilla.org/parent/sound;1",
                       "@mozilla.org/content/sound;1", do_GetService)
MAKE_COMPONENT_CHOOSER(nsDragServiceSelector,
                       "@mozilla.org/widget/parent/dragservice;1",
                       "@mozilla.org/widget/content/dragservice;1",
                       do_GetService)

#undef MAKE_COMPONENT_CHOOSER

#endif  // defined nsContentProcessWidgetFactory_h
