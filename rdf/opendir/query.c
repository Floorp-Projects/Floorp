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




#include "rdf-int.h"

#define SEP '|'

typedef struct _QuerySegmentStruct {
    RDF_Resource s;
    int          num;
    int          inversep;
} QuerySegmentStruct;

typedef QuerySegmentStruct* QuerySegment;

QuerySegment*
parseQuery (char* query, RDF_Resource *root, int *numSegments) {
    int len = strlen(query);
    int count ;
    char* rootURL = getMem(100);
    QuerySegment* ans;
	*numSegments = 0;
    for (count = 0; count < len; count++) {
	if (query[count] != SEP) {
	    if (!*numSegments) rootURL[count] = query[count];
	} else 
	    (*numSegments)++;
    }
    *root = getResource(rootURL, 1);
    freeMem(rootURL);
    ans =  (QuerySegment*)getMem(*numSegments * (sizeof(QuerySegment)));
    query = strchr(query, SEP)+1;
    for (count = 0; count < *numSegments; count++) {
      char *surl = getMem(100);
      int num = -1; 
      char* nquery = strchr(query, SEP) ;
      QuerySegment nq = (QuerySegment)getMem(sizeof(QuerySegmentStruct));
      if (query[0] == '!') {
		memcpy(surl, query+1, (nquery ? nquery-query-1 : strlen(query)-1));
	    nq->inversep = 1;
      } else {
	    memcpy(surl, query, (nquery ? nquery-query : strlen(query)));
      }
      if (strchr(surl, '[') && strchr(surl, ']')) {
		  int n = strchr(surl, '[') - surl;
		  surl[n] = '\0';
		  sscanf(&surl[n+1], "%i]", &num);
		  if (num == -1) num = 4096;
          nq->s = getResource(surl, 1);
          nq->num = num;
      } else {
        nq->s = getResource(surl, 1);
        nq->num = 4096;
      } 
      freeMem(surl);
      ans[count] = nq;
      if (nquery) {
        query = nquery+1;
      } else break;
    }
    return ans;
}

char**
processRDFQuery (char* query) {
    RDF_Resource root;
    int          numSegments;
    int count = 0;
    char** ans = NULL;
    QuerySegment* querySegments = parseQuery(query, &root, &numSegments);
    int          n = 0;
    QuerySegment current = NULL;
    QuerySegment prev    = NULL;
    char** currentValArray = (char**)getMem(sizeof(RDF_Resource)*2);
    char** newValArray     = (char**)getMem(4096 * sizeof(RDF_Resource));
    currentValArray[0] = (char*) root;
    for (n = 0; n < numSegments; n++) {
      QuerySegment q = querySegments[n];     
      int nc = 0;
      int ansType = ((n == numSegments-1) ? RDF_STRING_TYPE : RDF_RESOURCE_TYPE);
	  count = 0;
      for (nc = 0; currentValArray[nc] != NULL; nc++) {
	    RDF_Cursor c = getSlotValues(NULL, (RDF_Resource)currentValArray[nc], q->s, 
                                     ansType, q->inversep, 1);
	    RDF_Resource ans;
            int lc = 0;
	    while (c  && (lc++ <= q->num) && (ans = nextValue(c))) {
              newValArray[count] = (char*) ans;
              count++;
	    }
        if (c) disposeCursor(c);
      }
      freeMem(currentValArray);
      currentValArray = newValArray;
      newValArray = (char**) getMem(4096 * sizeof(RDF_Resource));
    }
    freeMem(newValArray);
    if (count > 0) {
      ans = (char**)getMem((count+1) * sizeof(char*));
      memcpy(ans, currentValArray, (count * sizeof(char*)));
    }
    freeMem(currentValArray);
    for (n = 0; n < numSegments; n++) freeMem(querySegments[n]);     
    return ans;
}
    
    
