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
    
    
