/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rdf.h"
#include "gs.h"

RDF_Resource 
getNodeFromQuery (char* query) {
  RDF_Resource ans;
  if (!(ans = RDF_GetResource(query, 0))) {
    return RDF_GetResource("Top", 1);
  } else return ans;
}

#define ROW_WIDTH 3

void 
AnswerOpenDirQuery (WriteClientProc callBack, void* obj, char *query) {
  char *buff = (char*) malloc(10000);
  RDF_Resource items[300];
  RDF_Resource topics[100];
  RDF_Resource child = RDF_GetResource("child", 1);
  RDF_Resource name = RDF_GetResource("name", 1);
  RDF_Resource type = RDF_GetResource("type", 1);
  RDF_Resource topic = RDF_GetResource("Topic", 1);
  RDF_Resource desc = RDF_GetResource("description", 1);
  RDF_Resource node = getNodeFromQuery(query);
  int itemCount = 0;
  int topicCount = 0;

  if (node) {
    RDF_Cursor c = RDF_GetTargets(0, node, child, RDF_RESOURCE_TYPE);
    RDF_Resource ans ;
    while (c && (ans = (RDF_Resource) RDF_NextValue(c))) {
      int   subjectp = RDF_HasAssertion(0, ans, type, topic, RDF_RESOURCE_TYPE); 
      if (subjectp) {
        topics[topicCount++] = ans;
      } else {
        items[itemCount++] = ans;
      }
    }
    RDF_DisposeCursor(c);

    if (topicCount > 0) {
      int n = 0;
      (*callBack)(obj, "<hr><table cellspacing=\"4\" cellpadding=\"6\">");
      while (n < topicCount) {
        int w = 0;
        (*callBack)(obj, "<tr>");
        while ((w < ROW_WIDTH) && (n < topicCount)) {
          RDF_Resource u = topics[n];
          char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
          char* id = RDF_ResourceID(u);
          sprintf(buff, "<td><li><a href=\"OpenDir?%s\">%s</a></td>", id, (nm ? nm : id));
          (*callBack)(obj, buff);
          w++;
          n++;
        }
        (*callBack)(obj, "</tr>");
      }
      (*callBack)(obj, "</table>");
    }
     (*callBack)(obj, "<hr>");
    if (itemCount > 0) {
      int n = 0;
      (*callBack)(obj, "<ul>");
      while (n < itemCount) {
        int w = 0;
        
          RDF_Resource u = items[n];
          char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
          char* id = RDF_ResourceID(u);
          sprintf(buff, "<li><a href=\"%s\">%s</a>", id, (nm ? nm : id));
          (*callBack)(obj, buff);
           
          n++;
         
         
      }
      (*callBack)(obj, "</ul>");
    }
  }
  free(buff);  
}

void 
AnswerSearchQuery (WriteClientProc callBack, void* obj, char *query) {
    RDF_Cursor c = RDFGS_Search(0, query, 0);
    char *buff = (char*) malloc(10000);
    RDF_Resource name = RDF_GetResource("name", 1);
    RDF_Resource u ;
    while (c && (u = (RDF_Resource) RDFGS_NextValue(c))) {
      char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
      char* id = RDF_ResourceID(u);
      sprintf(buff, "<li><a href=\"%s\">%s</a>", id, (nm ? nm : id));
      (*callBack)(obj, buff);
    }
    freeMem(buff);
}
    
    
  
  
  

