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

#ifndef __XmEmbedMozillaP_h__
#define __XmEmbedMozillaP_h__

#ifdef	__cplusplus
extern "C" {
#endif

#include "EmbedMozilla.h"
#include <Xm/ManagerP.h>

typedef struct
{
  int something;
} XmEmbedMozillaClassPart;

typedef struct _XmEmbedMozillaClassRec
{
  CoreClassPart	core_class;
  CompositeClassPart		composite_class;
  ConstraintClassPart		constraint_class;
  XmManagerClassPart		manager_class;
  XmEmbedMozillaClassPart	embed_mozilla_class;
} XmEmbedMozillaClassRec;

extern XmEmbedMozillaClassRec xmEmbedMozillaClassRec;

typedef struct 
{
  XtCallbackList                input_callback;
  Window                        embed_window;
} XmEmbedMozillaPart;

typedef struct _XmEmbedMozillaRec
{
    CorePart			core;
    CompositePart		composite;
    ConstraintPart		constraint;
    XmManagerPart		manager;
    XmEmbedMozillaPart		embed_mozilla;
} XmEmbedMozillaRec;

#ifdef	__cplusplus
}
#endif

#endif /* __XmEmbedMozillaP_h__ */
