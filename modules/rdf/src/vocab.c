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

/* 
   This file implements a standard vocabulary for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "vocabint.h"
#include "bmk2mcf.h"


	/* globals */
RDF_WDVocab gWebData = NULL;
RDF_NCVocab gNavCenter = NULL;
RDF_CoreVocab gCoreVocab = NULL;
size_t gCoreVocabSize = 0;
RDF_Resource* gAllVocab;

	/* externs */
extern char* gLocalStoreURL;
extern char* profileDirURL;
RDF gCoreDB = 0;



void
createVocabs ()
{
  size_t n = 0;
  size_t m = 0;
  gAllVocab = getMem((gCoreVocabSize = 
		      sizeof(RDF_CoreVocabStruct)+sizeof(RDF_NCVocabStruct)+sizeof(RDF_WDVocabStruct)));
  gCoreDB = (RDF)getMem(sizeof(struct RDF_DBStruct));

  createCoreVocab();
  createNavCenterVocab();
  createWebDataVocab();
  while (n < (sizeof(RDF_CoreVocabStruct)/sizeof(RDF_Resource))) {*(gAllVocab + m++) = *((RDF_Resource*)gCoreVocab + n++);}

  n = 0;
  while (n < (sizeof(RDF_NCVocabStruct)/sizeof(RDF_Resource))) {*(gAllVocab + m++) = *((RDF_Resource*)gNavCenter + n++);}

  n = 0;
  while (n < (sizeof(RDF_WDVocabStruct)/sizeof(RDF_Resource))) {*(gAllVocab + m++) = *((RDF_Resource*)gWebData + n++);}

}



void
createCoreVocab ()
{
  gCoreVocab = (RDF_CoreVocab) getMem(sizeof(RDF_CoreVocabStruct));
  gCoreVocab->RDF_parent = RDF_GetResource(gCoreDB, "parent", 1);
  gCoreVocab->RDF_name = RDF_GetResource(gCoreDB, "name", 1);
  gCoreVocab->RDF_instanceOf = RDF_GetResource(gCoreDB, "instanceOf", 1);
  gCoreVocab->RDF_subClassOf = RDF_GetResource(gCoreDB, "subClassOf", 1);
  gCoreVocab->RDF_Class = RDF_GetResource(gCoreDB, "Class", 1);
  gCoreVocab->RDF_PropertyType = RDF_GetResource(gCoreDB, "PropertyType", 1);
  gCoreVocab->RDF_slotsHere = RDF_GetResource(gCoreDB, "slotsHere", 1);
  gCoreVocab->RDF_slotsIn = RDF_GetResource(gCoreDB, "slotsIn", 1);
  gCoreVocab->RDF_domain = RDF_GetResource(gCoreDB, "domain", 1);
  gCoreVocab->RDF_range = RDF_GetResource(gCoreDB, "range", 1);
  gCoreVocab->RDF_StringType = RDF_GetResource(gCoreDB, "String", 1);
  gCoreVocab->RDF_IntType = RDF_GetResource(gCoreDB, "Int", 1);
  gCoreVocab->RDF_equals = RDF_GetResource(gCoreDB, "equals", 1);
  gCoreVocab->RDF_lessThan = RDF_GetResource(gCoreDB, "lessThan", 1);
  gCoreVocab->RDF_greaterThan = RDF_GetResource(gCoreDB, "greaterThan", 1);
  gCoreVocab->RDF_lessThanOrEqual = RDF_GetResource(gCoreDB, "lessThanOrEqualTo", 1);
  gCoreVocab->RDF_greaterThanOrEqual = RDF_GetResource(gCoreDB, "greaterThanOrEqualTo", 1);
  gCoreVocab->RDF_stringEquals = RDF_GetResource(gCoreDB, "stringEquals", 1);
  gCoreVocab->RDF_substring = RDF_GetResource(gCoreDB, "substring", 1);
  gCoreVocab->RDF_child = RDF_GetResource(gCoreDB, "child", 1);
  gCoreVocab->RDF_content = RDF_GetResource(gCoreDB, "content", 1);
  gCoreVocab->RDF_summary = RDF_GetResource(gCoreDB, "summary", 1);
  gCoreVocab->RDF_comment = RDF_GetResource(gCoreDB, "comment", 1);
  
}



void
createNavCenterVocab () {
  gNavCenter = (RDF_NCVocab) getMem(sizeof(RDF_NCVocabStruct));
  gNavCenter->RDF_overview = RDF_GetResource(gCoreDB, "overview", 1);
  gNavCenter->RDF_Trash = createContainer("Trash");
  gNavCenter->RDF_Clipboard = createContainer("Clipboard");
  gNavCenter->RDF_Top = createContainer("NC:NavCenter"); 
  setResourceType(gNavCenter->RDF_Top, RDF_RT);
  gNavCenter->RDF_Search = createContainer("NC:Search");
  gNavCenter->RDF_Sitemaps = createContainer("NC:Sitemaps");
  gNavCenter->RDF_BreadCrumbCategory = createContainer("BreadCrumbs");
  gNavCenter->RDF_BookmarkFolderCategory = createContainer("NC:Bookmarks");
  gNavCenter->RDF_NewBookmarkFolderCategory = RDF_GetResource(gCoreDB, "NewBookmarks", true);
  gNavCenter->RDF_History =  createContainer("NC:History");
  gNavCenter->RDF_HistoryBySite = createContainer("NC:HistoryBySite");
  gNavCenter->RDF_HistoryByDate = createContainer("NC:HistoryByDate");
  setResourceType(gNavCenter->RDF_History, HISTORY_RT);
  setResourceType(gNavCenter->RDF_HistoryBySite, HISTORY_RT);
  setResourceType(gNavCenter->RDF_HistoryByDate, HISTORY_RT);
  gNavCenter->RDF_bookmarkAddDate  = newResource("bookmarkAddDate", RDF_ADDED_ON_STR);
  gNavCenter->RDF_PersonalToolbarFolderCategory = RDF_GetResource(gCoreDB, "PersonalToolbarCat", true);
  gNavCenter->RDF_Column = RDF_GetResource(gCoreDB, "Column", true);
  gNavCenter->RDF_ColumnResource = RDF_GetResource(gCoreDB, "ColumnResource", true);
  gNavCenter->RDF_ColumnWidth = RDF_GetResource(gCoreDB, "ColumnWidth", true);
  gNavCenter->RDF_ColumnIconURL = RDF_GetResource(gCoreDB, "ColumnIconURL", true);
  gNavCenter->RDF_ColumnDataType = RDF_GetResource(gCoreDB, "ColumnDataType", true);
  gNavCenter->RDF_smallIcon = newResource("smallIcon", RDF_ICON_URL_STR);
  gNavCenter->RDF_largeIcon  = newResource("largeIcon", RDF_LARGE_ICON_URL_STR);
  gNavCenter->RDF_HTMLURL = newResource("htmlURL", RDF_HTML_URL_STR);
  gNavCenter->RDF_HTMLHeight = newResource("htmlHeight", RDF_HTML_HEIGHT_STR);
  gNavCenter->RDF_LocalFiles = RDF_GetResource(gCoreDB, "NC:LocalFiles", true);
  gNavCenter->RDF_Appletalk = createContainer("at:");
  setResourceType(gNavCenter->RDF_Appletalk, ATALKVIRTUAL_RT);
  gNavCenter->RDF_Mail = RDF_GetResource(gCoreDB, "NC:Mail", true);
  gNavCenter->RDF_Guide = RDF_GetResource(gCoreDB, "NC:Guide", true);
  gNavCenter->RDF_Password = RDF_GetResource(gCoreDB, "password", true);
  gNavCenter->RDF_SBProviders = RDF_GetResource(gCoreDB, "NC:SmartBrowsingProviders", true);
  gNavCenter->RDF_WorkspacePos = RDF_GetResource(gCoreDB, "workspacePos", true);
  gNavCenter->RDF_Locks = RDF_GetResource(gCoreDB, "locks", true);
  gNavCenter->RDF_AddLock = RDF_GetResource(gCoreDB, "addLock", true);
  gNavCenter->RDF_DeleteLock = RDF_GetResource(gCoreDB, "deleteLock", true);
  gNavCenter->RDF_IconLock = RDF_GetResource(gCoreDB, "iconLock", true);
  gNavCenter->RDF_NameLock = RDF_GetResource(gCoreDB, "nameLock", true);
  gNavCenter->RDF_CopyLock = RDF_GetResource(gCoreDB, "copyLock", true);
  gNavCenter->RDF_MoveLock = RDF_GetResource(gCoreDB, "moveLock", true);
  gNavCenter->RDF_WorkspacePosLock = RDF_GetResource(gCoreDB, "workspacePosLock", true);
  gNavCenter->RDF_DefaultSelectedView = RDF_GetResource(gCoreDB, "selectedView", true);
  gNavCenter->RDF_AutoOpen = RDF_GetResource(gCoreDB, "autoOpen", true);
  gNavCenter->RDF_resultType = RDF_GetResource (gCoreDB, "resultType", true);
  gNavCenter->RDF_HTMLType = RDF_GetResource (gCoreDB, "HTMLPage", true);
  gNavCenter->RDF_URLShortcut = RDF_GetResource(gCoreDB, "URLShortcut", true);
  gNavCenter->RDF_Cookies = createContainer("NC:Cookies");

  /* Commands */
  
  gNavCenter->RDF_Command = RDF_GetResource (gCoreDB, "Command", true);
  gNavCenter->RDF_Command_Launch = RDF_GetResource(gCoreDB, "Command:Launch", true);
  gNavCenter->RDF_Command_Refresh = RDF_GetResource(gCoreDB, "Command:Refresh", true);

  /* NavCenter appearance styles */

  gNavCenter->treeFGColor = newResource("treeFGColor", RDF_FOREGROUND_COLOR_STR);
  gNavCenter->treeBGColor = newResource("treeBGColor", RDF_BACKGROUND_COLOR_STR);
  gNavCenter->treeBGURL = newResource("treeBGURL", RDF_BACKGROUND_IMAGE_STR);
  gNavCenter->showTreeConnections = newResource("showTreeConnections", RDF_SHOW_TREE_CONNECTIONS_STR);
  gNavCenter->treeConnectionFGColor = newResource("treeConnectionFGColor", RDF_CONNECTION_FG_COLOR_STR);
  gNavCenter->treeOpenTriggerIconURL = newResource("treeOpenTriggerIconURL", RDF_OPEN_TRIGGER_IMAGE_STR);
  gNavCenter->treeClosedTriggerIconURL = newResource("treeClosedTriggerIconURL", RDF_CLOSED_TRIGGER_IMAGE_STR);
  gNavCenter->selectionFGColor = newResource("selectionFGColor", RDF_FOREGROUND_COLOR_STR);
  gNavCenter->selectionBGColor = newResource("selectionBGColor", RDF_BACKGROUND_COLOR_STR);
  gNavCenter->columnHeaderFGColor = newResource("columnHeaderFGColor", RDF_FOREGROUND_COLOR_STR);
  gNavCenter->columnHeaderBGColor = newResource("columnHeaderBGColor", RDF_BACKGROUND_COLOR_STR);
  gNavCenter->columnHeaderBGURL = newResource("columnHeaderBGURL", RDF_BACKGROUND_IMAGE_STR);
  gNavCenter->showColumnHeaders = newResource("showColumnHeaders", RDF_SHOW_HEADERS_STR);
  gNavCenter->showColumnHeaderDividers = newResource("showColumnHeaderDividers", RDF_SHOW_HEADER_DIVIDERS_STR);
  gNavCenter->sortColumnFGColor = newResource("sortColumnFGColor", RDF_SORT_COLUMN_FG_COLOR_STR);
  gNavCenter->sortColumnBGColor = newResource("sortColumnBGColor", RDF_SORT_COLUMN_BG_COLOR_STR);
  gNavCenter->titleBarFGColor = newResource("titleBarFGColor", RDF_FOREGROUND_COLOR_STR);
  gNavCenter->titleBarBGColor = newResource("titleBarBGColor", RDF_BACKGROUND_COLOR_STR);
  gNavCenter->titleBarBGURL = newResource("titleBarBGURL", RDF_BACKGROUND_IMAGE_STR);
  gNavCenter->dividerColor = newResource("dividerColor", RDF_DIVIDER_COLOR_STR);
  gNavCenter->showDivider = newResource("showDivider", RDF_SHOW_COLUMN_DIVIDERS_STR);
  gNavCenter->selectedColumnHeaderFGColor = newResource("selectedColumnHeaderFGColor", RDF_SELECTED_HEADER_FG_COLOR_STR);
  gNavCenter->selectedColumnHeaderBGColor = newResource("selectedColumnHeaderBGColor", RDF_SELECTED_HEADER_BG_COLOR_STR);
  gNavCenter->showColumnHilite = newResource("showColumnHilite", RDF_SHOW_COLUMN_HILITING_STR);
  gNavCenter->triggerPlacement = newResource("triggerPlacement", RDF_TRIGGER_PLACEMENT_STR);
}



void
createWebDataVocab ()
{
  gWebData = (RDF_WDVocab) getMem(sizeof(RDF_WDVocabStruct));
  gWebData->RDF_URL =  newResource("URL", RDF_URL_STR);
  gWebData->RDF_description = newResource("description", RDF_DESCRIPTION_STR);
  gWebData->RDF_Container = RDF_GetResource (gCoreDB, "Container", true);
  gWebData->RDF_firstVisitDate = newResource("firstVisitDate", RDF_FIRST_VISIT_STR);
  gWebData->RDF_lastVisitDate = newResource("lastVisitDate", RDF_LAST_VISIT_STR);
  gWebData->RDF_numAccesses = newResource("numAccesses", RDF_NUM_ACCESSES_STR);
  gWebData->RDF_creationDate = newResource("creationDate", RDF_CREATED_ON_STR);
  gWebData->RDF_lastModifiedDate = newResource("lastModifiedDate", RDF_LAST_MOD_STR);
  gWebData->RDF_size = newResource("size", RDF_SIZE_STR);
} 



RDF_Resource
newResource(char *id, int optionalNameStrID)
{
	RDF_Resource		r, RDF_name;
	char			*optionalNameStr;

	if ((r = RDF_GetResource(gCoreDB, id, true)) != NULL)
	{
		if ((optionalNameStr = XP_GetString(optionalNameStrID)) != NULL)
		{
			/* need to have our own private "name" resource */
			RDF_name = RDF_GetResource(gCoreDB, "name", 1);

			remoteStoreAdd(gRemoteStore, r, RDF_name, copyString(optionalNameStr),
					RDF_STRING_TYPE, PR_TRUE);
		}
	}
	return(r);
}
