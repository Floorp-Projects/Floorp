/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http:/www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* -*- Mode: C; tab-width: 4 -*-
 *   nsGIFDecoder.cpp --- interface to gif decoder
 */
#ifndef _nsGIFDec_h
#define _nsGIFDec_h


#define NS_GIFDECODER_CID \
{ 0x0d471b70, 0xbaf5, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

/* 402fb190-bbd0-11d2-802c-0060088f91a3 */
#define NS_GIFDECODER_IID \
{ 0x402b190, 0xbbd0, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }


static NS_DEFINE_IID(kGIFDecoderIID, NS_GIFDECODER_IID);
static NS_DEFINE_CID(kGIFDecoderCID, NS_GIFDECODER_CID);


#endif
