// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LIB_JXL_BASE_OS_MACROS_H_
#define LIB_JXL_BASE_OS_MACROS_H_

// Defines the JXL_OS_* macros.

#if defined(_WIN32) || defined(_WIN64)
#define JXL_OS_WIN 1
#else
#define JXL_OS_WIN 0
#endif

#ifdef __linux__
#define JXL_OS_LINUX 1
#else
#define JXL_OS_LINUX 0
#endif

#ifdef __MACH__
#define JXL_OS_MAC 1
#else
#define JXL_OS_MAC 0
#endif

#ifdef __FreeBSD__
#define JXL_OS_FREEBSD 1
#else
#define JXL_OS_FREEBSD 0
#endif

#ifdef __HAIKU__
#define JXL_OS_HAIKU 1
#else
#define JXL_OS_HAIKU 0
#endif

#endif  // LIB_JXL_BASE_OS_MACROS_H_
