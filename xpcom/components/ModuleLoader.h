/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
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
  virtual const Module* LoadModule(mozilla::FileLocation &aFile) = 0;
};
NS_DEFINE_STATIC_IID_ACCESSOR(ModuleLoader, MOZILLA_MODULELOADER_PSEUDO_IID)

} // namespace mozilla

#endif // mozilla_ModuleLoader_h
