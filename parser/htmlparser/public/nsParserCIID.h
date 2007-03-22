/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// Class IID for the logging sink
#define NS_LOGGING_SINK_CID \
 {0xa6cf9060, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// {8323FAD0-2102-11d4-8142-000064657374}
#define NS_VIEWSOURCE_DTD_CID \
{ 0x8323fad0, 0x2102, 0x11d4, { 0x81, 0x42, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

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
