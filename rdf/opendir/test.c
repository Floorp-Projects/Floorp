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
