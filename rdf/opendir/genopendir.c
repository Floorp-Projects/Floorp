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

void describeItem (WriteClientProc callBack, void* obj, RDF_Resource u) ;
void describeItemSimple (WriteClientProc callBack, void* obj, RDF_Resource u) ;

RDF_Resource 
getNodeFromQuery (char* query) {
  RDF_Resource ans;
  if (!(ans = RDF_GetResource(query, 0))) {
    return RDF_GetResource("Top", 1);
  } else return ans;
}

#define ROW_WIDTH 3

#define PREFIX "<form method=get action=\"OpenDir?\"><B>Search:</B> <input size=25 name=search> <input type=submit value=search><br><BR><BR></form><div align=right><a href=\"/odoptions.html\">Personalize</a></div>"

void
SortResourceList (RDF_Resource* list, size_t n, char sortType) {
  if (sortType == 'd') {
  } else {
  }
}

void
listParents(WriteClientProc callBack, void* obj, RDF_Resource node, int depth) {
  RDF_Resource narrow = RDF_GetResource("narrow", 1);
  RDF_Resource parent = RDF_OnePropSource(0, node, narrow);
  RDF_Resource name = RDF_GetResource("name", 1);
  char* id = RDF_ResourceID(node);
  char* nm = (char*) RDF_OnePropValue(0, node, name, RDF_STRING_TYPE);
  char* cnm = strrchr(id, '/');
  char buff[500];
  if (!nm || strchr(nm, '/')) nm = (cnm ? cnm + 1 : id);
  if ((depth < 10) && parent) listParents(callBack, obj, parent, depth+1);
  sprintf(buff, "<B><a href=\"OpenDir?browse=%s\">%s</a> > </B> ", id, nm);

  (*callBack)(obj, buff); 
}

char
cookiePropValue (char* cookie, char* prop) {
  size_t len = strlen(prop);
  char*  str = strstr(cookie, prop);
  if (!str) {
    return 0;
  } else {
    return *(str + len + 1);
  }
}

void 
AnswerOpenDirQuery (WriteClientProc callBack, void* obj, char *query, char* cookie) {
  char buff[1000];
  RDF_Resource narrow = RDF_GetResource("narrow", 1);
  RDF_Resource link = RDF_GetResource("link", 1);
  RDF_Resource name = RDF_GetResource("name", 1);
  RDF_Resource topic = RDF_GetResource("Topic", 1);
  RDF_Resource desc = RDF_GetResource("description", 1);
  RDF_Resource editor = RDF_GetResource("editor", 1);
  RDF_Resource newsGroup = RDF_GetResource("newsGroup", 1);
  RDF_Resource node = getNodeFromQuery(query);
  char        widthc = cookiePropValue(cookie, "cols");
  char        sort   = cookiePropValue(cookie, "sort");
  size_t      rw      = (widthc ? ((int)widthc - 48) : 3);

  if (!sort) sort = 'D';

  if (node) {
    RDF_Cursor c = RDF_GetTargets(0, node, narrow, RDF_RESOURCE_TYPE);
    RDF_Resource u = (RDF_Resource) RDF_NextValue(c);
    (*callBack)(obj, PREFIX); 
    listParents(callBack, obj, node, 0);     
    if (u) {
      (*callBack)(obj, "<BR><hr><table cellspacing=\"6\" cellpadding=\"6\" width=\"80%\">");
      while (u) {
        int w = 0;
        (*callBack)(obj, "<tr>");
        while ((w < rw) && u) {
          char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
          char* id = RDF_ResourceID(u);
          if (!nm || strchr(nm, '/')) nm = strrchr(id, '/') +1;
          sprintf(buff, "<td width=\"%i%\"><li><a href=\"OpenDir?browse=%s\">%s</a></td>",
                  (100 / rw), id, (nm ? nm : id));
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
        sprintf(buff, " %s", id);
        (*callBack)(obj, buff);
        u =  (RDF_Resource) RDF_NextValue(c);
      }

      RDF_DisposeCursor(c);
    }
  }

}


void describeCategory (WriteClientProc callBack, void* obj, RDF_Resource u) {
  char buff[1000];
  sprintf(buff, "<li><B><a href=\"OpenDir?browse=%s\">%s</a></B>", RDF_ResourceID(u),
          RDF_ResourceID(u));
  (*callBack)(obj, buff);
}

#define MRN 1000

char* 
xGetMem (size_t n) {
  char* ans = (char*) malloc(n);
  if (ans) memset(ans, '\0', n);
  return ans;
}


void 
xFreeMem(void* item) {
  free(item);
}

RDF_Resource
stToProp (char c) {
  if (!c || (c == 'a')) {
    return 0;
  } else if (c == 'n') {
    return RDF_GetResource("name", 1);
  } else if (c == 'd') {
    return RDF_GetResource("description", 1);
  } else return 0;
}

void 
AnswerSearchQuery (WriteClientProc callBack, void* obj, char *query, char* cookie) {
    char      st = cookiePropValue(cookie, "searchTarget");
    RDF_Cursor c = RDFGS_Search(0, query, stToProp(st));
    RDF_Resource name = RDF_GetResource("name", 1);
    RDF_Resource topic = RDF_GetResource("Topic", 1);
    RDF_Resource type = RDF_GetResource("type", 1);
    RDF_Resource u ;
    RDF_Resource* cats = (RDF_Resource*)xGetMem(sizeof(RDF_Resource) * (MRN + 1));
    RDF_Resource* items =  (RDF_Resource*)xGetMem(sizeof(RDF_Resource) * (MRN + 1));;
    size_t catn = 0;
    size_t itemn = 0;    
    size_t index;
    char showDesc = cookiePropValue(cookie, "searchDesc");

    (*callBack)(obj, PREFIX);
    (*callBack)(obj, "<B>Search : ");
    (*callBack)(obj, query);
    (*callBack)(obj, "</B>");

    while ((u = (RDF_Resource) RDFGS_NextValue(c)) && 
           (itemn < MRN-1) && (catn < MRN-1)) {
      if (RDF_HasAssertion(0, u, type, topic, RDF_RESOURCE_TYPE)) {
        cats[catn++] = u;  
      } else {
        items[itemn++] = u;  
      }
    }

    RDFGS_DisposeCursor(c);
    if (catn > 0) {            
      (*callBack)(obj, "<HR><B>Matching Categories :</B><BR>");
      (*callBack)(obj, "<UL>");
      for (index = 0; index < catn ; index++) {
        if (cats[index]) describeCategory(callBack, obj, cats[index]);
      }
      (*callBack)(obj, "</UL>");
    }

    if (itemn > 0) {
      (*callBack)(obj, "<HR><B>Matching Links :</B><BR><UL>");
      for (index = 0; index < itemn ; index++) {
        if (items[index]) {
          if (showDesc == 'n') {
            describeItemSimple(callBack, obj, items[index]);
          } else {
            describeItem(callBack, obj, items[index]);
          }
        } 
      }
      (*callBack)(obj, "</UL>");      
    }

   xFreeMem(items);
   xFreeMem(cats);
}


void
describeItemSimple (WriteClientProc callBack, void* obj, RDF_Resource u) {
  char buff[5000];
  RDF_Resource name = RDF_GetResource("name", 1);
  char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
  char* id = RDF_ResourceID(u);
  sprintf(buff, "<li><a href=\"%s\">%s</a>", id, (nm ? nm : id));
  (*callBack)(obj, buff);
}

void
describeItem (WriteClientProc callBack, void* obj, RDF_Resource u) {
  char buff[5000];
  RDF_Resource name = RDF_GetResource("name", 1);
  RDF_Resource desc = RDF_GetResource("description", 1);
  char* nm = (char*) RDF_OnePropValue(0, u, name, RDF_STRING_TYPE);
  char* id = RDF_ResourceID(u);
  char* des = (char*) RDF_OnePropValue(0, u, desc, RDF_STRING_TYPE);
  sprintf(buff, "<li><a href=\"%s\">%s</a> %s %s", id, (nm ? nm : id), 
          (des ? " : " : ""), (des ? des : ""));
  (*callBack)(obj, buff);
}
    
  
  
  





