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




#include "rdf.h"
#include <stdlib.h>
#include <stdio.h>

void  main () {
  char* nextQuery = malloc(200);
  memset(nextQuery, '\0', 200);
  RDF_Initialize();
  while (1) {
    int n;
    int startsWith(const char* pattern, const char* item); 
    nextQuery = gets(nextQuery);
    if (nextQuery) {
      if (startsWith("read", nextQuery)) {        
        FILE* f = fopen(&nextQuery[5], "r");	
        if (f) {
	  char* buff  = malloc(100 * 1024);
          int len ;
	  memset(buff, '\0', (100 * 1024));
	  len = fread(buff, 1, (100 * 1024) -1, f);
          buff[len] = '\0';
          if (!RDF_Consume(&nextQuery[5], buff, len)) {
             printf("Error in RDF File\n");
          } else {
			  printf("Finished reading %s\n", &nextQuery[5]);
		  }
          free(buff);
        } else {
          printf("could not open %s\n", &nextQuery[5]);
        }        
      } else if (startsWith("rdf", nextQuery)) {
        char** ans = RDF_processPathQuery(&nextQuery[4]);
		printf("\n");
        if (ans) {
          for (n = 0; ans[n] != 0; n++) {
            printf("(%i) %s\n",n, ans[n]);
          } 
	} else {
	  printf("No answers\n");
	}
	free(ans);
      } else break;   
    }
  }
}
