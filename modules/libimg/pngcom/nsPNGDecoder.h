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
 *   nsPNGDecoder.cpp --- interface to png decoder
 */
#ifndef _nsPNGDec_h
#define _nsPNGDec_h

/* 573010b0-de61-11d2-802c-0060088f91a3 */
#define NS_PNGDECODER_CID \
{ 0x573010b0, 0xde61, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

/* 789d4ab0-de61-11d2-802e-0060088f91a3 */
#define NS_PNGDECODER_IID \
{ 0x789d4ab0, 0xde61, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }


static NS_DEFINE_IID(kPNGDecoderIID, NS_PNGDECODER_IID);
static NS_DEFINE_CID(kPNGDecoderCID, NS_PNGDECODER_CID);


#endif
