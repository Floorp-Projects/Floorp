/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEAKCRYPTOOVERRIDE_H
#define WEAKCRYPTOOVERRIDE_H

#include "nsIWeakCryptoOverride.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace psm {

class WeakCryptoOverride final : public nsIWeakCryptoOverride
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEAKCRYPTOOVERRIDE

  WeakCryptoOverride();

protected:
  ~WeakCryptoOverride();
};

} // psm
} // mozilla

#define NS_WEAKCRYPTOOVERRIDE_CID /* ffb06724-3c20-447c-8328-ae71513dd618 */ \
{ 0xffb06724, 0x3c20, 0x447c, \
  { 0x83, 0x28, 0xae, 0x71, 0x51, 0x3d, 0xd6, 0x18 } }

#endif
