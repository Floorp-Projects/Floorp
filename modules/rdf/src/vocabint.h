/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
