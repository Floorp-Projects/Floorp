/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
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
 *  il_strm.h --- Stream converters for the image library.
 *  $Id: il_strm.h,v 3.3 1999/05/27 22:37:43 pnunn%netscape.com Exp $
 */


/******************** Identifiers for standard image types. ******************/
#define IL_UNKNOWN 0
#define IL_GIF     1
#define IL_XBM     2
#define IL_JPEG    3
#define IL_PPM     4
#define IL_PNG     5

#define IL_NOTFOUND 256


/********************** Opaque reference to MWContext. ***********************/
#ifdef IL_INTERNAL              /* If used within the Image Library. */
#define OPAQUE_CONTEXT void
#else
#define OPAQUE_CONTEXT MWContext /* The old MWContext. */
#endif /* IL_INTERNAL */



