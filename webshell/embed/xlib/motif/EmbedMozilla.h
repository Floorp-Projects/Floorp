/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef __XmEmbedMozilla_h__
#define __XmEmbedMozilla_h__

#ifdef	__cplusplus
extern "C" {
#endif

#include <Xm/Xm.h>

extern WidgetClass xmEmbedMozillaClass;
typedef struct _XmEmbedMozillaClassRec *XmEmbedMozillaClass;
typedef struct _XmEmbedMozillaRec      *XmEmbedMozilla;


extern Widget
XmCreateEmbedMozilla		(Widget		parent,
                                 Window     window,
                                 String		name,
                                 Arg *		args,
                                 Cardinal	num_args);

#ifdef	__cplusplus
}
#endif

#endif /* __XmEmbedMozilla_h__ */
