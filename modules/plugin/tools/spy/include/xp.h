/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef __XP_H__
#define __XP_H__

/*****************************************************/

#ifdef XP_WIN

  #include <windows.h>

#endif

/*****************************************************/


#ifdef XP_MAC
 	#include <all.h>
#endif
	
#if (defined XP_MAC || defined XP_UNIX)

	typedef unsigned char	BYTE;
	
	#define strcmpi  strcmp

#endif


/*****************************************************/

#ifdef XP_UNIX
	#include <stdio.h>
	#include <string.h>       /* strcat() */
  #include <sys/types.h>    /* stat() */
  #include <sys/stat.h>
  #include <stdlib.h>       /* atoi() */
  #include <assert.h>       /* assert() */
  #include <ctype.h>        /* isprint() */

	#ifndef FALSE
	#define FALSE			  false
	#endif
	  
	#ifndef TRUE
	#define TRUE			  true
	#endif

  #ifndef HIBYTE
  #define HIBYTE(x)       ((((u_int)(x)) & 0xff00)>>8)
  #endif

  #ifndef LOBYTE
  #define LOBYTE(x)       (((u_int)(x)) & 0x00ff)
  #endif

  #ifndef XP_EXPORT_API
  #define XP_EXPORT_API
  #endif

#endif

/*****************************************************/

#ifdef XP_WIN

  #define XP_HFILE        HFILE
  #define XP_HLIB         HINSTANCE

  #define XP_EXPORT_API   WINAPI

#else

  #define XP_HFILE        FILE*

  #ifndef DWORD
  #define DWORD           unsigned long
  #endif

  #ifndef UINT
  #define UINT            unsigned int
  #endif

  #ifndef WORD
  #define WORD            unsigned short
  #endif

  #ifndef BOOL
  #define BOOL		  bool
  #endif

  #if (defined XP_UNIX)			/* For IRIX */
    #ifndef bool
    #define bool		  unsigned char
    #endif
  #endif

#endif



/*****************************************************/

#ifndef _MAX_PATH
  #define _MAX_PATH       256
#endif

/*****************************************************/

// File utilities
BOOL XP_IsFile(char * szFileName);
XP_HFILE XP_CreateFile(char * szFileName);
XP_HFILE XP_OpenFile(char * szFileName);
void XP_CloseFile(XP_HFILE hFile);
void XP_DeleteFile(char * szFileName);
DWORD XP_WriteFile(XP_HFILE hFile, void * pBuf, int iSize);
void XP_FlushFileBuffers(XP_HFILE hFile);


// misc
void * XP_GetSymbol(XP_HLIB hLib, char * szProcName);

#endif
