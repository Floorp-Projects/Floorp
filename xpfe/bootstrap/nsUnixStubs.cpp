/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "xp_mcom.h"
#include "net.h"
#include "xp_linebuf.h"
#include "mkbuf.h"

#ifndef MOZ_USER_DIR
#define MOZ_USER_DIR ".mozilla"
#endif

extern "C" XP_Bool ValidateDocData(MWContext *window_id) 
{ 
  printf("ValidateDocData not implemented, stubbed in webshell/tests/viewer/nsStubs.cpp\n"); 
  return PR_TRUE; 
} 

extern "C" char *fe_GetConfigDir(void) {
  char *home = getenv("HOME");
  if (home) {
    int len = strlen(home);
    len += strlen("/") + strlen(MOZ_USER_DIR) + 1;

    char* config_dir = (char *)XP_CALLOC(len, sizeof(char));
    // we really should use XP_STRN*_SAFE but this is MODULAR_NETLIB
    XP_STRCPY(config_dir, home);
    XP_STRCAT(config_dir, "/");
    XP_STRCAT(config_dir, MOZ_USER_DIR); 
    return config_dir;
  }
  
  return strdup("/tmp");
}
