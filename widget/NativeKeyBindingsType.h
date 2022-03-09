/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NativeKeyBindingsType_h
#define mozilla_NativeKeyBindingsType_h

namespace mozilla {

enum class NativeKeyBindingsType : uint8_t {
  SingleLineEditor,  // <input type="text"> etc
  MultiLineEditor,   // <textarea>
  RichTextEditor,    // contenteditable or designMode
};

}  // namespace mozilla

#endif  // #ifndef mozilla_NativeKeyBindings_h
