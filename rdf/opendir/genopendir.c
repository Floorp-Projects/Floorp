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

void
describeItem (WriteClientProc callBack, void* obj, RDF_Resource u) ;

RDF_Resource 
getNodeFromQuery (char* query) {
  RDF_Resource ans;
  if (!(ans = RDF_GetResource(query, 0))) {
    return RDF_GetResource("Top", 1);
  } else return ans;
}

#define ROW_WIDTH 3

#define PREFIX "<form method=get action=\"OpenDir?\"><B>Search:</B> <input size=25 name=search> <input type=submit value=search><br>"


void 
AnswerOpenDirQuery (WriteClientProc callBack, void* obj, char *query) {
  char buff[1000];
  RDF_Resource narrow = RDF_GetResource("narrow", 1);
  RDF_Resource link = RDF_GetResource("link", 1);
  RDF_Resource name = RDF_GetResource("name", 1);
  RDF_Resource topic = RDF_GetResource("Topic", 1);
  RDF_Resource desc = RDF_GetResource("description", 1);
  RDF_Resource editor = RDF_GetResource("editor", 1);
  RDF_Resource newsGroup = RDF_GetResource("newsGroup", 1);
  RDF_Resource node = getNodeFromQuery(query);

  if (node) {
    RDF_Cursor c = RDF_GetTargets(0, node, narrow, RDF_RESOURCE_TYPE);
    RDF_Resource u = (RDF_Resource) RDF_NextValue(c);
    (*callBack)(obj, PREFIX); 
    if (u) {
      (*callBack)(obj, "<hr><table cellspacing=\"6\" cellpadding=\"6\">");
      while (u) {
        int w = 0;
        (*callBack)(obj, "<tr>");
        while ((w < ROW_WIDTH) && u) {
          char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
          char* id = RDF_ResourceID(u);
          if (!nm) nm = strrchr(id, '/') +1;
          sprintf(buff, "<td width=\"%i%\"><li><a href=\"OpenDir?browse=%s\">%s</a></td>",
                  (100 / ROW_WIDTH), id, (nm ? nm : id));
          (*callBack)(obj, buff);
          w++;
          u =  (RDF_Resource) RDF_NextValue(c);
        }
        (*callBack)(obj, "</tr>");
      }
      (*callBack)(obj, "</table>");
    }

    RDF_DisposeCursor(c);
    (*callBack)(obj, "<hr><UL>");
    c = RDF_GetTargets(0, node, link, RDF_RESOURCE_TYPE);
    u = (RDF_Resource) RDF_NextValue(c);
    while (u) {
      describeItem(callBack, obj, u);
      u =  (RDF_Resource) RDF_NextValue(c);
    }
    (*callBack)(obj, "</UL>");
    RDF_DisposeCursor(c);

    c = RDF_GetTargets(0, node, newsGroup, RDF_RESOURCE_TYPE);
    u = (RDF_Resource) RDF_NextValue(c);
    if (u) {
      (*callBack)(obj, "<hr><b>NewsGroups:</B>");      
      while (u) {
        char* id = RDF_ResourceID(u);
        sprintf(buff, "<li><a href=\"%s\">%s</a>", id, id);
        (*callBack)(obj, buff);
        u =  (RDF_Resource) RDF_NextValue(c);
      }
      (*callBack)(obj, "</UL>");
    }
    RDF_DisposeCursor(c);


    c = RDF_GetTargets(0, node, editor, RDF_RESOURCE_TYPE);
    u = (RDF_Resource) RDF_NextValue(c);
    if (u) {
      (*callBack)(obj, "<hr><b>Editors:</b>");
      while (u) {
        char* id = RDF_ResourceID(u);
        sprintf(buff, "%s, ", id);
        (*callBack)(obj, buff);
        u =  (RDF_Resource) RDF_NextValue(c);
      }

      RDF_DisposeCursor(c);
    }
  }

}

void 
AnswerSearchQuery (WriteClientProc callBack, void* obj, char *query) {
    RDF_Cursor c = RDFGS_Search(0, query, 0);
    RDF_Resource name = RDF_GetResource("name", 1);
    RDF_Resource u ;
	(*callBack)(obj, "<UL>");
    while (c && (u = (RDF_Resource) RDFGS_NextValue(c))) {
      describeItem(callBack, obj, u);
    }
	(*callBack)(obj, "</UL>");
}

void
describeItem (WriteClientProc callBack, void* obj, RDF_Resource u) {
  char buff[1000];
  RDF_Resource name = RDF_GetResource("name", 1);
  RDF_Resource desc = RDF_GetResource("description", 1);
  char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
  char* id = RDF_ResourceID(u);
  char* des = (char*) RDF_OnePropValue(0, u, desc, RDF_STRING_TYPE);
  sprintf(buff, "<li><a href=\"%s\">%s</a> %s %s", id, (nm ? nm : id), 
          (des ? " : " : ""), (des ? des : ""));
  (*callBack)(obj, buff);
}
    
  
  
  

