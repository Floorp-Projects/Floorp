/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Cross-platform lightweight thread local data wrappers

#if defined(XP_WIN)
  // This file will get included in any file that wants to add
  // a profiler mark. In order to not bring <windows.h> together
  // we could include windef.h and winbase.h which are sufficient
  // to get the prototypes for the Tls* functions.
  // # include <windef.h>
  // # include <winbase.h>
  // Unfortunately, even including these headers causes
  // us to add a bunch of ugly to our namespace. e.g #define CreateEvent CreateEventW
extern "C" {
__declspec(dllimport) void * __stdcall TlsGetValue(unsigned long);
__declspec(dllimport) int __stdcall TlsSetValue(unsigned long, void *);
__declspec(dllimport) unsigned long __stdcall TlsAlloc();
}
#else
# include <pthread.h>
# include <signal.h>
#endif

namespace mozilla {

#if defined(XP_WIN)
typedef unsigned long sig_safe_t;
#else
typedef sig_atomic_t sig_safe_t;
#endif

namespace tls {

#if defined(XP_WIN)

typedef unsigned long key;

template <typename T>
inline T* get(key mykey) {
  return (T*) TlsGetValue(mykey);
}

template <typename T>
inline bool set(key mykey, const T* value) {
  return TlsSetValue(mykey, const_cast<T*>(value));
}

inline bool create(key* mykey) {
  key newkey = TlsAlloc();
  if (newkey == (unsigned long)0xFFFFFFFF /* TLS_OUT_OF_INDEXES */) {
    return false;
  }
  *mykey = newkey;
  return true;
}

#else

typedef pthread_key_t key;

template <typename T>
inline T* get(key mykey) {
  return (T*) pthread_getspecific(mykey);
}

template <typename T>
inline bool set(key mykey, const T* value) {
  return !pthread_setspecific(mykey, value);
}

inline bool create(key* mykey) {
  return !pthread_key_create(mykey, NULL);
}

#endif

}

}
 
