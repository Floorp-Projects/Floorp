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

#ifndef	_RDF_VOCAB_H_
#define	_RDF_VOCAB_H_

typedef struct _RDF_CoreVocabStruct {
  RDF_Resource RDF_parent;
  RDF_Resource RDF_name;
  RDF_Resource RDF_instanceOf;
  RDF_Resource RDF_subClassOf;
  RDF_Resource RDF_PropertyType;
  RDF_Resource RDF_Class;
  RDF_Resource RDF_slotsHere;
  RDF_Resource RDF_slotsIn;
  RDF_Resource RDF_domain;
  RDF_Resource RDF_range;
  RDF_Resource RDF_StringType;
  RDF_Resource RDF_IntType; 
  RDF_Resource RDF_equals;
  RDF_Resource RDF_lessThan;
  RDF_Resource RDF_greaterThan;
  RDF_Resource RDF_lessThanOrEqual;
  RDF_Resource RDF_greaterThanOrEqual;
  RDF_Resource RDF_stringEquals;
  RDF_Resource RDF_stringNotEquals;
  RDF_Resource RDF_substring;
  RDF_Resource RDF_stringStartsWith;
  RDF_Resource RDF_stringEndsWith;
  RDF_Resource RDF_child;
  RDF_Resource RDF_comment;
  RDF_Resource RDF_content;
  RDF_Resource RDF_summary;
} RDF_CoreVocabStruct;

typedef RDF_CoreVocabStruct* RDF_CoreVocab;

typedef struct _RDF_NCVocabStruct {
  RDF_Resource RDF_overview;
  RDF_Resource RDF_Trash;
  RDF_Resource RDF_Clipboard;
  RDF_Resource RDF_Top;
  RDF_Resource RDF_Search;
  RDF_Resource RDF_Sitemaps;
  RDF_Resource RDF_BreadCrumbCategory;
  RDF_Resource RDF_BookmarkFolderCategory;
  RDF_Resource RDF_NewBookmarkFolderCategory;
  RDF_Resource RDF_History;
  RDF_Resource RDF_HistoryBySite;
  RDF_Resource RDF_HistoryByDate;
  RDF_Resource RDF_bookmarkAddDate;
  RDF_Resource RDF_PersonalToolbarFolderCategory;
  RDF_Resource RDF_Column;
  RDF_Resource RDF_ColumnResource;
  RDF_Resource RDF_ColumnWidth;
  RDF_Resource RDF_ColumnIconURL;
  RDF_Resource RDF_ColumnDataType;
  RDF_Resource RDF_smallIcon;
  RDF_Resource RDF_largeIcon;
  RDF_Resource RDF_Guide;
  RDF_Resource RDF_HTMLURL;
  RDF_Resource RDF_HTMLHeight;
  RDF_Resource RDF_LocalFiles;
  RDF_Resource RDF_FTP;
  RDF_Resource RDF_Appletalk;
  RDF_Resource RDF_Mail;
  RDF_Resource RDF_Password;
  RDF_Resource RDF_SBProviders;
  RDF_Resource RDF_WorkspacePos;
  RDF_Resource RDF_Locks;
  RDF_Resource RDF_AddLock;
  RDF_Resource RDF_DeleteLock;
  RDF_Resource RDF_IconLock;
  RDF_Resource RDF_NameLock;
  RDF_Resource RDF_CopyLock;
  RDF_Resource RDF_MoveLock;
  RDF_Resource RDF_WorkspacePosLock;  
  RDF_Resource RDF_DefaultSelectedView;
  RDF_Resource RDF_AutoOpen;
  RDF_Resource RDF_resultType;
  RDF_Resource RDF_HTMLType;
  RDF_Resource RDF_URLShortcut;
  RDF_Resource RDF_Cookies;
  RDF_Resource RDF_Toolbar;

  /* Commands */
  
  RDF_Resource RDF_Command;
  RDF_Resource RDF_Command_Launch;
  RDF_Resource RDF_Command_Refresh;
  RDF_Resource RDF_Command_Atalk_FlatHierarchy;
  RDF_Resource RDF_Command_Atalk_Hierarchy;

  /* NavCenter appearance styles */

  RDF_Resource treeFGColor;
  RDF_Resource treeBGColor;
  RDF_Resource treeBGURL;
  RDF_Resource showTreeConnections;
  RDF_Resource treeConnectionFGColor;
  RDF_Resource treeOpenTriggerIconURL;
  RDF_Resource treeClosedTriggerIconURL;
  RDF_Resource selectionFGColor;
  RDF_Resource selectionBGColor;
  RDF_Resource columnHeaderFGColor;
  RDF_Resource columnHeaderBGColor;
  RDF_Resource columnHeaderBGURL;
  RDF_Resource showColumnHeaders;
  RDF_Resource showColumnHeaderDividers;
  RDF_Resource sortColumnFGColor;
  RDF_Resource sortColumnBGColor;
  RDF_Resource titleBarFGColor;
  RDF_Resource titleBarBGColor;
  RDF_Resource titleBarBGURL;
  RDF_Resource dividerColor;
  RDF_Resource showDivider;
  RDF_Resource selectedColumnHeaderFGColor;
  RDF_Resource selectedColumnHeaderBGColor;
  RDF_Resource showColumnHilite;
  RDF_Resource triggerPlacement;
} RDF_NCVocabStruct;

typedef RDF_NCVocabStruct* RDF_NCVocab;

typedef struct _RDF_WDVocabStruct {
  RDF_Resource RDF_URL;
  RDF_Resource RDF_description;
  RDF_Resource RDF_Container;
  RDF_Resource RDF_firstVisitDate;
  RDF_Resource RDF_lastVisitDate;
  RDF_Resource RDF_numAccesses;
  RDF_Resource RDF_creationDate;
  RDF_Resource RDF_lastModifiedDate;
  RDF_Resource RDF_size;
} RDF_WDVocabStruct;

typedef RDF_WDVocabStruct* RDF_WDVocab;

#endif
