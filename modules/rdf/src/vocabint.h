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

#ifndef	_RDF_VOCABINT_H_
#define	_RDF_VOCABINT_H_


#include "rdf-int.h"
#include "htrdf.h"
#include "xpgetstr.h"



/* vocab.c data structures */

extern	int	RDF_FOREGROUND_COLOR_STR, RDF_BACKGROUND_COLOR_STR, RDF_BACKGROUND_IMAGE_STR;
extern	int	RDF_SHOW_TREE_CONNECTIONS_STR, RDF_CONNECTION_FG_COLOR_STR, RDF_OPEN_TRIGGER_IMAGE_STR;
extern	int	RDF_CLOSED_TRIGGER_IMAGE_STR, RDF_SHOW_HEADERS_STR, RDF_SHOW_HEADER_DIVIDERS_STR;
extern	int	RDF_SORT_COLUMN_FG_COLOR_STR, RDF_SORT_COLUMN_BG_COLOR_STR, RDF_DIVIDER_COLOR_STR;
extern	int	RDF_SHOW_COLUMN_DIVIDERS_STR, RDF_SELECTED_HEADER_FG_COLOR_STR, RDF_SELECTED_HEADER_BG_COLOR_STR;
extern	int	RDF_SHOW_COLUMN_HILITING_STR, RDF_TRIGGER_PLACEMENT_STR, RDF_URL_STR;
extern	int	RDF_DESCRIPTION_STR, RDF_FIRST_VISIT_STR, RDF_LAST_VISIT_STR, RDF_NUM_ACCESSES_STR;
extern	int	RDF_CREATED_ON_STR, RDF_LAST_MOD_STR, RDF_SIZE_STR, RDF_ADDED_ON_STR, RDF_ICON_URL_STR;
extern	int	RDF_LARGE_ICON_URL_STR, RDF_HTML_URL_STR, RDF_HTML_HEIGHT_STR;
extern	int	RDF_CONTAINS_STR, RDF_IS_STR, RDF_IS_NOT_STR, RDF_STARTS_WITH_STR, RDF_ENDS_WITH_STR;
extern	int	RDF_FTP_NAME_STR, RDF_APPLETALK_TOP_NAME;


/* vocab.c function prototypes */

XP_BEGIN_PROTOS

void			createVocabs ();
void			createCoreVocab ();
void			createNavCenterVocab ();
void			createWebDataVocab ();
RDF_Resource		newResource(char *id, int optionalNameStrID);
char			*getResourceDefaultName(RDF_Resource r);

XP_END_PROTOS

#endif
