/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "Scribble.h"

#ifdef XP_PC


#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, 
                     HINSTANCE hPrevInstance, 
                     LPSTR lpszCmdLine, 
                     int nCmdShow) 
{
  int     argC = 0;
  char ** argv = NULL;
  return(CreateApplication(&argC, argv));
}

void main(int argc, char **argv)
{
  WinMain(GetModuleHandle(NULL), NULL, 0, SW_SHOW);
}

#endif

#ifdef XP_UNIX

#include "xp_mcom.h"
#include "net.h"
#include "xp_linebuf.h"
#include "mkbuf.h"

extern "C" XP_Bool ValidateDocData(MWContext *window_id) 
{ 
  printf("ValidateDocData not implemented, stubbed in widget/tests/scribble/main.cpp\n"); 
  return PR_TRUE; 
}
 
extern "C" char *fe_GetConfigDir(void) 
{
  printf("fe_GetConfigDir() not implemented, stubbed in widget/tests/scribble/main.cpp\n"); 
  return strdup("/tmp");
}

/* dist/public/xp/xp_linebuf.h */
extern "C" int XP_ReBuffer (const char *net_buffer, int32 net_buffer_size,
                 uint32 desired_buffer_size,
                 char **bufferP, uint32 *buffer_sizeP,
                 uint32 *buffer_fpP,
                 int32 (*per_buffer_fn) (char *buffer,
                                         uint32 buffer_size,
                                         void *closure),
                 void *closure) 
{ 
  printf("XP_ReBuffer not implemented, stubbed in widget/tests/scribble/main.cpp\n"); 
  return(0); 
}

int main(int argc, char **argv)
{
  int argC = argc;
  return(CreateApplication(&argC, argv));

}
#endif

#ifdef XP_MAC
int main(int argc, char **argv)
{
  int argC = argc;

  return(CreateApplication(&argC, argv));
	
	return 0;
}
#endif


