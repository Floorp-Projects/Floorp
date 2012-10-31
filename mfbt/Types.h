/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* mfbt foundational types and macros. */

#ifndef mozilla_Types_h_
#define mozilla_Types_h_

/*
 * This header must be valid C and C++, includable by code embedding either
 * SpiderMonkey or Gecko.
 */

/*
 * Expose all the integer types defined in C99's <stdint.h> (and the integer
 * limit and constant macros, if compiling C code or if compiling C++ code and
 * the right __STDC_*_MACRO has been defined for each).  These are all usable
 * throughout mfbt code, and throughout Mozilla code more generally.
 */
#include "mozilla/StandardInteger.h"

/* Also expose size_t. */
#include <stddef.h>

/* Implement compiler and linker macros needed for APIs. */

/*
 * MOZ_EXPORT_API is used to declare and define a method which is externally
 * visible to users of the current library.  It encapsulates various decorations
 * needed to properly export the method's symbol.  MOZ_EXPORT_DATA serves the
 * same purpose for data.
 *
 *   api.h:
 *     extern MOZ_EXPORT_API(int) MeaningOfLife(void);
 *     extern MOZ_EXPORT_DATA(int) LuggageCombination;
 *
 *   api.c:
 *     MOZ_EXPORT_API(int) MeaningOfLife(void) { return 42; }
 *     MOZ_EXPORT_DATA(int) LuggageCombination = 12345;
 *
 * If you are merely sharing a method across files, just use plain |extern|.
 * These macros are designed for use by library interfaces -- not for normal
 * methods or data used cross-file.
 */
#if defined(WIN32) || defined(XP_OS2)
#  define MOZ_EXPORT_DIRECTIVE  __declspec(dllexport)
#else /* Unix */
#  ifdef HAVE_VISIBILITY_ATTRIBUTE
#    define MOZ_EXTERNAL_VIS       __attribute__((visibility("default")))
#  elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#    define MOZ_EXTERNAL_VIS      __global
#  else
#    define MOZ_EXTERNAL_VIS
#  endif
#  define MOZ_EXPORT_DIRECTIVE   MOZ_EXTERNAL_VIS
#endif

#define MOZ_EXPORT_API(type)    MOZ_EXPORT_DIRECTIVE type
#define MOZ_EXPORT_DATA(type)   MOZ_EXPORT_DIRECTIVE type

/*
 * Whereas implementers use MOZ_EXPORT_API and MOZ_EXPORT_DATA to declare and
 * define library symbols, users use MOZ_IMPORT_API and MOZ_IMPORT_DATA to
 * access them.  Most often the implementer of the library will expose an API
 * macro which expands to either the export or import version of the macro,
 * depending upon the compilation mode.
 */
#ifdef _WIN32
#  if defined(__MWERKS__)
#    define MOZ_IMPORT_API_DIRECTIVE /* nothing */
#  else
#    define MOZ_IMPORT_API_DIRECTIVE __declspec(dllimport)
#  endif
#elif defined(XP_OS2)
#  define MOZ_IMPORT_API_DIRECTIVE  __declspec(dllimport)
#else
#  define MOZ_IMPORT_API_DIRECTIVE MOZ_EXPORT_DIRECTIVE
#endif

#define MOZ_IMPORT_API(x)    MOZ_IMPORT_API_DIRECTIVE x

#if defined(_WIN32) && !defined(__MWERKS__)
#  define MOZ_IMPORT_DATA_DIRECTIVE __declspec(dllimport)
#elif defined(XP_OS2)
#  define MOZ_IMPORT_DATA_DIRECTIVE __declspec(dllimport)
#else
#  define MOZ_IMPORT_DATA_DIRECTIVE MOZ_EXPORT_DIRECTIVE
#endif

#define MOZ_IMPORT_DATA(x)    MOZ_IMPORT_DATA_DIRECTIVE x

/*
 * Consistent with the above comment, the MFBT_API and MFBT_DATA macros expose
 * export mfbt declarations when building mfbt, and they expose import mfbt
 * declarations when using mfbt.
 */
#if defined(IMPL_MFBT)
#  define MFBT_API     MOZ_EXPORT_DIRECTIVE
#  define MFBT_DATA    MOZ_EXPORT_DIRECTIVE
#else
  /*
   * On linux mozglue is linked in the program and we link libxul.so with
   * -z,defs. Normally that causes the linker to reject undefined references in
   * libxul.so, but as a loophole it allows undefined references to weak
   * symbols. We add the weak attribute to the import version of the MFBT API
   * macros to exploit this.
   */
#  if defined(MOZ_GLUE_IN_PROGRAM)
#    define MFBT_API   __attribute__((weak)) MOZ_IMPORT_API_DIRECTIVE
#    define MFBT_DATA  __attribute__((weak)) MOZ_IMPORT_DATA_DIRECTIVE
#  else
#    define MFBT_API   MOZ_IMPORT_API_DIRECTIVE
#    define MFBT_DATA  MOZ_IMPORT_DATA_DIRECTIVE
#  endif
#endif

/*
 * C symbols in C++ code must be declared immediately within |extern "C"|
 * blocks.  However, in C code, they need not be declared specially.  This
 * difference is abstracted behind the MOZ_BEGIN_EXTERN_C and MOZ_END_EXTERN_C
 * macros, so that the user need not know whether he is being used in C or C++
 * code.
 *
 *   MOZ_BEGIN_EXTERN_C
 *
 *   extern MOZ_EXPORT_API(int) MostRandomNumber(void);
 *   ...other declarations...
 *
 *   MOZ_END_EXTERN_C
 *
 * This said, it is preferable to just use |extern "C"| in C++ header files for
 * its greater clarity.
 */
#ifdef __cplusplus
#  define MOZ_BEGIN_EXTERN_C    extern "C" {
#  define MOZ_END_EXTERN_C      }
#else
#  define MOZ_BEGIN_EXTERN_C
#  define MOZ_END_EXTERN_C
#endif

#endif  /* mozilla_Types_h_ */
