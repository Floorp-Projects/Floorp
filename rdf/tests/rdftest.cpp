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

#define RDF_DB "file:///sitemap.rdf"
#define SUCCESS 0
#define FAILURE -1

void fail(char* msg);

int
main(int argc, char** argv)
{
  nsIRDFService* pRDF = 0;

  NS_GetRDFService( &pRDF );
  PR_ASSERT( pRDF != 0 );

  nsIRDFDataBase* pDB;
  char* url[] = { RDF_DB, NULL };

  /* turn on logging */

  pRDF->CreateDatabase( url, &pDB );
  PR_ASSERT( pDB != 0 );

  /* execute queries */
  RDF_Resource resource = 0;
  if( NS_OK != pDB->CreateResource("http://www.hotwired.com", &resource) )
    fail("Unable to get resource on db!!!\n");
  RDF_Resource child = 0;
  if( NS_OK != pDB->GetResource("child", &resource) )
    fail("Unable to get resource 'child'!!!\n");
  PR_ASSERT( child != 0 );
  {
    // enumerate children
    nsIRDFCursor* cursor;
    if( NS_OK != pDB->GetTargets( resource, child, RDF_RESOURCE_TYPE, &cursor ) )
	fail("Unable to get targets on db\n!!!");
    
    PRBool hasElements;
    cursor->HasElements( hasElements );
    while( hasElements ) {
      RDF_NodeStruct node;
      cursor->Next( node );
      pDB->ReleaseResource( node.value.r );
      cursor->HasElements( hasElements );
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
