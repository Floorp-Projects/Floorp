/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 * 
 * Contributor(s): 
 */

#ifndef __XtEmbedMozillaP_h__
#define __XtEmbedMozillaP_h__

#ifdef	__cplusplus
extern "C" {
#endif

#include "EmbedMozilla.h"
#include <X11/IntrinsicP.h>

typedef struct
{
  int something;
} XtEmbedMozillaClassPart;

typedef struct _XtEmbedMozillaClassRec
{
  CoreClassPart	            core_class;
  CompositeClassPart		    composite_class;
  ConstraintClassPart		    constraint_class;
  XtEmbedMozillaClassPart	  embed_mozilla_class;
} XtEmbedMozillaClassRec;

extern XtEmbedMozillaClassRec xtEmbedMozillaClassRec;

typedef struct 
{
  XtCallbackList            input_callback;
  Window                    embed_window;
} XtEmbedMozillaPart;

typedef struct _XtEmbedMozillaRec
{
  CorePart			            core;
  CompositePart		          composite;
  ConstraintPart		        constraint;
  XtEmbedMozillaPart		    embed_mozilla;
} XtEmbedMozillaRec;

#ifdef	__cplusplus
}
#endif

#endif /* __XtEmbedMozillaP_h__ */
