/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* This is the header file for the RDF related search functions */

void RDFGS_AddSearchIndex (RDFT db, char* string, RDF_Resource label, RDF_Resource target) ;
RDF_Cursor RDFGS_Search (RDFT db, char* searchString, RDF_Resource label);
RDF_Resource RDFGS_NextValue (RDF_Cursor c) ;
void RDFGS_DisposeCursor (RDF_Cursor c);


