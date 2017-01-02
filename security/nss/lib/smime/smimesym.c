/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern void SEC_PKCS7DecodeItem();
extern void SEC_PKCS7DestroyContentInfo();

smime_CMDExports()
{
    SEC_PKCS7DecodeItem();
    SEC_PKCS7DestroyContentInfo();
}
