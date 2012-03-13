/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
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

/* Implementations of hash functions */

#include "mozilla/HashFunctions.h"
#include <string.h>

namespace mozilla {

MFBT_API(uint32_t)
HashBytes(const void* bytes, size_t length)
{
  uint32_t hash = 0;
  const char* b = reinterpret_cast<const char*>(bytes);

  /* Walk word by word. */
  size_t i = 0;
  for (; i < length - (length % sizeof(size_t)); i += sizeof(size_t)) {
    /* Do an explicitly unaligned load of the data. */
    size_t data;
    memcpy(&data, b + i, sizeof(size_t));

    hash = AddToHash(hash, data, sizeof(data));
  }

  /* Get the remaining bytes. */
  for (; i < length; i++) {
    hash = AddToHash(hash, b[i]);
  }

  return hash;
}

} /* namespace mozilla */
