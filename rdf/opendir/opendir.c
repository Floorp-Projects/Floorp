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
#include <sys/types.h>
#include "ONEiiop.h"
#include "rdf.h"

#if !defined(WIN32) 
extern "C" int gethostname(char *name, int namelen); 
#endif

void
WriteClient (void* obj, char* buffer) {
  size_t len = strlen(buffer);
  WAIWriteClient((ServerSession_t)obj, (const unsigned char *) buffer, len);
}

void AnswerOpenDirQuery(WriteClientProc callBack, void* obj, char* query, char* cookie);
void  AnswerSearchQuery (WriteClientProc callBack, void* obj, char *query, char* cookie) ;

#define PREFIX "<html><body><a href=\"/\"><center><img src=\"http://directory.mozilla.org/img/opendir.gif\" width=396 height=79 border=\"0\"></center></a>"

#define POSTFIX "</body><html>"

long
Run(ServerSession_t obj)
{
  char* query = (char*) malloc(300);
  char* val;
  char* cookie;
  WAIgetRequestInfo(obj, "QUERY",  &query);  
  WAIgetCookie(obj, &cookie); 

  if (!query) return 0;
  printf("Query = %s. Cookie = %s\n", query, cookie);
  val = strchr(query, '=');
  if (!val) return 0;
  val++;
  *(val -1) = '\0';
  WAIStartResponse(obj);
  /*  WriteClient(obj, PREFIX); */
  
  if (strcmp(query, "browse") == 0) {
    AnswerOpenDirQuery(WriteClient, obj, val, cookie);
  } else if (strcmp(query, "search") == 0) {
    AnswerSearchQuery(WriteClient, obj, val, cookie);
  } else if (strcmp(query, "query") == 0) {
    char** ans = processRDFQuery(val);
    if (ans) {
      int n = 0;
      for (n = 0; ans[n] != 0; n++) {
        WriteClient(obj, ans[n]);
      }
    }
  }
  /*  WriteClient(obj, POSTFIX); */
  return 0;
}


#if defined(WIN32)
int WINAPI WinMain(
    HINSTANCE hInstance,	// handle to current instance
    HINSTANCE hPrevInstance,	// handle to previous instance
    LPSTR lpCmdLine,	// pointer to command line
    int nCmdShow 	// show state of window
   )

{
  int argc = 0;
  char **argv = NULL;
#else
  int main(int argc, char **argv)
    {
#endif
      char localhost[256];
      char *host;
      RDF_Resource u;
      int n = 2;
      IIOPWebAppService_t obj;
      
      //
	// Normally we expect to see a hostname:port as
             // our one and only argument.  If we are started by
             // the OAD we will see an extra argument which should
             // be ignored.
             // The recommended registration with the OAD should 
	// specify the hostname:port as an argument to the
             // object provider.
             //
             // So try to ferrit out the hostname:port
#ifndef WIN32
             // Easier if we just have a normal main.
                               if (argc > 1){
                                 if (argc == 1)
                                   host = argv[1];
                                 else if (*argv[1] != '-')
			host = argv[1];

#else
	// Windows command line drops the program name
	// that appears as argv[0].
	// GetCommandLine can be used to retrieve the
	// whole thing.
        n = 1;
	if (lpCmdLine && *lpCmdLine){
		if (*lpCmdLine != '-'){
			char *sep = strchr(lpCmdLine, ' ');
			if (sep)
				*sep = '\0';
			host = lpCmdLine;
		}
#endif
	}else{
		gethostname(localhost, sizeof(localhost));
		host = localhost;
	}

	//
	// Pass in argc/argv if we have 'em.
#ifndef WIN32
	obj = WAIcreateWebAppService("OpenDir", Run, argc, argv);
#else
	/* obj = WAIcreateWebAppService("OpenDir", Run, 0, 0); */
#endif
/*	WAIregisterService(obj, host); */
        RDF_Initialize();
        printf("RDF Initialized!\n");
        if (argc == 0) {
        char** ans;
        RDF_ReadFile("s1");
        RDF_ReadFile("s2");
		} else {
        while (n < argc) {
           char* name = argv[n++];
             printf("Reading %s\n", name);
             RDF_ReadFile(name);
       }
		}
        printf("done");

	WAIimplIsReady();
    
	return 0;
}
 

