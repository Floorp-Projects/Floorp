/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Special include file for xptc*_gcc_x86_unix.cpp */

// 
// this may improve the static function calls, but may not.
//

// #define MOZ_USE_STDCALL

#ifdef MOZ_USE_STDCALL
#define ATTRIBUTE_STDCALL __attribute__ ((__stdcall__))
#else
#define ATTRIBUTE_STDCALL
#endif

#ifdef MOZ_NEED_LEADING_UNDERSCORE
#define SYMBOL_UNDERSCORE "_"
#else
#define SYMBOL_UNDERSCORE
#endif

/*
  What are those keeper functions?

  The problem: gcc doesn't know that the assembler routines call
  static functions so gcc may not emit the definition (i.e., the
  code) for these functions. In gcc 3.1 and up
    "__attribute__ ((used))" exists and solves the problem.
  For older gcc versions it's not so easy. One could use the
  -fkeep-inline-functions but that keeps a surprising number of
  functions which bloats the compiled library. It seems otherwise
  harmless, though. Alternatively, one could use
  -fno-inline-functions which works right now but might cause a
  slowdown under some circumstances. The problem with these methods
  is that they do not automatically adapt to the compiler used.

  The best solution seems to be to create dummy functions that
  reference the appropriate static functions. It's then necessary
  to "use" these functions in a way that gcc will not optimize
  away. The keeper functions use assembly code to confuse gcc.

  One drawback is that the keeper functions are externally visible
  so they shouldn't do anything harmful.

  With the right linker, one could make the keeper functions local
  so they wouldn't be visible.
 */


// gcc 3.1 and up
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
#define ATTRIBUTE_USED __attribute__ ((__used__))
#else
#define ATTRIBUTE_USED
#endif

