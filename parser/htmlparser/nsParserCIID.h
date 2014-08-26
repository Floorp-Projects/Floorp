/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsParserCIID_h__
#define nsParserCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

// {2ce606b0-bee6-11d1-aad9-00805f8a3e14}
#define NS_PARSER_CID      \
{ 0x2ce606b0, 0xbee6, 0x11d1, { 0xaa, 0xd9, 0x0, 0x80, 0x5f, 0x8a, 0x3e, 0x14 } }

// XXX: This object should not be exposed outside of the parser.
//      Remove when CNavDTD subclasses do not need access
#define NS_PARSER_NODE_IID      \
  {0x9039c670, 0x2717,  0x11d2,  \
  {0x92, 0x46, 0x00,    0x80, 0x5f, 0x8a, 0x7a, 0xb6}}

// {a6cf9107-15b3-11d2-932e-00805f8add32}
#define NS_CNAVDTD_CID \
{ 0xa6cf9107, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// {FFF4FBE9-528A-4b37-819D-FC18F3A401A7}
#define NS_EXPAT_DRIVER_CID \
{ 0xfff4fbe9, 0x528a, 0x4b37, { 0x81, 0x9d, 0xfc, 0x18, 0xf3, 0xa4, 0x1, 0xa7 } }

// {a6cf910f-15b3-11d2-932e-00805f8add32}
#define NS_HTMLCONTENTSINKSTREAM_CID \
{ 0xa6cf910f, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// {a6cf9112-15b3-11d2-932e-00805f8add32}
#define NS_PARSERSERVICE_CID \
{ 0xa6cf9112, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

#endif
