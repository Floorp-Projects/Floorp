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
 * test.cpp
 *
 * This file is provided to ensure that the RDF engine will
 * compile and run standalone (outside of mozilla). It may also
 * be useful as a demonstration of how to initialize and
 * use the engine, and possibly for performance testing.
 * Finally, it ensures that the header files are probably written
 * for C++.
 *
 * Currently this program simply reads in an rdf site-map file
 * and spits it back out to the display.  Feel free to
 * modify/enhance as desired.
 *
 * See Dan Libby (danda@netscape.com) for more info.
 * 
 */

#include "stdlib.h"
#include "rdf-int.h"
#include "rdf.h"
#include "rdfparse.h"

const char    *dataSources[] = { 
   "rdf:remoteStore", NULL 
}; 

void 
main(int argc, char** argv) 
{ 
   RDF                     rdf; 
   RDF_Error               err; 
   RDF_InitParamsStruct    initParams = {0}; 
   RDF_Resource  u, s, root; 
   void   *v; 
   RDFFile file;
#ifdef XP_WIN
   char* fileURL = (argc > 1) ? argv[1] : "file:///test.rdf";
#else
   char* fileURL = (argc > 1) ? argv[1] : "file://test.rdf";
#endif
   char* rootURL = (char*)getMem(200);

   err = RDF_Init(&initParams); 
   if (err)
   {
      perror("RDF_Init: "); 
      exit(1); 
   }
   else
   {
      printf("RDF Init success\n"); 
   } 

  /* 
   * Try and get a reference to the remote store DB 
   */ 
   rdf = RDF_GetDB(dataSources); 
   if (rdf == NULL)
   {
      perror("RDF_GetDB"); 
      exit(1); 
   }
   else
   {
      printf("RDF_GetDB success\n"); 
   } 
   sprintf(rootURL, "%s#root", fileURL);
   root = (RDF_Resource)RDF_GetResource(NULL, rootURL, PR_TRUE);
   setResourceType(root, RDF_RT);

   /* Create a test resource */
   u = RDF_GetResource(rdf, "http://people.netscape.com/danda/", TRUE); 
   s = gCoreVocab->RDF_name; 
   v = "Dan Libby";  

   /* make an assertion into RDF's graph */
   RDF_Assert(rdf, u,s,v, RDF_STRING_TYPE);  

   /* check to see if assertion exists */
   if (!RDF_HasAssertion(rdf,u,s,v, RDF_STRING_TYPE, true))
   {
      printf("Assertion failure.\n"); 
   }
   else
   {
      printf("Assertion success.\n"); 
   } 

   /* Import an RDF file */
   printf("Reading \"%s\"\n", fileURL);
   fflush(stdout);

   file = readRDFFile (fileURL, root, PR_TRUE, gRemoteStore);
   if (file && file->assertionCount > 0)
   {
      printf("\"%s\" read in successfully. (%i assertions)\n", fileURL, file->assertionCount);
      fflush(stdout);

      PRFileDesc *fd = PR_GetSpecialFD(PR_StandardOutput);
      outputRDFTree (rdf, fd, root);

#if 0
      RDF_Cursor c;
      int i = 0;

      c = RDF_GetSources(rdf, root, gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE, true);
      if (c)
      {
         u = (RDF_Resource)RDF_NextValue(c);
         while (u)
         {
            printf("%i: %s\n", ++i, u->url);
            u = (RDF_Resource)RDF_NextValue(c);
         }

         RDF_DisposeCursor(c);
      }
#endif
   }
   else
   {
      printf("error reading %s\n", fileURL);
   }

   RDF_Shutdown(); 
}


/* This function has to be here when building standalone RDF or you
 * will get a link error.
 */
extern "C"
void notifySlotValueAdded(RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)
{
   if(type == RDF_STRING_TYPE)
   {
#if 0
      printf("String Value added: %s\n", (char*)v);
#endif
   }
   else if(type == RDF_RESOURCE_TYPE)
   {
      if(type == RDF_RESOURCE_TYPE)
      {
         /* Right here you can find out when 
          * resources are added, and what their
          * ids are, for querying later.  This is
          * useful when the ID of the resource is
          * not known at compile time.
          */
#if 0
          printf("Resource added, ID: %s\n", resourceID((RDF_Resource)v));
#endif
      }
   }
}


