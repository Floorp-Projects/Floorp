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

#ifndef	_RDF_XMLSS_H_
#define	_RDF_XMLSS_H_


#include "rdf-int.h"
#include "xmlparse.h"



/* xmlss.c data structures */




/* xmlss.c function prototypes */

XP_BEGIN_PROTOS

int		parseNextXMLCSSBlob (NET_StreamClass *stream, char* blob, int32 size);
void		parseNextXMLCSSElement (StyleSheet f, char* ele);
size_t		countSSNumTags (char* ele);
size_t		countTagStackSize (char* ele);
char*		getNextSSTag (char* ele, size_t* n);

XP_END_PROTOS

#endif
