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

#include "rdf.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsIURL.h"
#include "nsINetService.h"
#include "plstr.h"
#include "plevent.h"
#include "nsRepository.h"

#define RDF_DB "rdf:bookmarks"
#define SUCCESS 0
#define FAILURE -1


#ifdef XP_PC
#define NETLIB_DLL "netlib.dll"
#endif

#include "nsIPostToServer.h"
#include "nsINetService.h"


static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

NS_DEFINE_IID(kIPostToServerIID, NS_IPOSTTOSERVER_IID);

void fail(char* msg);

int
main(int argc, char** argv)
{
  nsIRDFService* pRDF = 0;
  PL_InitializeEventsLib("");
  nsRepository::RegisterFactory(kNetServiceCID, NETLIB_DLL, PR_FALSE, PR_FALSE);

  NS_GetRDFService( &pRDF );
  PR_ASSERT( pRDF != 0 );
  pRDF->SetBookmarkFile("bookmark.htm");
  nsIRDFDataBase* pDB;
  char* url[] = { RDF_DB, NULL };

  /* turn on logging */

  pRDF->CreateDatabase((const char**) url, &pDB );
  PR_ASSERT( pDB != 0 );

  /* execute queries */
  RDF_Resource resource = 0;
  
  if( NS_OK != pDB->CreateResource("NC:Bookmarks", &resource) )
    fail("Unable to get resource on db!!!\n");
  RDF_Resource parent = 0;
  if( NS_OK != pDB->GetResource("parent", &parent) )
    fail("Unable to get resource 'parent'!!!\n");
  PR_ASSERT(parent != 0 );
  {
    // enumerate children
    nsIRDFCursor* cursor;
    if( NS_OK != pDB->GetSources(resource, parent, RDF_RESOURCE_TYPE, &cursor ) )
	fail("Unable to get targets on db\n!!!");
    RDF_NodeStruct node;
	cursor->Next(node);
    while(node.value.r != NULL) { 
      char* url;
      pRDF->ResourceIdentifier(node.value.r, &url);
      printf("%s\n", url );
      pDB->ReleaseResource( node.value.r );
	  	cursor->Next(node);
    }

  }

  pDB->Release(); /* destroy the DB */
  pRDF->Release(); /* shutdown the RDF system */

  return( SUCCESS );
}

void fail(char* msg)
{
  fprintf(stderr,msg);
  exit( FAILURE );
}
