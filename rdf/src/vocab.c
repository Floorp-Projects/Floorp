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
#include "bmk2rdf.h"


	/* globals */

#ifdef	XP_MAC
#pragma export on
#endif

RDF_WDVocab gWebData = NULL;
RDF_NCVocab gNavCenter = NULL;
RDF_CoreVocab gCoreVocab = NULL;


#ifdef	XP_MAC
#pragma export off
#endif

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

  while (n < (sizeof(RDF_CoreVocabStruct)/sizeof(RDF_Resource))) {
    *(gAllVocab + m++) = *((RDF_Resource*)gCoreVocab + n++);
  }

  n = 0;
  while (n < (sizeof(RDF_NCVocabStruct)/sizeof(RDF_Resource))) {
    *(gAllVocab + m++) = *((RDF_Resource*)gNavCenter + n++);
  }

  n = 0;
  while (n < (sizeof(RDF_WDVocabStruct)/sizeof(RDF_Resource))) {
    *(gAllVocab + m++) = *((RDF_Resource*)gWebData + n++);
  }
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
  gCoreVocab->RDF_stringEquals = newResource("stringEquals", 0);
  gCoreVocab->RDF_stringNotEquals = newResource("stringNotEquals", 0);
  gCoreVocab->RDF_substring = newResource("substring", 0);
  gCoreVocab->RDF_stringStartsWith = newResource("stringStartsWith", 0);
  gCoreVocab->RDF_stringEndsWith = newResource("stringEndsWith", 0);
  gCoreVocab->RDF_child = RDF_GetResource(gCoreDB, "child", 1);
  gCoreVocab->RDF_content = RDF_GetResource(gCoreDB, "content", 1);
  gCoreVocab->RDF_summary = RDF_GetResource(gCoreDB, "summary", 1);
  gCoreVocab->RDF_comment = RDF_GetResource(gCoreDB, "comment", 1);
  
}



void
createNavCenterVocab () {
  gNavCenter = (RDF_NCVocab) getMem(sizeof(RDF_NCVocabStruct));
#ifdef MOZILLA_CLIENT
  gNavCenter->RDF_overview = RDF_GetResource(gCoreDB, "overview", 1);
  gNavCenter->RDF_Trash = createContainer("Trash");
  gNavCenter->RDF_Clipboard = createContainer("Clipboard");
  gNavCenter->RDF_Top = createContainer("NC:NavCenter"); 
  setResourceType(gNavCenter->RDF_Top, RDF_RT);
  gNavCenter->RDF_Search = createContainer("NC:Search");
  setResourceType(gNavCenter->RDF_Search, SEARCH_RT);  
  gNavCenter->RDF_Sitemaps = createContainer("NC:Sitemaps");
  gNavCenter->RDF_BreadCrumbCategory = createContainer("BreadCrumbs");
  gNavCenter->RDF_BookmarkFolderCategory = createContainer("NC:Bookmarks");
  gNavCenter->RDF_NewBookmarkFolderCategory = RDF_GetResource(gCoreDB, "NewBookmarks", true);
  gNavCenter->RDF_History =  createContainer("NC:History");
  gNavCenter->RDF_HistoryBySite = createContainer("NC:HistoryBySite");
  gNavCenter->RDF_HistoryByDate = createContainer("NC:HistoryByDate");
  gNavCenter->RDF_HistoryMostVisited = createContainer("NC:HistoryMostVisited");
  setResourceType(gNavCenter->RDF_History, HISTORY_RT);
  setResourceType(gNavCenter->RDF_HistoryBySite, HISTORY_RT);
  setResourceType(gNavCenter->RDF_HistoryByDate, HISTORY_RT);
  setResourceType(gNavCenter->RDF_HistoryMostVisited, HISTORY_RT);

  /* IE items */
  gNavCenter->RDF_IEBookmarkFolderCategory = createContainer("NC:IEBookmarks");
  gNavCenter->RDF_IEHistory =  createContainer("NC:IEHistory");
  setResourceType(gNavCenter->RDF_IEHistory, HISTORY_RT);

  gNavCenter->RDF_bookmarkAddDate  = newResource("bookmarkAddDate", 0);
  gNavCenter->RDF_PersonalToolbarFolderCategory = 
    RDF_GetResource(gCoreDB, "PersonalToolbarCat", true);
  gNavCenter->RDF_Column = RDF_GetResource(gCoreDB, "Column", true);
  gNavCenter->RDF_ColumnResource = RDF_GetResource(gCoreDB, "ColumnResource", true);
  gNavCenter->RDF_ColumnWidth = RDF_GetResource(gCoreDB, "ColumnWidth", true);
  gNavCenter->RDF_ColumnIconURL = RDF_GetResource(gCoreDB, "ColumnIconURL", true);
  gNavCenter->RDF_ColumnDataType = RDF_GetResource(gCoreDB, "ColumnDataType", true);
  gNavCenter->RDF_smallIcon = newResource("smallIcon", 0);
  gNavCenter->RDF_largeIcon  = newResource("largeIcon", 0);
  gNavCenter->RDF_HTMLURL = newResource("htmlURL", 0);
  gNavCenter->RDF_HTMLHeight = newResource("htmlHeight", 0);
  gNavCenter->RDF_LocalFiles = createContainer("NC:LocalFiles");
  gNavCenter->RDF_FTP = createContainer("NC:FTP");
  gNavCenter->RDF_FTP = newResource("NC:FTP", 0);
  gNavCenter->RDF_Appletalk = createContainer("NC:Appletalk");
  gNavCenter->RDF_Appletalk = newResource("NC:Appletalk", 0);
  setResourceType(gNavCenter->RDF_Appletalk, 0);
  gNavCenter->RDF_Mail = RDF_GetResource(gCoreDB, "NC:Mail", true);
  gNavCenter->RDF_Guide = RDF_GetResource(gCoreDB, "NC:Guide", true);
  gNavCenter->RDF_Password = RDF_GetResource(gCoreDB, "password", true);
  gNavCenter->RDF_SBProviders = RDF_GetResource(gCoreDB, "NC:SmartBrowsingProviders", true);
  gNavCenter->RDF_WorkspacePos = RDF_GetResource(gCoreDB, "workspacePos", true);
  gNavCenter->RDF_ItemPos = RDF_GetResource(gCoreDB, "pos", true);
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
  gNavCenter->RDF_methodType = RDF_GetResource (gCoreDB, "methodType", true);
  gNavCenter->RDF_prompt = RDF_GetResource (gCoreDB, "prompt", true);  
  gNavCenter->RDF_HTMLType = RDF_GetResource (gCoreDB, "HTMLPage", true);
  gNavCenter->RDF_URLShortcut = RDF_GetResource(gCoreDB, "URLShortcut", true);
  gNavCenter->RDF_Poll = RDF_GetResource(gCoreDB, "poll", true);
  gNavCenter->RDF_PollInterval = RDF_GetResource(gCoreDB, "pollInterval", true);
  gNavCenter->RDF_PollURL = RDF_GetResource(gCoreDB, "pollURL", true);
  
  gNavCenter->RDF_Cookies = createContainer("NC:Cookies");
  setResourceType(gNavCenter->RDF_Cookies, COOKIE_RT); 

  gNavCenter->RDF_Toolbar = createContainer("NC:Toolbar");
  gNavCenter->RDF_JSec = createContainer("NC:Jsec");
  gNavCenter->RDF_JSecPrincipal = RDF_GetResource(gCoreDB, "JsecPrincipal", true);
  gNavCenter->RDF_JSecTarget = RDF_GetResource(gCoreDB, "JsecTarget", true);
  gNavCenter->RDF_JSecAccess = RDF_GetResource(gCoreDB, "JsecAccess", true);

  /* Commands */
  
  gNavCenter->RDF_Command = RDF_GetResource (gCoreDB, "Command", true);
  gNavCenter->RDF_Command_Launch = RDF_GetResource(gCoreDB, "Command:Launch", true);
  gNavCenter->RDF_Command_Refresh = RDF_GetResource(gCoreDB, "Command:Refresh", true);
  gNavCenter->RDF_Command_Reveal = RDF_GetResource(gCoreDB, "Command:Reveal", true);
  gNavCenter->RDF_Command_Atalk_FlatHierarchy = RDF_GetResource(gCoreDB, "Command:at:View Zone List", true);
  gNavCenter->RDF_Command_Atalk_Hierarchy = RDF_GetResource(gCoreDB, "Command:at:View Zone Hierarchy", true);

  /* NavCenter appearance styles */

  gNavCenter->viewFGColor = newResource("viewFGColor", 0);
  gNavCenter->viewBGColor = newResource("viewBGColor", 0);
  gNavCenter->viewBGURL = newResource("viewBGURL", 0);
  gNavCenter->showTreeConnections = newResource("showTreeConnections", 0);
  gNavCenter->treeConnectionFGColor = newResource("treeConnectionFGColor", 0);
  gNavCenter->treeOpenTriggerIconURL = newResource("treeOpenTriggerIconURL", 0);
  gNavCenter->treeClosedTriggerIconURL = newResource("treeClosedTriggerIconURL", 0);
  gNavCenter->selectionFGColor = newResource("selectionFGColor", 0);
  gNavCenter->selectionBGColor = newResource("selectionBGColor", 0);
  gNavCenter->columnHeaderFGColor = newResource("columnHeaderFGColor", 0);
  gNavCenter->columnHeaderBGColor = newResource("columnHeaderBGColor", 0);
  gNavCenter->columnHeaderBGURL = newResource("columnHeaderBGURL", 0);
  gNavCenter->showColumnHeaders = newResource("showColumnHeaders", 0);
  gNavCenter->showColumnHeaderDividers = newResource("showColumnHeaderDividers", 0);
  gNavCenter->sortColumnFGColor = newResource("sortColumnFGColor", 0);
  gNavCenter->sortColumnBGColor = newResource("sortColumnBGColor", 0);
  gNavCenter->titleBarFGColor = newResource("titleBarFGColor", 0);
  gNavCenter->titleBarBGColor = newResource("titleBarBGColor", 0);
  gNavCenter->titleBarBGURL = newResource("titleBarBGURL", 0);
  gNavCenter->dividerColor = newResource("dividerColor", 0);
  gNavCenter->showDivider = newResource("showDivider", 0);
  gNavCenter->selectedColumnHeaderFGColor = newResource("selectedColumnHeaderFGColor", 0);
  gNavCenter->selectedColumnHeaderBGColor = newResource("selectedColumnHeaderBGColor", 0);
  gNavCenter->showColumnHilite = newResource("showColumnHilite", 0);
  gNavCenter->triggerPlacement = newResource("triggerPlacement", 0);

  /* NavCenter behavior properties */
  gNavCenter->useInlineEditing = newResource("useInlineEditing", 0 /* XXX */);
  gNavCenter->useSingleClick = newResource("useSingleClick", 0 /* XXX */);
  gNavCenter->loadOpenState = newResource("loadOpenState", 0 /* XXX */);
  gNavCenter->saveOpenState = newResource("saveOpenState", 0 /* XXX */);
 
  /* Toolbars */

  /* Toolbar Appearance Styles */
  gNavCenter->toolbarBitmapPosition = newResource("toolbarBitmapPosition", 0 /* XXX "Toolbar Bitmap Position" */ );
  gNavCenter->toolbarButtonsFixedSize = newResource("toolbarButtonsFixedSize", 0 /* XXX "Toolbar Bitmap Position" */ );
  gNavCenter->toolbarDisplayMode = newResource("toolbarDisplayMode", 0);
  gNavCenter->toolbarCollapsed = newResource("toolbarCollapsed", 0);
  gNavCenter->toolbarVisible = newResource("toolbarVisible", 0);
  gNavCenter->toolbarDisabledIcon = newResource("toolbarDisabledIcon", 0 /* XXX */);
  gNavCenter->toolbarEnabledIcon  = newResource("toolbarEnabledIcon", 0 /* XXX */);
  gNavCenter->toolbarRolloverIcon = newResource("toolbarRolloverIcon", 0 /* XXX */);
  gNavCenter->toolbarPressedIcon  = newResource("toolbarPressedIcon", 0 /* XXX */);
  gNavCenter->buttonTooltipText = newResource("buttonTooltipText", 0 /* XXX */);
  gNavCenter->buttonStatusbarText = newResource("buttonStatusbarText", 0 /* XXX */);
  gNavCenter->buttonBorderStyle = newResource("buttonBorderStyle", 0 /* XXX */);
  gNavCenter->urlBar = newResource("urlBar", 0 /* XXX */);
  gNavCenter->urlBarWidth = newResource("urlBarWidth", 0 /* XXX */);
  gNavCenter->pos = newResource("pos", 0 /* XXX */);
  gNavCenter->viewRolloverColor = newResource("viewRolloverColor", 0 /* XXX */);
  gNavCenter->viewPressedColor = newResource("viewPressedColor", 0 /* XXX */);
  gNavCenter->viewDisabledColor = newResource("viewDisabledColor", 0 /* XXX */);
  gNavCenter->controlStripFGColor = newResource("controlStripFGColor", 0 /* XXX */);
  gNavCenter->controlStripBGColor = newResource("controlStripBGColor", 0 /* XXX */);
  gNavCenter->controlStripBGURL = newResource("controlStripBGURL", 0 /* XXX */);
  gNavCenter->controlStripCloseText = newResource("controlStripCloseText", 0 /* XXX */);
  gNavCenter->titleBarShowText = newResource("titleBarShowText", 0 /* XXX */);
  gNavCenter->showTitleBar = newResource("showTitleBar", 0 /* XXX */);
  gNavCenter->showControlStrip = newResource("showControlStrip", 0 /* XXX */);

  /* Buttons */
  gNavCenter->buttonTreeState = newResource("buttonTreeState", 0 /* XXX */);

  /* Cookies */
  gNavCenter->cookieDomain = newResource("cookieDomain", 0 /* XXX */);
  gNavCenter->cookieValue = newResource("cookieValue", 0 /* XXX */);
  gNavCenter->cookieHost  = newResource("cookieHost", 0 /* XXX */);
  gNavCenter->cookiePath  = newResource("cookiePath", 0 /* XXX */);
  gNavCenter->cookieSecure = newResource("cookieSecure", 0 /* XXX */);
  gNavCenter->cookieExpires = newResource("cookieExpiration", 0 /* XXX */);
  gNavCenter->from = newResource("mail:From", 0 );  
  gNavCenter->to   = newResource("mail:To", 0 );
  gNavCenter->subject = newResource("mail:Subject", 0 /* XXX */);
  gNavCenter->date = newResource("mail:Date", 0 /* XXX */);
  gNavCenter->displayURL = newResource("displayURL", 0 /* XXX */);
  remoteStoreAdd(gRemoteStore, gNavCenter->from, gCoreVocab->RDF_name, copyString("from"), RDF_STRING_TYPE, 1);
  remoteStoreAdd(gRemoteStore, gNavCenter->to, gCoreVocab->RDF_name, copyString("to"), RDF_STRING_TYPE, 1);
  remoteStoreAdd(gRemoteStore, gNavCenter->subject, gCoreVocab->RDF_name, copyString("subject"), RDF_STRING_TYPE, 1);
  remoteStoreAdd(gRemoteStore, gNavCenter->date, gCoreVocab->RDF_name, copyString("date"), RDF_STRING_TYPE, 1);
#endif /* MOZILLA_CLIENT */
}



void
createWebDataVocab ()
{
  gWebData = (RDF_WDVocab) getMem(sizeof(RDF_WDVocabStruct));
#ifdef MOZILLA_CLIENT
  gWebData->RDF_URL =  newResource("URL", 0);
  gWebData->RDF_description = newResource("description", 0);
  gWebData->RDF_keyword = newResource("keyword", 0 /* XXX */);
  gWebData->RDF_Container = RDF_GetResource (gCoreDB, "Container", true);
  gWebData->RDF_firstVisitDate = newResource("firstVisitDate", 0);
  gWebData->RDF_lastVisitDate = newResource("lastVisitDate", 0);
  gWebData->RDF_numAccesses = newResource("numAccesses", 0);
  gWebData->RDF_creationDate = newResource("creationDate", 0);
  gWebData->RDF_lastModifiedDate = newResource("lastModifiedDate", 0);
  gWebData->RDF_size = newResource("size", 0);
#endif /* MOZILLA_CLIENT */
} 



RDF_Resource
newResource(char *id, int optionalNameStrID)
{
	RDF_Resource		r;

	r = RDF_GetResource(gCoreDB, id, true);
	return(r);
}



char *
getResourceDefaultName(RDF_Resource node)
{
	int			strID = 0;
	char			*defaultName = NULL;
    return resourceID(node);
}

char	*gDefaultNavcntr =
 "<RDF:RDF>  <Topic id=\"NC:Toolbar\">  <child> <Topic id=\"NC:CommandToolBar\" name=\"Command Toolbar\" toolbarBitmapPosition=\"top\"  toolbarButtonsFixedSize=\"yes\"  >  <child href=\"command:back\" name=\"Back\"/>  <child buttonTooltipText=\"Reload this page from the server\"  buttonStatusbarText=\"Reload the current page\" href=\"command:reload\" name=\"Reload\"/>  <child href=\"command:stop\" name=\"Stop\"/> <child href=\"command:forward\" name=\"Forward\"/>  <child name=\"separator0\" href=\"nc:separator0\"/>  <child href=\"command:urlbar\" name=\" \"  buttonStatusBarText=\"Location/Search Bar\"   buttonTooltipText=\"Location/Search Bar\"  urlBar=\"Yes\" urlBarWidth=\"*\"/>   <child name=\"separator2\" href=\"nc:separator2\"/> </Topic>  </child>   <child> <Topic id=\"NC:InfoToolbar\" name=\"Info Toolbar\">         <child>           <Topic id=\"NC:Bookmarks\" name=\"Bookmarks\"></Topic>          </child>                     <child>             <Topic id=\"NC:History\" largeIcon=\"icon/large:workspace,history\" name=\"History\">                    <child href=\"NC:HistoryMostVisited\" name=\"Most Frequented Pages\"/>                 <child href=\"NC:HistoryBySite\" name=\"History By Site\"/>                  <child href=\"NC:HistoryByDate\" name=\"History By Date\"/>           </Topic>           </child>                   <child href=\"NC:Sitemaps\" name=\"Related\" htmlURL=\"http://rdf.netscape.com/rdf/navcntradvert.html\"/>  </Topic> </child>  <child> <Topic id=\"NC:PersonalToolbar\" name=\"Personal Toolbar\"> </Topic> </child>  </Topic>  <Topic id=\"NC:NavCenter\"> <child href=\"NC:Bookmarks\" name=\"Bookmarks\"/> <child href=\"NC:Search\"  largeIcon=\"icon/large:workspace,search\" name=\"Search\"/> <child href=\"NC:History\" name=\"History\"/> <child id=\"NC:Sitemaps\" name=\"Site Tools\" htmlURL=\"http://rdf.netscape.com/rdf/navcntradvert.html\" /> <child id=\"NC:LocalFiles\" name=\"Files\" largeIcon=\"http://rdf.netscape.com/rdf/heabou.gif\"/> </Topic></RDF:RDF>"     ;
