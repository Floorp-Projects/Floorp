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
 *   nsJPGDecoder.cpp --- interface to gif decoder
 */
#ifndef _nsJPGDec_h
#define _nsJPGDec_h

/* 1b0e6b90-d68a-11d2-802e-0060088f91a3 */
#define NS_JPGDECODER_CID \
{ 0x1b0e6b90, 0xd68a, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

/* 394711e0-d68a-11d2-802e-0060088f91a3 */
#define NS_JPGDECODER_IID \
{ 0x394711e0, 0xd68a, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

static NS_DEFINE_IID(kJPGDecoderIID, NS_JPGDECODER_IID);
static NS_DEFINE_CID(kJPGDecoderCID, NS_JPGDECODER_CID);


#endif
