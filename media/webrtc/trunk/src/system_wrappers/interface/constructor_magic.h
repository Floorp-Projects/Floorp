/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * WebRtc
 * Copy from third_party/libjingle/source/talk/base/constructormagic.h
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CONSTRUCTOR_MAGIC_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CONSTRUCTOR_MAGIC_H_

#ifndef DISALLOW_ASSIGN
#define DISALLOW_ASSIGN(TypeName) \
  void operator=(const TypeName&)
#endif

#ifndef DISALLOW_COPY_AND_ASSIGN
// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName)    \
  TypeName(const TypeName&);                    \
  DISALLOW_ASSIGN(TypeName)
#endif

#ifndef DISALLOW_EVIL_CONSTRUCTORS
// Alternative, less-accurate legacy name.
#define DISALLOW_EVIL_CONSTRUCTORS(TypeName) \
  DISALLOW_COPY_AND_ASSIGN(TypeName)
#endif

#ifndef DISALLOW_IMPLICIT_CONSTRUCTORS
// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName();                                    \
  DISALLOW_EVIL_CONSTRUCTORS(TypeName)
#endif

#endif  // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_CONSTRUCTOR_MAGIC_H_
