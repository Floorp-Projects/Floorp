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

#include "xp.h"
#include "mkgeturl.h"
#include "mkstream.h"
#include "mkformat.h"


PUBLIC NET_StreamClass * 
NET_PlainTextConverter   (FO_Present_Types format_out,
                          void       *data_obj,
                          URL_Struct *URL_s,
                          MWContext  *window_id)
{
	NET_StreamClass * stream;
	char plaintext [] = "<plaintext>"; /* avoid writable string lossage... */

	StrAllocCopy(URL_s->content_type, TEXT_HTML);

	/* stuff for text fe */
	if(format_out == FO_VIEW_SOURCE)
	  {
		format_out = FO_PRESENT;
	  }

	stream = NET_StreamBuilder(format_out, URL_s, window_id);

	if(stream)
		(*stream->put_block)(stream, plaintext, 11);

    return stream;
}
