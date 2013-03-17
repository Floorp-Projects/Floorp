/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSYSTEMINFO_H_
#define _NSSYSTEMINFO_H_

#include "nsHashPropertyBag.h"

class nsSystemInfo : public nsHashPropertyBag {
public:
    nsSystemInfo();

    nsresult Init();

protected:
    void SetInt32Property(const nsAString &aPropertyName,
                          const int32_t aValue);
    void SetUint64Property(const nsAString &aPropertyName,
                           const uint64_t aValue);

private:
    ~nsSystemInfo();
};

#define NS_SYSTEMINFO_CONTRACTID "@mozilla.org/system-info;1"
#define NS_SYSTEMINFO_CID \
{ 0xd962398a, 0x99e5, 0x49b2, \
{ 0x85, 0x7a, 0xc1, 0x59, 0x04, 0x9c, 0x7f, 0x6c } }

#endif /* _NSSYSTEMINFO_H_ */
