/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2
-*- 
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

#ifdef XP_PC
#include "windows.h"
#endif

#include "nscore.h"
#include "nsString.h"
#include "nsUrlParser.h"
#include "nspr.h"
#include "plstr.h"


nsUrlParser::nsUrlParser(nsString& sCurl)
{
  m_sURL = sCurl;
}

nsUrlParser::nsUrlParser(char* psCurl)
{
  m_sURL = psCurl;
}

nsUrlParser::~nsUrlParser()
{
}

/*
 * Assumes full path passed in.  This is to differentiate from
 * file: URL's
 */

PRBool nsUrlParser::IsLocalFile()
{
  PRBool bLocal = PR_FALSE;

  char * pszFile = (char *) m_sURL.ToNewCString();

  if (pszFile != nsnull)
  {

/// XP_MAC?
#ifdef XP_UNIX
    if (PL_strlen(pszFile) > 0)
    {
      if (pszFile[0] == '/')
        bLocal = PR_TRUE;
    }
#else
    if (PL_strlen(pszFile) > 2)
    {
      if ((pszFile[1] == ':') && (pszFile[2] == '\\'))
        bLocal = PR_TRUE;
    }
#endif

    delete pszFile;
  }

  return bLocal;
}

/*
 * this method converts a "file://" or "resource://" URL
 * to a platform specific file handle.  It is the caller's
 * responsibility to free ther returned string using 
 * PR_Free().
 *
 * This code was lifted partly from Live3D and Netlib (nsURL.cpp).
 * May the Lord above have mercy on my soul.....
 *
 */

char * nsUrlParser::ToLocalFile()
{
  char * pszFile = nsnull;

  char * pszURL = (char *) m_sURL.ToNewCString();


  /*
   * Check to see it is a local file already
   */

  if (IsLocalFile() == PR_TRUE)
    return pszURL;

  if (PL_strncasecmp(pszURL, "resource://",11) == 0)
  {
    pszFile = ResourceToFile();

  } else {

    pszFile = (char *)PR_Malloc(PL_strlen(pszURL));

    pszFile[0] = '\0' ;

    if (PL_strncasecmp(pszURL, "file://", 7) == 0)
    {
      char *pSrc = pszURL + 7 ;

      if (PL_strncasecmp(pSrc, "//", 2) == 0)              // file:////<hostname>
      {
        PL_strcpy(pszFile, pSrc) ;
      }
      else
      {
        if (PL_strncasecmp(pSrc, "localhost/", 10) == 0)      // file://localhost/
        {
          pSrc += 10 ;  // Go past the '/' after "localhost"
        }
        else
        {
          if (*pSrc == '/')                           // file:///
            pSrc++ ;
        }

        if (*(pSrc+1) == '|') // We got a drive letter
        {
          PL_strcpy(pszFile, pSrc) ;
          pszFile[1] = ':' ;
        }
        else
        {
  #ifdef XP_UNIX
          pszFile[0] = '/' ;
          PL_strcpy(pszFile+1, pSrc) ;
  #else
          pszFile[0] = 'C' ;
          pszFile[1] = ':' ;
          pszFile[2] = '/' ;
          PL_strcpy(pszFile+3, pSrc) ;
  #endif
        }
      }
  #ifndef XP_UNIX
      char *pDest = pszFile ;
      while(*pDest != '\0')              // translate '/' to '\' -> PC stuff
      {
          if (*pDest == '/')
              *pDest = '\\' ;
          pDest++ ;
      }
  #endif
    }
  }

  delete pszURL;

  return pszFile;
}

char * nsUrlParser::ResourceToFile()
{

  char * aResourceFileName = (char *) m_sURL.ToNewCString();
  // XXX For now, resources are not in jar files 
  // Find base path name to the resource file
  char* resourceBase;

#ifdef XP_PC
  char * cp;
  // XXX For now, all resources are relative to the .exe file
  resourceBase = (char *)PR_Malloc(_MAX_PATH);;
  DWORD mfnLen = GetModuleFileName(NULL, resourceBase, _MAX_PATH);
  // Truncate the executable name from the rest of the path...
  cp = strrchr(resourceBase, '\\');
  if (nsnull != cp) {
    *cp = '\0';
  }
  // Change the first ':' into a '|'
  cp = PL_strchr(resourceBase, ':');
  if (nsnull != cp) {
      *cp = '|';
  }
#endif

#ifdef XP_UNIX
  // XXX For now, all resources are relative to the current working directory

    FILE *pp;

#define MAXPATHLEN 2000

    resourceBase = (char *)PR_Malloc(MAXPATHLEN);;

    if (!(pp = popen("pwd", "r"))) {
      printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 1\n");
      return(nsnull);
    }
    else {
      if (fgets(resourceBase, MAXPATHLEN, pp)) {
        printf("[%s] %d\n", resourceBase, PL_strlen(resourceBase));
        resourceBase[PL_strlen(resourceBase)-1] = 0;
      }
      else {
       printf("RESOURCE protocol error in nsURL::mangeResourceIntoFileURL 2\n");
       return(nsnull);
      }
   }

   printf("RESOURCE name %s\n", resourceBase);
#endif

  // Join base path to resource name
  if (aResourceFileName[0] == '/') {
    aResourceFileName++;
  }
  PRInt32 baseLen = PL_strlen(resourceBase);
  PRInt32 resLen = PL_strlen(aResourceFileName);
  PRInt32 totalLen = 8 + baseLen + 1 + resLen + 1;
  char* fileName = (char *)PR_Malloc(totalLen);
  PR_snprintf(fileName, totalLen, "file:///%s/%s", resourceBase, aResourceFileName+11);

#ifdef XP_PC
  // Change any backslashes into foreward slashes...
  while ((cp = PL_strchr(fileName, '\\')) != 0) {
    *cp = '/';
    cp++;
  }
#endif

  PR_Free(resourceBase);
  
  nsString oldString = m_sURL ;
  m_sURL = fileName ;

  char *localFile = ToLocalFile();

  m_sURL = oldString;

  PR_Free(fileName);
  delete aResourceFileName;

  return localFile;
}


#define MAX_URL 1024

char * nsUrlParser::LocalFileToURL()
{
  char * pszFile = (char *) m_sURL.ToNewCString();
  
  char * pszURL = (char *)PR_Malloc(MAX_URL);

  pszURL[0] = '\0';

#ifdef XP_PC

  if (IsLocalFile() == PR_TRUE)
  {

    char    szDrive[5], szDir[255], szFile[10], szExt[5] ;
    
    PL_strcpy(pszURL, "file:///") ;

    _splitpath(pszFile, szDrive, szDir, szFile, szExt) ;
           
    if (szDrive[0] != '\0')
    {
        PL_strcat(pszURL, szDrive) ;

        pszURL[PL_strlen(pszURL)-1] = '|' ;
    }

    char *pDest = szDir ;

    while(*pDest != '\0')              // translate '\' to '/' -> PC stuff
    {
        if (*pDest == '\\')
            *pDest = '/' ;      
        pDest++ ;
    }                  
    
    PL_strcat(pszURL, szDir) ;
    PL_strcat(pszURL, szFile) ;
    PL_strcat(pszURL, szExt) ;

  }
  else
  {
  // ***************************************************************
  // Need to handle file spec of "\\host\c\vrml\bobo.wrl"
  // and convert it to           "file:////host/c/vrml/bobo.wrl"

      if ((pszFile[0] == '\\') && (pszFile[1] == '\\'))
      {
          PL_strcpy(pszURL, "file://") ;
          PL_strcat(pszURL, pszFile) ;

          char *pDest = pszURL ;

          while(*pDest != '\0')              // translate '\' to '/' -> PC stuff
          {
              if (*pDest == '\\')
                  *pDest = '/' ;      
              pDest++ ;
          }                  
      }
      else
      {
          // TODO: Should check that pszFile is a valid HTTP here!
          PL_strcpy(pszURL, pszFile) ;
      }
  }

#endif

  delete pszFile;

  return pszURL;
}
