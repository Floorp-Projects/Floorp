/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rdf.h"
#include "gs.h"

void describeItem (WriteClientProc callBack, void* obj, RDF_Resource u) ;
void describeItemSimple (WriteClientProc callBack, void* obj, RDF_Resource u) ;
extern int startsWith(const char* prefix, const char* pattern);


RDF_Resource    gNarrow;
RDF_Resource    gLink; 
RDF_Resource    gName; 
RDF_Resource    gTopic;
RDF_Resource    gDesc; 
RDF_Resource    gEditor; 
RDF_Resource    gNewsGroup;
char*           gTemplate;
int gInited = 0;
#define MAX_TEMPLATE_SIZE 20000

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

  char* id = RDF_ResourceID(node);
  char* nm = (char*) RDF_OnePropValue(0, node, gName, RDF_STRING_TYPE);
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
Init () {
  if (!gInited) {
    int n = 0;
    FILE *templateFile = fopen("template.html", "r");
    if (!templateFile) printf("Could not open template!\n");
    gInited = 1;    
    gNarrow = RDF_GetResource("narrow", 1);
    gLink = RDF_GetResource("link", 1);
    gName = RDF_GetResource("Title", 1);
    gTopic = RDF_GetResource("Topic", 1);
    gDesc = RDF_GetResource("Description", 1);
    gEditor = RDF_GetResource("editor", 1);
    gNewsGroup = RDF_GetResource("newsGroup", 1);
    gTemplate = (char*) malloc(MAX_TEMPLATE_SIZE+1);
    memset(gTemplate, '\0', MAX_TEMPLATE_SIZE);
    n = fread(gTemplate, 1, MAX_TEMPLATE_SIZE, templateFile);
    gTemplate[n] = '\0';
    fclose(templateFile);
  }
}

char*
resourceName (RDF_Resource u) {
  char* nm = (char*) RDF_OnePropValue(0, u, gName, RDF_STRING_TYPE);
  char* id = RDF_ResourceID(u);
  if (!nm || strchr(nm, '/')) nm = strrchr(id, '/') +1;
  return nm;
}

void
outputTopicName (WriteClientProc callBack, void* obj, RDF_Resource node) {
  char* nm = (char*) RDF_OnePropValue(0, node, gName, RDF_STRING_TYPE);
  char* id = RDF_ResourceID(node);

  if (!nm || strchr(nm, '/')) 
	  if (strchr(id, '/')) {
		  nm = strrchr(id, '/') +1;
	  } else {
		  nm = id;
	  }

  (*callBack)(obj, nm);
}
  
void
listSubTopics (WriteClientProc callBack, void* obj, RDF_Resource node, char* cookie, RDF_Resource p) {
  if (node) {
    RDF_Cursor c = RDF_GetTargets(0, node, p, RDF_RESOURCE_TYPE);
    char        widthc = cookiePropValue(cookie, "cols");
    RDF_Resource u;
    char buff[5000];
    RDF_Resource subTopicList[200];
    int n = 0;
    int w = 0;
    while (u =  (RDF_Resource) RDF_NextValue(c)) {
      subTopicList[n++] = u;
    }
    for (w = 0; w < n; w++) {
      char* id;
      if (((w == 0) || (w == ((n+1)/2))) && (p == gNarrow)) {
        if (w != 0)  (*callBack)(obj, "</td>");
        (*callBack)(obj, "</UL>\n<TD width=\"40%\" valign=top><UL><FONT Face=\"sans-serif, Arial, Helvetica\">\n<UL>\n");
      }
      u = subTopicList[n-w-1];
      id = RDF_ResourceID(u);
      if (p == gNarrow) {
        sprintf(buff, "<li><a href=\"OpenDir?browse=%s\"><B>%s</B></a>\n", id, resourceName(u));
      } else {
        char* des = (char*) RDF_OnePropValue(0, u, gDesc, RDF_STRING_TYPE);
        sprintf(buff, "<li><a href=\"%s\">%s</a> %s %s\n", id, resourceName(u), 
                (des ? " : " : ""), (des ? des : ""));
      }        
      (*callBack)(obj, buff);
    }
  }
}

void 
AnswerOpenDirQuery (WriteClientProc callBack, void* obj, char *query, char* cookie) {
  RDF_Resource node = getNodeFromQuery(query);
  size_t n = 0;
  char buff[MAX_TEMPLATE_SIZE];
  char* pbegin = 0;
  char* begin;  
  Init();
  begin = gTemplate;
  while (pbegin = strstr(begin, "&Topic")) {
    memset(buff, '\0', 10000);
    memcpy(buff, begin, pbegin-begin);
    (*callBack)(obj, buff);
    if (startsWith("&TopicName;", pbegin)) {
      outputTopicName(callBack, obj, node);
    } else if (startsWith("&TopicPath;", pbegin)) {
      listParents(callBack, obj, node, 0);
    } else if (startsWith("&TopicSubTopics;", pbegin)) {
      listSubTopics(callBack, obj, node, cookie, gNarrow);
    } else if (startsWith("&TopicItems;", pbegin)) {
      listSubTopics(callBack, obj, node, cookie, gLink);
    } else if (startsWith("&TopicAddURL;", pbegin)) {
      (*callBack)(obj, "http://directory.mozilla.org/cgi-bin/add.cgi?where=");
      (*callBack)(obj, RDF_ResourceID(node));
    } else if (startsWith("&TopicBecomeEditor;", pbegin)) {
      (*callBack)(obj, "http://directory.mozilla.org/cgi-bin/apply.cgi?where=");
      (*callBack)(obj, RDF_ResourceID(node));
    }
    begin = pbegin + (strchr(pbegin, ';') - pbegin) + 1; 

  }
  (*callBack)(obj, begin);
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
    return gName;
  } else if (c == 'd') {
    return gDesc;
  } else return 0;
}

void 
AnswerSearchQuery (WriteClientProc callBack, void* obj, char *query, char* cookie) {
    char      st = cookiePropValue(cookie, "searchTarget");
    RDF_Cursor c = RDFGS_Search(0, query, stToProp(st));
    RDF_Resource topic = RDF_GetResource("Topic", 1);
    RDF_Resource type = RDF_GetResource("type", 1);
    RDF_Resource u ;
    size_t catn = 0;
    size_t itemn = 0;    
    size_t index;
    RDF_Resource* cats = (RDF_Resource*)xGetMem(sizeof(RDF_Resource) * (MRN + 1));
    RDF_Resource* items =  (RDF_Resource*)xGetMem(sizeof(RDF_Resource) * (MRN + 1));
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
  char* nm = (char*) RDF_OnePropValue(0, u, gName, RDF_STRING_TYPE);
  char* id = RDF_ResourceID(u);
  sprintf(buff, "<li><a href=\"%s\">%s</a>", id, (nm ? nm : id));
  (*callBack)(obj, buff);
}

void
describeItem (WriteClientProc callBack, void* obj, RDF_Resource u) {
  char buff[5000];
  char* nm = (char*) RDF_OnePropValue(0, u, gName, RDF_STRING_TYPE);
  char* id = RDF_ResourceID(u);
  char* des = (char*) RDF_OnePropValue(0, u, gDesc, RDF_STRING_TYPE);
  sprintf(buff, "<li><a href=\"%s\">%s</a> %s %s", id, (nm ? nm : id), 
          (des ? " : " : ""), (des ? des : ""));
  (*callBack)(obj, buff);
}
    
  
  
  





