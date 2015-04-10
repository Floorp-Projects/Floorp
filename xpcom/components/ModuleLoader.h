/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ModuleLoader_h
#define mozilla_ModuleLoader_h

#include "nsISupports.h"
#include "mozilla/Module.h"
#include "mozilla/FileLocation.h"

#define MOZILLA_MODULELOADER_PSEUDO_IID \
{ 0xD951A8CE, 0x6E9F, 0x464F, \
  { 0x8A, 0xC8, 0x14, 0x61, 0xC0, 0xD3, 0x63, 0xC8 } }

namespace mozilla {

/**
 * Module loaders are responsible for loading a component file. The static
 * component loader is special and does not use this abstract interface.
 *
 * @note Implementations of this interface should be threadsafe,
 *       methods may be called from any thread.
 */
class ModuleLoader : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_MODULELOADER_PSEUDO_IID)

  /**
   * Return the module for a specified file. The caller should cache
   * the module: the implementer should not expect for the same file
   * to be loaded multiple times. The Module object should either be
   * statically or permanently allocated; it will not be freed.
   */
  virtual const Module* LoadModule(mozilla::FileLocation& aFile) = 0;
};
NS_DEFINE_STATIC_IID_ACCESSOR(ModuleLoader, MOZILLA_MODULELOADER_PSEUDO_IID)

} // namespace mozilla

#endif // mozilla_ModuleLoader_h
