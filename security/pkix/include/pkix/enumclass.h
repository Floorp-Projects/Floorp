/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix__enumclass_h
#define mozilla_pkix__enumclass_h

#if defined(_MSC_VER) && (_MSC_VER < 1700)
// Microsoft added support for "enum class" in Visual C++ 2012. Before that,
// Visual C++ has supported typed enums for longer than that, but using typed
// enums results in C4480: nonstandard extension used: specifying underlying
// type for enum.
#define MOZILLA_PKIX_ENUM_CLASS  __pragma(warning(suppress: 4480)) enum
#elif defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ < 407)
// GCC before version 4.7 may crash when compiling code that static_casts a
// value of scoped typed enum type. See
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=48106.
#define MOZILLA_PKIX_ENUM_CLASS enum
#else
#define MOZILLA_PKIX_ENUM_CLASS enum class
#define MOZILLA_PKIX_ENUM_CLASS_REALLY_IS_ENUM_CLASS
#endif

#endif // mozilla_pkix__enumclass_h
