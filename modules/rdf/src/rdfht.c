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

/* 
   This file implements high level support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "rdf-int.h"
#include "xpassert.h"
#include "fs2rdf.h"
#include "glue.h"
#include "vocab.h"
#include "vocabint.h"
#include "bmk2mcf.h"
#include "ht.h"
#include "mcf.h"


	/* externs */
extern	char	*profileDirURL;
extern	char	*gBookmarkURL;
extern	RDF	gNCDB ;
extern	char	*gGlobalHistoryURL;

static PRBool sRDFInitedB = PR_FALSE;
char*  gNavCntrUrl;


char * gNavCenterDataSources[15] = 
{"rdf:localStore", "rdf:remoteStore", "rdf:remoteStore", "rdf:history",
 /* "rdf:ldap", */
 "rdf:esftp",
 "rdf:lfs", "rdf:CookieStore",
 "rdf:columns",  "rdf:find",

#ifdef	XP_MAC
	"rdf:appletalk",
#endif
 
 NULL, NULL
};



RDF
newNavCenterDB()
{
	return  RDF_GetDB((char**)gNavCenterDataSources);
}


void
walkThroughAllBookmarks (RDF_Resource u)
{
#ifdef MOZILLA_CLIENT
  RDF_Cursor c = RDF_GetSources(gNCDB, u, gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE, true);
  RDF_Resource next;
  while ((next = RDF_NextValue(c)) != NULL) {
    if (resourceType(next) == RDF_RT) walkThroughAllBookmarks(next);
  }
  RDF_DisposeCursor(c);
#endif
}



PR_PUBLIC_API(RDF_Error)
RDF_Init(RDF_InitParams params)
{
#ifdef MOZILLA_CLIENT
  char* navCenterURL;
#endif
  if ( sRDFInitedB )
    return -1;

#ifdef MOZILLA_CLIENT
  XP_ASSERT(params->profileURL != NULL);
  XP_ASSERT(params->bookmarksURL != NULL);
  XP_ASSERT(params->globalHistoryURL != NULL);

  /*
     copy init params out before doing anything else (such as creating vocabulary)
     to prevent any XP_GetString round-robin problems (ex: FE could be using XP_GetString
     to pass in the init strings, which createVocabs() could affect
  */
  profileDirURL     = copyString(params->profileURL);
  gBookmarkURL      = copyString(params->bookmarksURL);
  gGlobalHistoryURL = copyString(params->globalHistoryURL);
#endif

  resourceHash = PL_NewHashTable(500, PL_HashString, PL_CompareStrings, 
                                 PL_CompareValues,   NULL, NULL);
  dataSourceHash = PL_NewHashTable(100, PL_HashString, PL_CompareStrings, 
                                   PL_CompareValues,  NULL, NULL);
  RDFglueInitialize();
  MakeRemoteStore("rdf:remoteStore");
  createVocabs();
  sRDFInitedB = PR_TRUE;

#ifdef MOZILLA_CLIENT
  PREF_SetDefaultCharPref("browser.NavCenter", "http://rdf.netscape.com/rdf/navcntr.rdf");
  PREF_CopyCharPref("browser.NavCenter", &navCenterURL);
    
  if (!strchr(navCenterURL, ':')) {
    navCenterURL = makeDBURL(navCenterURL);
  }
   gNavCntrUrl = copyString(navCenterURL);
  *(gNavCenterDataSources + 1) = copyString(navCenterURL);
  gNCDB = newNavCenterDB(); 
  freeMem(navCenterURL);

  HT_Startup();

  GuessIEBookmarks();

#endif
  walkThroughAllBookmarks(RDF_GetResource(NULL, "NC:Bookmarks", true));
  return 0;
}



/*need to keep a linked list of all the dbs opened so that they
  can all be closed down on exit */



PR_PUBLIC_API(RDF_Error)
RDF_Shutdown ()
{
#ifdef MOZILLA_CLIENT
  /*  flushBookmarks(); */
  HT_Shutdown();
  RDFglueExit();
  if (profileDirURL != NULL)
  {
  	freeMem(profileDirURL);
  	profileDirURL = NULL;
  }
  if (gBookmarkURL != NULL)
  {
  	freeMem(gBookmarkURL);
  	gBookmarkURL = NULL;
  }
  if (gGlobalHistoryURL != NULL)
  {
  	freeMem(gGlobalHistoryURL);
  	gGlobalHistoryURL = NULL;
  }
  if (gLocalStoreURL != NULL)
  {
  	freeMem(gLocalStoreURL);
  	gLocalStoreURL = NULL;
  }
#endif
  disposeAllDBs();
  sRDFInitedB = PR_FALSE;
  
  return 0;
}
