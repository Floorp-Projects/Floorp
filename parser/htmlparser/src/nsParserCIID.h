/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsParserCIID_h__
#define nsParserCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

// XXX Should be _CID and not _IID
#define NS_PARSER_IID      \
  {0x2ce606b0, 0xbee6,  0x11d1,  \
  {0xaa, 0xd9, 0x00,    0x80, 0x5f, 0x8a, 0x3e, 0x14}}

// XXX: This object should not be exposed outside of the parser.
//      Remove when CNavDTD subclasses do not need access
#define NS_PARSER_NODE_IID      \
  {0x9039c670, 0x2717,  0x11d2,  \
  {0x92, 0x46, 0x00,    0x80, 0x5f, 0x8a, 0x7a, 0xb6}}

// {E6FD9941-899D-11d2-8EAE-00805F29F370}
#define NS_WELLFORMEDDTD_CID \
{ 0xe6fd9941, 0x899d, 0x11d2, { 0x8e, 0xae, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }

// {a6cf9107-15b3-11d2-932e-00805f8add32}
#define NS_CNAVDTD_CID \
{ 0xa6cf9107, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// Class IID for the logging sink
#define NS_LOGGING_SINK_CID \
 {0xa6cf9060, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// {a6cf910e-15b3-11d2-932e-00805f8add32}
#define NS_XIF_DTD_CID \
{ 0xa6cf910e, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// {CCF5BED0-1AF8-11d4-812B-0010A4E0C706}
#define NS_COTHER_DTD_CID \
{ 0xccf5bed0, 0x1af8, 0x11d4, { 0x81, 0x2b, 0x0, 0x10, 0xa4, 0xe0, 0xc7, 0x6 } }

// {4611d482-960a-11d4-8eb0-b617661b6f7c}
#define NS_CTRANSITIONAL_DTD_CID \
{ 0x4611d482, 0x960a, 0x11d4, { 0x8e, 0xb0, 0xb6, 0x17, 0x66, 0x1b, 0x6f, 0x7c } }

// {8323FAD0-2102-11d4-8142-000064657374}
#define NS_VIEWSOURCE_DTD_CID \
{ 0x8323fad0, 0x2102, 0x11d4, { 0x81, 0x42, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

// {8323FAD1-2102-11d4-8142-000064657374}
#define NS_CRTF_DTD_CID \
{ 0x8323fad1, 0x2102, 0x11d4, { 0x81, 0x42, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }


// {a6cf910f-15b3-11d2-932e-00805f8add32}
#define NS_HTMLCONTENTSINKSTREAM_CID \
{ 0xa6cf910f, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// {a6cf9110-15b3-11d2-932e-00805f8add32}
#define NS_HTMLTOTXTSINKSTREAM_CID \
{ 0xa6cf9110, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// {a6cf9112-15b3-11d2-932e-00805f8add32}
#define NS_PARSERSERVICE_CID \
{ 0xa6cf9112, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x0, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

#endif
