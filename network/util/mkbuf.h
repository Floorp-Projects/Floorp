/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*  mkbuf.h --- a netlib stream that buffers data before passing it along.
    This is used by libmime and libmsg to undo the effects of line-buffering,
    and to pass along larger chunks of data to various stream handlers (e.g.,
    layout.)

    Created: Jamie Zawinski <jwz@mozilla.org>,  8-Aug-98.
*/

#ifndef __MKBUF_H__
#define __MKBUF_H__

#include "ntypes.h"

XP_BEGIN_PROTOS

extern NET_StreamClass *NET_MakeRebufferingStream (NET_StreamClass *next,
                                                   URL_Struct *url,
                                                   MWContext *context);

XP_END_PROTOS

#endif /* __MKBUF_H__ */
