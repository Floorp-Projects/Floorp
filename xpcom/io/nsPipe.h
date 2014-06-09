/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPipe_h__
#define nsPipe_h__

#define NS_PIPE_CONTRACTID \
  "@mozilla.org/pipe;1"
#define NS_PIPE_CID \
{ /* e4a0ee4e-0775-457b-9118-b3ae97a7c758 */ \
  0xe4a0ee4e,                                \
  0x0775,                                    \
  0x457b,                                    \
  {0x91,0x18,0xb3,0xae,0x97,0xa7,0xc7,0x58}  \
}

// Generic factory constructor for the nsPipe class
nsresult
nsPipeConstructor(nsISupports* outer, REFNSIID iid, void** result);

#endif  // !defined(nsPipe_h__)
