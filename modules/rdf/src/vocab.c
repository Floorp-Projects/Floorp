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
  gCoreVocab->RDF_notParent = RDF_GetResource(gCoreDB, "notParent", 1); 
  gCoreVocab->RDF_notInstanceOf = RDF_GetResource(gCoreDB, "notInstanceOf", 1); 
  gCoreVocab->RDF_notEquals = RDF_GetResource(gCoreDB, "notEquals", 1); 
  gCoreVocab->RDF_notStringEquals = RDF_GetResource(gCoreDB, "notStringEquals", 1); 
  gCoreVocab->RDF_notSubstring = RDF_GetResource(gCoreDB, "notSubstring", 1); 
  gCoreVocab->RDF_child = RDF_GetResource(gCoreDB, "child", 1);
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
  gNavCenter->RDF_bookmarkAddDate  = RDF_GetResource(gCoreDB, "bookmarkAddDate", true);
  gNavCenter->RDF_PersonalToolbarFolderCategory = RDF_GetResource(gCoreDB, "PersonalToolbarCat", true);
  gNavCenter->RDF_Column = RDF_GetResource(gCoreDB, "Column", true);
  gNavCenter->RDF_ColumnResource = RDF_GetResource(gCoreDB, "ColumnResource", true);
  gNavCenter->RDF_ColumnWidth = RDF_GetResource(gCoreDB, "ColumnWidth", true);
  gNavCenter->RDF_ColumnIconURL = RDF_GetResource(gCoreDB, "ColumnIconURL", true);
  gNavCenter->RDF_ColumnDataType = RDF_GetResource(gCoreDB, "ColumnDataType", true);
  gNavCenter->RDF_smallIcon = RDF_GetResource(gCoreDB, "smallIcon", true);
  gNavCenter->RDF_largeIcon  = RDF_GetResource(gCoreDB, "largeIcon", true);
  gNavCenter->RDF_HTMLURL = RDF_GetResource(gCoreDB, "htmlURL", true);
  gNavCenter->RDF_LocalFiles = RDF_GetResource(gCoreDB, "NC:LocalFiles", true);
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
  gNavCenter->RDF_Command = RDF_GetResource (gCoreDB, "Command", true);
}



void
createWebDataVocab ()
{
  gWebData = (RDF_WDVocab) getMem(sizeof(RDF_WDVocabStruct));
  gWebData->RDF_URL =  RDF_GetResource(gCoreDB, "URL", true);
  gWebData->RDF_description = RDF_GetResource(gCoreDB, "description", 1);
  gWebData->RDF_Container = RDF_GetResource(gCoreDB, "Container", 1);
  gWebData->RDF_firstVisitDate = RDF_GetResource(gCoreDB, "firstVisitDate", 1);
  gWebData->RDF_lastVisitDate = RDF_GetResource(gCoreDB, "lastVisitDate", 1);
  gWebData->RDF_numAccesses = RDF_GetResource(gCoreDB, "numAccesses", 1);
  gWebData->RDF_creationDate = RDF_GetResource(gCoreDB, "creationDate", 1);
  gWebData->RDF_lastModifiedDate = RDF_GetResource(gCoreDB, "lastModifiedDate", 1);
  gWebData->RDF_size = RDF_GetResource(gCoreDB,  "size", 1);
} 
