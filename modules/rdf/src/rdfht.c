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
   This file implements high level support for the rdf data model.
   For more information on this file, contact rjc or guha 
   For more information on RDF, look at the RDF section of www.mozilla.org
*/

#include "rdf-int.h"
#include "xpassert.h"
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



char * gNavCenterDataSources[15] = 
{"rdf:localStore", "rdf:remoteStore", "rdf:remoteStore", "rdf:history",
 /* "rdf:ldap", */
 "rdf:esftp",
 "rdf:lfs",
 "rdf:columns",  NULL
};



RDF
newNavCenterDB()
{
	return  RDF_GetDB((char**)gNavCenterDataSources);
}



PR_PUBLIC_API(RDF_Error)
RDF_Init(RDF_InitParams params)
{
  char* navCenterURL;
  if ( sRDFInitedB )
    return -1;

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

  resourceHash = PL_NewHashTable(500, PL_HashString, PL_CompareStrings, PL_CompareValues,  
				 NULL, NULL);
  RDFglueInitialize();
  MakeRemoteStore("rdf:remoteStore");
  createVocabs();
  sRDFInitedB = PR_TRUE;

  PREF_SetDefaultCharPref("browser.NavCenter", "http://rdf.netscape.com/rdf/navcntr.rdf");
  PREF_CopyCharPref("browser.NavCenter", &navCenterURL);
  if (!strchr(navCenterURL, ':')) {
    navCenterURL = makeDBURL(navCenterURL);
  } else {
    copyString(navCenterURL);
  }
  *(gNavCenterDataSources + 1) = copyString(navCenterURL);
  gNCDB = newNavCenterDB();
  freeMem(navCenterURL);

  HT_Startup();

  return 0;
}



/*need to keep a linked list of all the dbs opened so that they
  can all be closed down on exit */



PR_PUBLIC_API(RDF_Error)
RDF_Shutdown ()
{

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
  disposeAllDBs();
  sRDFInitedB = PR_FALSE;
  
  return 0;
}

