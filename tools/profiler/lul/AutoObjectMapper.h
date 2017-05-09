/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AutoObjectMapper_h
#define AutoObjectMapper_h

#include <string>

#include "mozilla/Attributes.h"
#include "PlatformMacros.h"

// A (nearly-) RAII class that maps an object in and then unmaps it on
// destruction.  This base class version uses the "normal" POSIX
// functions: open, fstat, close, mmap, munmap.

class MOZ_STACK_CLASS AutoObjectMapperPOSIX {
public:
  // The constructor does not attempt to map the file, because that
  // might fail.  Instead, once the object has been constructed,
  // call Map() to attempt the mapping.  There is no corresponding
  // Unmap() since the unmapping is done in the destructor.  Failure
  // messages are sent to |aLog|.
  explicit AutoObjectMapperPOSIX(void(*aLog)(const char*));

  // Unmap the file on destruction of this object.
  ~AutoObjectMapperPOSIX();

  // Map |fileName| into the address space and return the mapping
  // extents.  If the file is zero sized this will fail.  The file is
  // mapped read-only and private.  Returns true iff the mapping
  // succeeded, in which case *start and *length hold its extent.
  // Once a call to Map succeeds, all subsequent calls to it will
  // fail.
  bool Map(/*OUT*/void** start, /*OUT*/size_t* length, std::string fileName);

protected:
  // If we are currently holding a mapped object, these record the
  // mapped address range.
  void*  mImage;
  size_t mSize;

  // A logging sink, for complaining about mapping failures.
  void (*mLog)(const char*);

private:
  // Are we currently holding a mapped object?  This is private to
  // the base class.  Derived classes need to have their own way to
  // track whether they are holding a mapped object.
  bool mIsMapped;

  // Disable copying and assignment.
  AutoObjectMapperPOSIX(const AutoObjectMapperPOSIX&);
  AutoObjectMapperPOSIX& operator=(const AutoObjectMapperPOSIX&);
  // Disable heap allocation of this class.
  void* operator new(size_t);
  void* operator new[](size_t);
  void  operator delete(void*);
  void  operator delete[](void*);
};

#if defined(GP_OS_android)
// This is a variant of AutoObjectMapperPOSIX suitable for use in
// conjunction with faulty.lib on Android.  How it behaves depends on
// the name of the file to be mapped.  There are three possible cases:
//
// (1) /foo/bar/xyzzy/blah.apk!/libwurble.so
//     We hand it as-is to faulty.lib and let it fish the relevant
//     bits out of the APK.
//
// (2) libmozglue.so
//     This is part of the Fennec installation, but is not in the
//     APK.  Instead we have to figure out the installation path
//     and look for it there.  Because of faulty.lib limitations,
//     we have to use regular open/mmap instead of faulty.lib.
//
// (3) libanythingelse.so
//     faulty.lib assumes this is a system library, and prepends
//     "/system/lib/" to the path.  So as in (1), we can give it
//     as-is to faulty.lib.
//
// Hence (1) and (3) require special-casing here.  Case (2) simply
// hands the problem to the parent class.

class MOZ_STACK_CLASS AutoObjectMapperFaultyLib : public AutoObjectMapperPOSIX {
public:
  AutoObjectMapperFaultyLib(void(*aLog)(const char*));

  ~AutoObjectMapperFaultyLib();

  bool Map(/*OUT*/void** start, /*OUT*/size_t* length, std::string fileName);

private:
  // faulty.lib requires us to maintain an abstract handle that can be
  // used later to unmap the area.  If this is non-NULL, it is assumed
  // that unmapping is to be done by faulty.lib.  Otherwise it goes
  // via the normal mechanism.
  void* mHdl;

  // Disable copying and assignment.
  AutoObjectMapperFaultyLib(const AutoObjectMapperFaultyLib&);
  AutoObjectMapperFaultyLib& operator=(const AutoObjectMapperFaultyLib&);
  // Disable heap allocation of this class.
  void* operator new(size_t);
  void* operator new[](size_t);
  void  operator delete(void*);
  void  operator delete[](void*);
};

#endif // defined(GP_OS_android)

#endif // AutoObjectMapper_h
