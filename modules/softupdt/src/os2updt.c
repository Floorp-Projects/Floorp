/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#define INCL_DOSERRORS
#define INCL_DOSMISC
#define INCL_DOSFILEMGR
#define INCL_DOSMEMMGR
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_KBD
#define INCL_VIO

#define CR                       (CHAR) '\r'
#define LF                       (CHAR) '\n'
#define CRLF                     "\r\n"
#define MAXPATHLEN     260
#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <bsememf.h>
#include <string.h>


#include "os2updt.h"                  /* Various declares                 */

// used for input to logical disk Get device parms Ioctl
struct {
        UCHAR Infotype;
        UCHAR DriveUnit;
       } DriveRequest;

ULONG AppendBufferToFile(char *pszFileName,char **pszBuffer);


/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: CheckHPFS                                               º */
/* º                                                                      º */
/* º    Checks to see is given drive is HPFS                              º */
/* º                                                                      º */
/* º    Created: 7.23.97 by Lawrence Hsieh                                º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
BOOL CheckHPFS (PSZ pszBootLetter) {

  PSZ psz;
  CHAR szLine[10];
  BYTE         fsqBuffer[sizeof(FSQBUFFER2) + (3 * CCHMAXPATH)] = {0};
  ULONG        cbBuffer   = sizeof(fsqBuffer);        /* Buffer length */
  PFSQBUFFER2  pfsqBuffer = (PFSQBUFFER2) &fsqBuffer;
  BOOL rc=FALSE;

  rc = DosQueryFSAttach(pszBootLetter,
                      (ULONG)1,
                      FSAIL_QUERYNAME,
                      pfsqBuffer,
                      &cbBuffer);
                    
  psz = (pfsqBuffer->szName);
  psz = psz + pfsqBuffer->cbName + 1;
  strcpy(szLine, psz);
  if (!strcmpi(szLine, "HPFS"))
  {
    rc=TRUE;
  }

  return rc;
}

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: CheckValidFileName                                      º */
/* º                                                                      º */
/* º    Writes the DEVICE= and RUN= entry into config.sys to enable the   º */
/* º    lock file device driver: ibmlanlk.exe & ibmlanlk.sys on reboot    º */
/* º                                                                      º */
/* º    Created: 7.23.97 by Lawrence Hsieh                                º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
BOOL CheckValidFileName (PSZ pszFilename) {
PSZ pszStartFilename=NULL, pszDotLocation=NULL;
  ULONG t;
  BOOL rc = FALSE;
 
  pszStartFilename = strrchr(pszFilename, '\\');
  if (pszStartFilename == NULL) {
     pszStartFilename = strrchr(pszFilename, ':');
     if (pszStartFilename == NULL) {
       pszStartFilename = pszFilename;
     } else
       pszStartFilename++;
  } else
      pszStartFilename++;

  pszDotLocation = strchr(pszStartFilename,'.');

  if (pszDotLocation == NULL) {
     pszDotLocation = pszStartFilename + strlen(pszStartFilename);
  }

  if (pszDotLocation != NULL) {
  
    if ((pszDotLocation-pszStartFilename) <=8) 
    {
       if (strlen(pszDotLocation) == 0) 
          rc=TRUE;
       else
         if ((strlen(pszDotLocation) - 1) <= 3) 
            rc = TRUE;
    }
  }
  return rc;
}

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: WriteLockFileDDToConfig                                 º */
/* º                                                                      º */
/* º    Writes the DEVICE= and RUN= entry into config.sys to enable the   º */
/* º    lock file device driver: ibmlanlk.exe & ibmlanlk.sys on reboot    º */
/* º                                                                      º */
/* º    Created: 7.23.97 by Lawrence Hsieh                                º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
ULONG WriteLockFileDDToConfig (PSZ pszListFile)
{

  CHAR   szUpdate[512];
  CHAR   szTempBuff[512];
  CHAR   szQuery[75];
  ULONG  usSize, usRetCode;
  PSZ    pszCsys;
  CHAR   szFile[20];
  ULONG ulBootDrive = 0;
  UINT uLen = 0;
  char InstallPath[16];
  char BootLetter[2];
//  UINT  cchInstallPath;
  CHAR   *pszPos;

  // Make sure we have a valid buffer.

    // Get the boot drive.
  if (DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                     (PVOID)&ulBootDrive, sizeof(ULONG)) == NO_ERROR)
  {
     BootLetter[0] = ulBootDrive + 'A' -1;
     InstallPath[0]=BootLetter[0];
     BootLetter[1] = 0;
     strcpy(InstallPath+1, ":\\OS2\\INSTALL");
     uLen = strlen(InstallPath);
  }
  else
  {
     strcpy(InstallPath, "");
     uLen = 0;
  }
  if (uLen == 0) {
    return(ERROR_INVALID_FUNCTION);
  }

  szFile[0] = 0;
  strcpy(szFile, BootLetter);
  strcat(szFile, ":\\CONFIG.SYS");

  usRetCode = ReadFileToBuffer(szFile,
                               &pszCsys,
                               &usSize);
  if (usRetCode == NO_ERROR) {
    strcpy(szUpdate, BootLetter);
    strcat(szUpdate, ":\\OS2\\INSTALL\\IBMLANLK.SYS ");
    strcat(szUpdate, BootLetter);
    strcpy(szQuery,szUpdate);
    strcat(szQuery, ":\\OS2\\INSTALL\\");
    strcat(szQuery, pszListFile);

    strcat(szUpdate, ":\\OS2\\INSTALL\\");
    strcat(szUpdate, pszListFile);
    strcat(szUpdate, "\r\nRUN=");
    strcat(szUpdate, BootLetter);
    strcat(szUpdate, ":\\OS2\\INSTALL\\IBMLANLK.EXE ");
    strcat(szUpdate, BootLetter);
    strcat(szUpdate, ":\\OS2\\INSTALL\\");
    strcat(szUpdate, pszListFile);

    if ((CsysQuery (pszCsys, CSYS_DEVICE, szQuery,szTempBuff))==NULL) {
      pszPos = CsysUpdate(pszCsys,                      /* Insert at first */
                          CSYS_DEVICE,                  /* Device statement*/ 
                          NULL,
                          szUpdate,
                          CSYS_FIRST_TYPE);             /*Insert at first */
                                                        /*similar type   */

      usRetCode = WriteBufferToFile (szFile, &pszCsys); /* write out config.sys*/
    }
  } 
  else
     return (usRetCode);
  return (usRetCode);
} //WriteLockFileDDToConfig

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: WriteLockFileRenameEntry                                º */
/* º                                                                      º */
/* º    Appends to the listfile entry to copy and delete on reboot        º */
/* º    using the lock file device driver: ibmlanlk.exe & ibmlanlk.sys    º */
/* º                                                                      º */
/* º    Created: 7.23.97 by Lawrence Hsieh                                º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
ULONG WriteLockFileRenameEntry(PSZ pszFinal, PSZ pszCurrent, PSZ pszListFile)
//inserts a copy and erase entry inside the list file for lock file 
//device driver
{
  PSZ    pszList;
  PSZ    pszReadTestDummyBuffer;
  ULONG  usSize, usRetCode = NO_ERROR;
  static BOOL bFirstTime  = TRUE;
  CHAR szBootLetter[3]; 
  PSZ pszBootLetter=NULL;
  BOOL flag;
  ULONG ulBootDrive = 0;
  UINT uLen = 0;
  char fullPathnameListFile[30];
  char BootLetter[2];

  // Check if pszFinal name is in the valid format (8.3)
  // if not writing to HPFS
  pszBootLetter = strchr(pszFinal,':');
  if (pszBootLetter) {
    szBootLetter[0]= pszFinal[0];
    szBootLetter[1]= pszFinal[1];
    szBootLetter[2]= 0;
  }  else
     return(ERROR_INVALID_DRIVE);

  if (!CheckHPFS(szBootLetter)){
    if (CheckValidFileName(pszFinal)) 
       usRetCode = 0;
    else
       return(ERROR_BAD_FORMAT);
  }

  // Check if pszCurrent name is in the valid format (8.3)
  // if not writing to HPFS
  pszBootLetter = strchr(pszCurrent,':');
  if (pszBootLetter) {
    szBootLetter[0]= pszCurrent[0];
    szBootLetter[1]= pszCurrent[1];
    szBootLetter[2]= 0;
  }  else
     return(ERROR_INVALID_DRIVE);

  if (!CheckHPFS(szBootLetter)){
    if (CheckValidFileName(pszCurrent)) 
       usRetCode = 0;
    else
       return(ERROR_BAD_FORMAT);
  }


  //Write DEVICE and RUN statement into config.sys
  if (usRetCode == NO_ERROR) 
    usRetCode = WriteLockFileDDToConfig(pszListFile);
  else
     return (usRetCode);

  if (DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                     (PVOID)&ulBootDrive, sizeof(ULONG)) == NO_ERROR)
  {
     BootLetter[0] = ulBootDrive + 'A' -1;
     fullPathnameListFile[0]=BootLetter[0];
     BootLetter[1] = 0;
     strcpy(fullPathnameListFile+1, ":\\OS2\\INSTALL\\");
     uLen = strlen(fullPathnameListFile);
  }
  else
  {
     strcpy(fullPathnameListFile, "");
     uLen = 0;
  }
  if (uLen == 0) {
    return(ERROR_INVALID_FUNCTION);
  }

  strcat(fullPathnameListFile,pszListFile);

  // Check to see if file exists or not
  if (usRetCode == NO_ERROR) {
    usRetCode = ReadFileToBuffer(fullPathnameListFile,
                                 &pszReadTestDummyBuffer,
                                 &usSize);
  } else
    return (usRetCode);

  pszList = malloc((4096) * sizeof(char));
  pszList[0]=0;

  strcat(pszList,"COPY ");
  strcat(pszList,pszCurrent);
  strcat(pszList," ");
  strcat(pszList,pszFinal);
  strcat(pszList,"\r\n");

  strcat(pszList,"ERASE ");
  strcat(pszList,pszCurrent);
  strcat(pszList,"\r\n");
  
  if (usRetCode==ERROR_FILE_NOT_FOUND) {
     usRetCode = WriteBufferToFile (fullPathnameListFile, &pszList); 
  }
  else 
     if (usRetCode == NO_ERROR) {
       usRetCode = AppendBufferToFile (fullPathnameListFile, &pszList); 
     }
  return(usRetCode);
} //WriteLockFileRenameEntry

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: NextLine                                                º */
/* º                                                                      º */
/* º    Finds the next line after a specified spot in a file buffer.      º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
char *NextLine(PSZ pszPos)
 {               /* Returns pointer to the next line or NULL if end of file*/
     /* Get next line after this position */

  pszPos = strpbrk(pszPos,EOFLINE);   /* Find first CR, LF, or EOF (strpbrk*/
                                     /* ...finds 1st of str2 in str1)     */
  while(*pszPos==CR || *pszPos==LF)/* Skip over carriage returns and line feeds*/
     pszPos++;

  return((*pszPos==EOFFILE) ? NULL : pszPos);
      /* If at end of file return 0, else return pointer            */
}                                                             /* NextLine()*/

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: CopyLine                                                º */
/* º                                                                      º */
/* º    Copies a line of a file to a buffer                               º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
VOID CopyLine(char *pszPos,            /* Starting position to copy from   */
              char *pszBuffer)                /* Buffer to copy the line to*/
 {
   USHORT usCharsToCopy;               /* Number of characters to copy     */

   usCharsToCopy = strcspn(pszPos, EOFLINE);
   strncpy (pszBuffer, pszPos, usCharsToCopy);
   pszBuffer[usCharsToCopy] = '\0';

}                                                               /* CopyLine*/

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: StripFrontWhite                                         º */
/* º                                                                      º */
/* º      Strips the white space off the front of a string.               º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
VOID StripFrontWhite(char *pszStrip )  /* String to strip                  */
{
 USHORT cbStartJunk;                   /* White space at the beginning     */
                                            /* ... of the line              */
 cbStartJunk = strspn (pszStrip, BLANK_TAB);
                                            /* Get # of blanks and tabs at  */
                                            /* ...the front of the string   */
 if(cbStartJunk)                                 /* If there are some there*/
    ScootString(pszStrip+cbStartJunk,       /* ...strip them off by 'shift'*/
                -cbStartJunk);                            /* ...to the left*/
 return;
}                                                        /* StripFrontWhite*/

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: InsertString                                            º */
/* º                                                                      º */
/* º    Inserts a new string into a buffer at a particular location.      º */
/* º    Appends what's after a second specified location.                 º */
/* º    Can be used to either insert or overlay portions of the buffer.   º */
/* º                                                                      º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
ULONG InsertString(                   /* Returns the adjusted size in bytes*/
       char *pszInsert,                                 /* String to insert*/
       char *pszInsertAt,                       /* Position to insert it at*/
       char *pszSaveAfter)               /* Everything beyond this location*/
                            /* ...is appended after insert string*/
                            /* ...If == pszInsertAt then insert  */
                            /* ...Otherwise pszInsert replaces   */
                            /* ...from pszInsertAt - pszSaveAfter*/
                            /* ...Must be beyond pszInsertAt     */
 {
   USHORT cbInsertLen;
   USHORT cbDelChars;

   cbInsertLen = strlen(pszInsert);
   cbDelChars  = pszSaveAfter - pszInsertAt;

   if (cbInsertLen != cbDelChars)
      ScootString (pszSaveAfter, (cbInsertLen-cbDelChars));

  CopyNBytes (pszInsertAt, pszInsert, cbInsertLen);
  return(cbInsertLen - cbDelChars);    /* Return the size of the adjustment*/
                                       /* ...This is the # of chars inserted*/
                                       /* ...minus the  # of chars deleted  */

}

/****************************************************************************
* Steve Dobbelstein's routines                                              *
****************************************************************************/
/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: ScootString                                             º */
/* º                                                                      º */
/* º    Shifts or scoots the remainder of a string to the right or        º */
/* º    left.  Given a pointer into the string, the rest of the string    º */
/* º    will scoot a given number (positive or negative) of bytes.        º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
CHAR *ScootString(CHAR *pchString, SHORT sNumBytes)
   {
   CHAR *pchSource;
   CHAR *pchTarget;
   USHORT cbLength;
   signed int chDirection;

   if(pchString != NULL)
    {
     if(sNumBytes != 0)
      {
       cbLength = StringLength(pchString);
       if(sNumBytes > 0)
        {
         chDirection = -1;
         pchSource = pchString + cbLength;
        }
       else
        {
         chDirection = 1;
         pchSource = pchString;
        }

       pchTarget = pchSource + sNumBytes;
       cbLength++;
       while(cbLength)
        {
         *pchTarget = *pchSource;
         pchSource += chDirection;
         pchTarget += chDirection;
         cbLength--;
        }
      }
       return(pchString + sNumBytes);
    }
   else
    return(pchString);
 }                                            /*                           */

/***************************************************************************/
CHAR * CopyNBytes(CHAR *pchTarget, CHAR *pchSource, USHORT cbNumBytes)
 {
  USHORT ichIndex;

   for(ichIndex = 0; ichIndex < cbNumBytes; ichIndex++)
      {
      pchTarget[ichIndex] = pchSource[ichIndex];
      }

   return(pchTarget);
   }                                            /*                           */

/*****************************************************************************\
*                                                                             *
\*****************************************************************************/
                                                /*                           */
USHORT StringLength(pchLine)                    /*                           */
   CHAR * pchLine;                              /*                           */
   {                                            /*                           */
   USHORT cchStringLength;

   for(cchStringLength = 0;                      /*                          */
       pchLine[cchStringLength] != '\0';
       cchStringLength++);

   return(cchStringLength);
   }                                            /*                           */

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: ReadFileToBuffer                                        º */
/* º                                                                      º */
/* º      Reads a given file into a memory buffer allocated by this       º */
/* º      routine.  An ASCIIZ '\0' is appended to the end so the buffer   º */
/* º      can be treated as one long string.  The buffer is 4K larger     º */
/* º      than the file to allow expansion.  Files > (64K-4K) can't       º */
/* º      be used by this routine.                                        º */
/* º      The routine also appends an EOF character (x1A) at the          º */
/* º      end if one isn't there already.  This additional character      º */
/* º      is not reflected in the returned usFileSize, allowing a         º */
/* º      caller to figure if it was added (by comparing usFileSize       º */
/* º      to strlen(pszBuffer) ) and strip if desired.                    º */
/* º                                                                      º */
/* º                                                                      º */
/* º    Input parameters:                                                 º */
/* º      pszFileName      Full drive, path, and name of file             º */
/* º      fsOpenMode       Mode in which to open the file                 º */
/* º                                                                      º */
/* º    Output parameters:                                                º */
/* º      pszBuffer        Buffer allocated by this proc                  º */
/* º      usFileSize       Size in bytes of the file                      º */
/* º      hfHandle         File handle, needed to write file              º */
/* º                                                                      º */
/* º    Returns:           Function return code.  RF_xxx in LIB_DEFS.H    º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
ULONG ReadFileToBuffer(                /* Returns a return code            */
       char *pszFileName,             /* Full drive, path, and name of file*/
       char **pszBuffer,                   /* Buffer allocated by this proc*/
       ULONG *filsiz)
{
 ULONG usFileSize;
 HFILE hfHandle;
 ULONG     usAction;                   /* Action taken by DosOpen for input*/
 ULONG     cbBytesRead;                /* Number of bytes read into buffer */
 ULONG     fsOpenMode;
 APIRET     usRetCode;                 /* Return code from DosCalls        */
 APIRET     rc;
 FILESTATUS fstsFile;                  /* structure of file information    */
 CHAR rffail[MAXPATHLEN];              /* read file fail                   */
 char *FoundLF;                                     /* found last line feed*/
 char *FoundCR;                               /* found last carriage return*/

 fsOpenMode = DASD_FLAG | INHERIT | WRITE_THRU | FAIL_FLAG |
              SHARE_FLAG | ACCESS_FLAG;

 DosError(FERR_DISABLEHARDERR);              /* disable hard error popups    */
                                            /* Open the file                */

 /**************************************************************************
 *  If it's read-only, change it to normal                                 *
 **************************************************************************/
 usRetCode = DosOpen((PSZ)pszFileName,   /* File Name         */
                     &hfHandle,       /* Handle            */
                     &usAction,          /* Action taken      */
                     0L,                 /* Size if created   */
                     (ULONG)0,          /* File Attribute    */
                     (ULONG)OPEN_ACTION_OPEN_IF_EXISTS,
                                      /* Action to take if it exists or not*/
                     fsOpenMode,       /* Mode in which to open file    */
                     0L);                                       /* Reserved*/

 /**************************************************************************
 *  Restore the file attribute to the original value, if necessary         *
 **************************************************************************/
 DosError(FERR_ENABLEHARDERR);                  /* enable hard error popups*/

 if(!usRetCode)
  {
   DosQueryFileInfo(hfHandle,                   /* Get the size of the file*/
                    FIL_STANDARD,
                    (PVOID)&fstsFile,
                    sizeof(FILESTATUS));

   usFileSize = (ULONG)fstsFile.cbFile;

   if((*pszBuffer = malloc((usFileSize+4096) * sizeof(char))) != NULL)
    {
     rc = DosRead(hfHandle,                             /* Read in the file*/
                  *pszBuffer,
                  usFileSize,
                  &cbBytesRead);
     if(rc == 0)
      {
//     *filsiz=usFileSize;                                       /* P1D */
       (*pszBuffer)[cbBytesRead] = '\0';           /* null terminate string*/
       if(strchr((*pszBuffer), EOFFILE) == NULL)
        {                                  /* If no EOF (x1A) at end add it*/
         (*pszBuffer)[cbBytesRead] = EOFFILE;
         (*pszBuffer)[cbBytesRead+1] = '\0';
         cbBytesRead = cbBytesRead+1;
        }
       FoundLF= strrchr((*pszBuffer),LF);         /* find last lf     */
       FoundCR= strrchr((*pszBuffer),CR);         /* find last cr     */
       if(((!(FoundCR)) && (!(FoundLF))) ||
         ((!(FoundCR)) && ((*(FoundLF+1)) == EOFFILE)) ||
         ((!(FoundLF)) && ((*(FoundCR+1)) == EOFFILE)))
        {                         /* if no CRLF on last line add it*/
         (*pszBuffer)[cbBytesRead-1] = CR;     /* add carr ret     */
         (*pszBuffer)[cbBytesRead] = LF;       /* add linefeed     */
         (*pszBuffer)[cbBytesRead+1] = EOFFILE;/* add eof          */
         (*pszBuffer)[cbBytesRead+2] = '\0';   /* add null term    */
        }
       *filsiz=cbBytesRead;                                      /* P1A */
      }
     else
       usRetCode = ERROR_READ_FAULT;   /* nothing read              */
    }                                /* End if DosAllocSeg()         */
   else                                /* Else DosAllocSeg Fail        */
     usRetCode = ERROR_NOT_ENOUGH_MEMORY;   /* not enough memory      */

   if(DosClose(hfHandle))              /* Close the input file    */
     usRetCode = ERROR_FILE_NOT_FOUND;  /* return error if close failed */
  } /* End if !DosOpen()            */
 else   /* Else DosOpen Failed          */
  {
   if(usRetCode == ERROR_OPEN_FAILED)
     usRetCode = ERROR_FILE_NOT_FOUND;
   }

 return(usRetCode);

}   /* ReadFileToBuffer() */

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: WriteBufferToFile                                       º */
/* º                                                                      º */
/* º      Writes a memory buffer to a file.  The memory buffer            º */
/* º      contains the complete contents of the file.                     º */
/* º                                                                      º */
/* º    Input parameters:                                                 º */
/* º      hfHandle         Handle of the file                             º */
/* º      pszBuffer        Buffer (ASCIIZ string) to write                º */
/* º                                                                      º */
/* º    Output parameters:                                                º */
/* º      None                                                            º */
/* º                                                                      º */
/* º    Returns:           0 or DOS API return code                       º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
ULONG WriteBufferToFile(     /* Returns a return code             */
       char *pszFileName,    /* File name                       */
       char **pszBuffer)     /* Buffer to write                   */
{
 HFILE      hfHandle;
 ULONG      usAction;             /* Action taken by DosOpen for input */
 APIRET     usRetCode;            /* Return code from DosCalls        */
 ULONG      cchFileSize;          /* Size of the new file             */
 ULONG      ulNewPtr;             /* File pointer                     */
 ULONG      cbBytesWritten;       /* Number of bytes written from buff*/
 ULONG      fsOpenMode;

 fsOpenMode = DASD_FLAG | INHERIT | WRITE_THRU | FAIL_FLAG |
              SHARE_FLAG | ACCESS_FLAG;

 DosError(FERR_DISABLEHARDERR);              /* disable hard error popups    */

 cchFileSize = strlen(*pszBuffer);           /* Number of bytes to write     */


 usRetCode = DosOpen((PSZ)pszFileName,   /* File Name         */
                     &hfHandle,       /* Handle            */
                     &usAction,          /* Action taken      */
                     cchFileSize,                 /* Size if created   */
                     (ULONG)0,          /* File Attribute    */
                     (ULONG)OPEN_ACTION_REPLACE_IF_EXISTS |
                            OPEN_ACTION_CREATE_IF_NEW,
                                      /* Action to take if it exists or not*/
                     fsOpenMode,       /* Mode in which to open file    */
                     0L);              /* Reserved          */

 if(!usRetCode)                        /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
  {                                    /*³ Write the buffer           ³*/
   usRetCode =                         /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
               DosWrite(hfHandle,      /* File Handle                  */
                        *pszBuffer,    /* Pointer to buffer            */
                        cchFileSize,   /* Number of bytes to write     */
                        &cbBytesWritten); /* # of bytes actually written  */
  usRetCode = DosClose(hfHandle);         /* Close the file       */
 }

// DosFreeMem((PVOID)*pszBuffer);         /*³Free the memory buffer³*/
 DosError(FERR_ENABLEHARDERR);          /* enable hard error popups     */

 return(usRetCode);                     /* Return last DOS API ret code */
}                                       /* WriteBufferToFile()          */

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: AppendBufferToFile                                      º */
/* º                                                                      º */
/* º      Appends a memory buffer to a file.  The memory buffer           º */
/* º      contains the data desired to append to a file.                  º */
/* º                                                                      º */
/* º    Input parameters:                                                 º */
/* º      hfHandle         Handle of the file                             º */
/* º      pszBuffer        Buffer (ASCIIZ string) to write                º */
/* º                                                                      º */
/* º    Output parameters:                                                º */
/* º      None                                                            º */
/* º                                                                      º */
/* º    Returns:           0 or DOS API return code                       º */
/* º                                                                      º */
/* º    Created 07.23.97 by Lawrence Hsieh                                º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
ULONG AppendBufferToFile(    /* Returns a return code             */
       char *pszFileName,    /* File name                         */
       char **pszBuffer)     /* Buffer to write                   */
{
 HFILE      hfHandle;
 ULONG      usAction;             /* Action taken by DosOpen for input */
 APIRET     usRetCode;            /* Return code from DosCalls        */
 ULONG      cchFileSize;          /* Size of the new file             */
 ULONG      ulNewPtr;             /* File pointer                     */
 ULONG      cbBytesWritten;       /* Number of bytes written from buff*/
 ULONG      cbBytesRead;          /* Number of bytes written from buff*/
 ULONG      fsOpenMode;
 ULONG      ulLocal,ulLocal2;
 CHAR       eof=EOFFILE;
 CHAR       chChkEof;

 fsOpenMode = DASD_FLAG | INHERIT | WRITE_THRU | FAIL_FLAG |
              SHARE_FLAG | ACCESS_FLAG;

 DosError(FERR_DISABLEHARDERR);              /* disable hard error popups    */

 cchFileSize = strlen(*pszBuffer);           /* Number of bytes to write     */


 usRetCode = DosOpen((PSZ)pszFileName,   /* File Name         */
                     &hfHandle,          /* Handle            */
                     &usAction,          /* Action taken      */
                     cchFileSize,        /* Size if created   */
                     (ULONG)0,           /* File Attribute    */
                     (ULONG)OPEN_ACTION_OPEN_IF_EXISTS |
                            OPEN_ACTION_CREATE_IF_NEW,
                                       /* Action to take if it exists or not*/
                     fsOpenMode,       /* Mode in which to open file    */
                     0L);              /* Reserved          */


 /* Move the file pointer back to the end of the file */
 usRetCode = DosSetFilePtr (hfHandle,        /* File Handle */
                     -1,                     /* Offset */
                     FILE_END,               /* Move from BOF */
                     &ulLocal);              /* New location address */

 //usRetCode = DosSetFilePtr (hfHandle,        /* File Handle */
                     //-1,                     /* Offset */
                     //FILE_END,               /* Move from BOF */
                     //&ulLocal2);             /* New location address */

 usRetCode = DosRead(hfHandle,                 /* Read in the file*/
              &chChkEof,
              1,
              &cbBytesRead);

 if (chChkEof==EOFFILE) {
    usRetCode = DosSetFilePtr (hfHandle,        /* File Handle */
                        -1,                     /* Offset */
                        FILE_END,               /* Move from BOF */
                        &ulLocal);              /* New location address */
 }


 if(!usRetCode)                        /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
  {                                    /*³ Write the buffer           ³*/
   usRetCode =                         /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
               DosWrite(hfHandle,      /* File Handle                  */
                        *pszBuffer,    /* Pointer to buffer            */
                        cchFileSize,   /* Number of bytes to write     */
                        &cbBytesWritten); /* # of bytes actually written  */

   usRetCode =                           
               DosWrite(hfHandle,          /* File Handle                  */
                        &eof,              /* Pointer to buffer            */
                        1,                 /* Number of bytes to write     */
                        &cbBytesWritten);  /* # of bytes actually written  */
   usRetCode = DosClose(hfHandle);         /* Close the file       */
  }

// DosFreeMem((PVOID)*pszBuffer);         /* Free the memory buffer  */
 DosError(FERR_ENABLEHARDERR);            /* enable hard error popups */

 return(usRetCode);                       /* Return last DOS API ret code */
}                                         /* WriteBufferToFile()          */
/***************************************************************************/
ULONG search_file_drive(char *filename, char *def_path)
 {
  ULONG drive_map;
  ULONG file_drive;
  ULONG file_drive_index;
  APIRET rc;
  ULONG  cur_drive;
  ULONG count = 1;
  char drive[3] = {'c', ':', '\0'};
  char searchpath[512];
  HDIR dirhandle = HDIR_CREATE;
  FILEFINDBUF3 Buf;
  USHORT DriveIndex;
  ULONG parmsize, datasize;
  BIOSPARAMETERBLOCK devices; // array of device info to be collected

  file_drive = 0;
  file_drive_index = 0x00000004;

  DosQueryCurrentDisk(&cur_drive, &drive_map);
  if(drive_map == 0)
    return(file_drive);

  for(DriveIndex=2; DriveIndex<=25; DriveIndex++)
   {
    if((drive_map&file_drive_index) != 0)
     {
      parmsize=sizeof(DriveRequest);
      datasize=sizeof(devices);
      DriveRequest.Infotype=0;
      DriveRequest.DriveUnit=DriveIndex;
      devices.bDeviceType = DEVTYPE_UNKNOWN;

      // ask for the device attributes, type & default BPB cylinder size
      if(!DosDevIOCtl(-1,IOCTL_DISK,DSK_GETDEVICEPARAMS,
        (PVOID)&DriveRequest, sizeof(DriveRequest), &parmsize,
        (PVOID)&devices, sizeof(BIOSPARAMETERBLOCK), &datasize))
       {
        if(devices.bDeviceType==DEVTYPE_FIXED)
         {
          searchpath[0] = '\0';
          strcat(searchpath, drive);
          strcat(searchpath, def_path);
          strcat(searchpath, "\\");
          strcat(searchpath, filename);
          rc = DosFindFirst(searchpath,
                            &dirhandle,
                            0,
                            &Buf,
                            sizeof(Buf),
                            &count,
                            FIL_STANDARD);
          if(rc == 0)
           {
            file_drive = file_drive_index | file_drive;
            DosFindClose(dirhandle);
           }
         }
       }
      dirhandle = HDIR_CREATE;
     }
    file_drive_index = file_drive_index<<1;
    drive[0]++;
   } /* end of for(i=0;  */
  return(file_drive);
 }
/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: CsysQuery                                               º */
/* º                                                                      º */
/* º      Query entries in the CONFIG.SYS file                            º */
/* º                                                                      º */
/* º    Input parameters:                                                 º */
/* º      pszFileBuff      Pointer where the search is to start from      º */
/* º      usType           Type of the entry                              º */
/* º      pszOptStr        Optional search string                         º */
/* º      pszRetValue      Pointer to a caller supplied buffer where      º */
/* º                          the resulting string is copied              º */
/* º                                                                      º */
/* º    Output parameters:                                                º */
/* º      None                                                            º */
/* º                                                                      º */
/* º    Returns:           Pointer into the buffer for the start of the   º */
/* º                          line that contained the result, or NULL     º */
/* º                          if not found                                º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* º   Psuedo code:                                                       º */
/* º                                                                      º */
/* º       foundptr = Null                                                º */
/* º       Build string to left of '='                                    º */
/* º                                                                      º */
/* º       while (search ptr != NULL)                                     º */
/* º         get a line                                                   º */
/* º         *format the line*                                            º */
/* º         if what's left of = matches search left                      º */
/* º           switch usType                                              º */
/* º             case csys_command                                        º */
/* º                      _ev                                             º */
/* º                      _path_ev                                        º */
/* º                      _path                                           º */
/* º                      _dpath                                          º */
/* º                      _libpath:  copy after = to buffer               º */
/* º                                 foundptr = searchptr                 º */
/* º                                                                      º */
/* º             case csys_device                                         º */
/* º                      _run                                            º */
/* º                      _ifs                                            º */
/* º                                                                      º */
/* º                          if optstr is found to the right of =        º */
/* º                              copy after = to buffer                  º */
/* º                              foundptr = searchptr                    º */
/* º                                                                      º */
/* º       } end of while                                                 º */
/* º                                                                      º */
/* º        return(found ptr);                                            º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
PSZ CsysQuery(
                                  /* Returns pointer into the file buffer   */
                                  /* ...for the start of the line that      */
                                  /* ...contains the result, or NULL if     */
                                  /* ...not present                         */

       PSZ    pszFileBuff,           /* File buffer (from ReadFileToBuffer)*/
       USHORT usType,                  /* Type of the statement            */
       PSZ    pszOptStr,               /* Optional search string           */
       PSZ    pszRetValue  )      /* String for the result described in the*/
                                  /* ...chart below.  This space is set     */
                                  /* ...aside by the caller.  128 bytes are */
                                  /* ...recommended.                        */
{
 PSZ   pszQueryBegin;                  /* Returned PSZ.  Beginning of      */
                                            /* ...located line              */
 CHAR szLeftOfEqual[64];               /* Search string to left of '='     */
 CHAR szFormattedLine[512];            /* Temp buffr for formatted line    */
 CHAR pszOptStrUpr[512];               /* Uppercase of pszOptStr  P2C      */
 PSZ  pszScan;                         /* Temp pointer to caller buffer    */
 PSZ  pszAfterEqual;                   /* Temp ptr to char after =         */
 BOOL fContinue;                       /* Flag to continue search          */
                                            /* ...the caller buffer         */
                                            /*                              */
 pszQueryBegin = NULL;                                     /* Not found yet*/
 strcpy(pszRetValue, "");                       /* Init user's return value*/
 if (pszOptStr != NULL)                      /* If there's an option string*/
   {                                         /* ...then get a duplicate and*/
//  pszOptStrUpr = strdup (pszOptStr);      /* ...uppercase it.Query is P2D */
    strcpy(pszOptStrUpr,pszOptStr);                                  /* P2A*/
    strupr (pszOptStrUpr);                  /* ...case insensitive          */
   }
 else
//    pszOptStrUpr[0] = NULL;
    pszOptStrUpr[0] = '\0';
                                            /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                                            /*³ Build string to left of =  ³*/
 BuildAssignment (usType, pszOptStrUpr,     /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
                  szLeftOfEqual);           /*                              */
                                            /*                              */
                                            /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                                            /*³ Search for matching line   ³*/
                                            /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
 pszScan = pszFileBuff;                     /* Set scan pointer             */
 fContinue = TRUE;                                  /* Continue search flag*/
 while (pszScan != NULL &&                   /* Loop until end of buffer or*/
        fContinue == TRUE   )                     /* ...we indicate to stop*/
   {                                        /*                              */
    FormatLine(pszScan,szFormattedLine);            /* Format the next line*/
    if (strnicmp(szFormattedLine,                /* If the beginning of the*/
                 szLeftOfEqual,            /* ...formatted line matches the*/
                 strlen(szLeftOfEqual))==0) /* ...search string (compare=0)*/
      {                                                 /* ...ignoring case*/
       pszAfterEqual = strchr(pszScan,'=') +1;
                                            /* Pointer to 1st char after '='*/
       if (usType == CSYS_DEVICE ||          /* If the line type is DEVICE=*/
           usType == CSYS_RUN    ||           /* ...RUN= or IFS= the option*/
           usType == CSYS_20     ||           /* ...RUN= or IFS= the option*/
           usType == CSYS_REM    ||                          /* ...or CALL=*/
           usType == CSYS_SET    ||                          /* ...or CALL=*/
           usType == CSYS_IFS    ||                          /* ...or CALL=*/
           usType == CSYS_BASEDEV ||                         /* ...or CALL=*/
           usType == CSYS_MEMMAN ||                         /* ...or CALL=*/
           usType == CSYS_CALL     )           /* ...string is to the right*/
         {                                                     /* ...of '='*/
//          if (pszOptStrUpr[0] == NULL ||
          if (pszOptStrUpr[0] == '\0' ||
              strstr(szFormattedLine,pszOptStrUpr) )
            {                               /* If option string is in line  */
                                            /* ...or no option string (get  */
                                            /* ...first DEVICE, RUN, or IFS)*/
             pszQueryBegin = pszScan;       /* ...ret ptr to line beginning */
             CopyLine (pszAfterEqual, pszRetValue);
                                            /* Copy after '=' to caller     */
                                            /* ...supplied buffer           */
             fContinue = FALSE;             /* DEVICE, RUN, and IFS return  */
            }                               /* ...first                     */
         }                                  /*                              */
       else                                 /* Not a DEVICE= RUN= or IFS=   */
         {                                  /*                              */
          pszQueryBegin = pszScan;          /* Return ptr to line beginning */
          CopyLine (pszAfterEqual, pszRetValue);
                                            /* Copy from after the '=' to   */
                                            /* ...the caller's buffer       */
         }                                  /* Continue search to get last  */
      }                                     /*                              */
    pszScan = NextLine(pszScan);            /* Get next line                */
   }                                        /*                              */
                                            /*                              */
 return(pszQueryBegin);                     /* Return ptr to the beginning  */
                                            /* ...of the line found         */
}                                           /* CsysQuery                    */

/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: CsysUpdate                                              º */
/* º                                                                      º */
/* º      Changes entries in the CONFIG.SYS file                          º */
/* º                                                                      º */
/* º    Input parameters:                                                 º */
/* º      pszFileBuff      Pointer where the search is to start from      º */
/* º      usType           Type of the entry                              º */
/* º      pszOptStr        Optional string (for search or setting)        º */
/* º      pszUpdate        New value or entry to add                      º */
/* º      pszInsertAt      Position pointer for insertion                 º */
/* º                                                                      º */
/* º    Output parameters:                                                º */
/* º      None                                                            º */
/* º                                                                      º */
/* º    Returns:           Pointer into the buffer for the start of the   º */
/* º                          updated line                                º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* º   Psuedo code:                                                       º */
/* º                                                                      º */
/* º      -----get the location of the line----                           º */
/* º      if the line is already there (query it)                         º */
/* º        this line is insert postion                                   º */
/* º      else                                                            º */
/* º        use user specified insert position                            º */
/* º                                                                      º */
/* º                                                                      º */
/* º      -----get the location of the line----                           º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* º        build line                                                    º */
/* º        insert  adding a crlf to end                                  º */
/* º                                                                      º */
/* º                                                                      º */
/* º      -----get the location in the line where to put update ---       º */
/* º      not path - after =, nuke between = and crlf                     º */
/* º      path     - insert if not already there                          º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */

PSZ CsysUpdate(
                                  /* Returns pointer into the file buffer   */
                                  /* ...for the start of the line that      */
                                  /* ...contains the result.                */

       PSZ    pszFileBuff,           /* File buffer (from ReadFileToBuffer)*/
       USHORT usType,                  /* Type of the statement            */
       PSZ    pszOptStr,         /* Optional string (for search or setting)*/
       PSZ    pszUpdate,               /* New value or entry to add        */
       PSZ    pszInsertAt )            /* Position pointer for insertion   */


{
 PSZ   pszUpdateBegin;                 /* Returned PSZ.  Beginning of      */
                                            /* ...updated line              */
 PSZ   pszScan;                        /* Temp pointer to caller buffer    */
 PSZ   pszTemp;                        /* Another one                      */
 CHAR  szQueryResult[512];             /* Result of query for existing     */
 CHAR  szTempBuff[512];                /* Temp buffer for a line           */
 CHAR  szUpdateFormatted[512];         /* Caller's update string after     */
                                            /* ...formatting                */
 CHAR  szNewUpToEqual[512];            /* Used to build new line           */
 SHORT cbLenValue,                     /* Length of value part of line     */
       cbEndJunk;                           /* Bytes at the end of the line*/
 BOOL  fNewLineCreated;                /* Indicates a new line created     */

                             /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                             /*³ Get the location of the updated line      ³*/
                             /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
                                            /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                                            /*³ Search for existing line   ³*/
                                            /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
 if ((usType == CSYS_DEVICE  ||             /* If DEVICE= RUN= or IFS= line*/
      usType == CSYS_RUN     ||                /* ...and no optional search*/
      usType==CSYS_DISKCACHE ||
      usType == CSYS_20      ||
      usType == CSYS_CALL    ||
      usType == CSYS_REM     ||
      usType == CSYS_SET     ||
      usType == CSYS_BASEDEV ||
      usType == CSYS_MEMMAN  ||
      usType == CSYS_IFS )   &&             /* ...string then just add line*/
     (pszOptStr == NULL  )    )              /* ...by indicating none found*/
    pszScan = NULL;
 else
   {
    pszScan = CsysQuery (pszFileBuff, usType, pszOptStr,szQueryResult);
    strupr (szQueryResult);
   }                                            /* Search for existing line*/
                                            /* ...to overlay or, if path    */
                                            /* ...type append               */
                                            /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
 if (pszScan == NULL)                     /* ³ If none exists create new  ³*/
   {                                      /* ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
                                            /* ---Find where it goes -----  */
    if      (pszInsertAt == CSYS_LAST ||    /* Specified last line, or path*/
             usType == CSYS_PATH      ||   /* ...type.  The pszInsertAt for*/
             usType == CSYS_DPATH     ||   /* ...paths doesn't position the*/
             usType == CSYS_LIBPATH   ||      /* ...line.  It sets position*/
             usType == CSYS_BOOKSHELF ||    /* ...within existing line.  If*/
             usType == CSYS_AUTOSTART ||    /* ...within existing line.  If*/
             usType == CSYS_HELP      ||    /* ...within existing line.  If*/
             usType == CSYS_PATH_EV     )   /* ...no existing, place at end.*/
                                            /*                          P3C */
                                            /*                              */
     {                                                                 //11P8A
      pszScan = strchr(pszFileBuff,EOFFILE);/* Insert just before EndOfFile */
      --pszScan;
      if (*pszScan != LF)
        {
           ++pszScan;
           strcpy (szNewUpToEqual, CRLF);
           InsertString (szNewUpToEqual,
                         pszScan,
                         pszScan);
           ++pszScan;
        }
      ++pszScan;
     }
    else if (pszInsertAt == CSYS_FIRST)     /* Specified first line of file */
      pszScan = pszFileBuff;                /* ...goes at beginning of buff */
    else if (pszInsertAt == CSYS_FIRST_TYPE)/* First executable of similar  */
      {                                     /* ...type (DEVICE, RUN, or IFS)*/
       pszScan = CsysQuery (pszFileBuff, usType, NULL,szTempBuff);
                                            /* Find first line of similar   */
                                            /* ...type (NULL as 3rd parm)   */
       if (pszScan == NULL)
         {                                                             //11P8A
          pszScan = strchr(pszFileBuff,EOFFILE);
          --pszScan;
          if (*pszScan != LF)
            {
               ++pszScan;
               strcpy (szNewUpToEqual, CRLF);
               InsertString (szNewUpToEqual,
                             pszScan,
                             pszScan);
               ++pszScan;
            }
          ++pszScan;
         }
                                            /* If none there put at the end */
      }
    else if (pszInsertAt == CSYS_LAST_TYPE) /* Last executable of similar   */
      {                                     /* ...type (DEVICE, RUN, or IFS)*/
       pszScan = strchr(pszFileBuff,EOFFILE);

       --pszScan;                                                      //9P8A
       if (*pszScan != LF)
         {
            ++pszScan;
            strcpy (szNewUpToEqual, CRLF);
            InsertString (szNewUpToEqual,
                          pszScan,
                          pszScan);
            ++pszScan;
         }
       ++pszScan;
                                            /* Init putting it at the end   */
                                            /* ...if no similar type exists */
       pszTemp = CsysQuery (pszFileBuff, usType, NULL,szTempBuff);
                                            /* Find first line of similar   */
                                            /* ...type (NULL as 3rd parm)   */
       while (pszTemp != NULL)              /* Loop until no more of similar*/
         {                                  /* ...type found                */
          pszTemp = NextLine(pszTemp);      /* Get ptr to next line         */
          if (pszTemp != NULL)              /* If there is a next line      */
            {                               /* ...then save it as the new   */
             pszScan = pszTemp;             /* ...update location           */
             pszTemp = CsysQuery (pszTemp, usType, NULL,szTempBuff);
                                            /* Find next of type            */
            }
          else
            {                                                             //11P8A
             pszScan = strchr(pszFileBuff,EOFFILE);
             --pszScan;
             if (*pszScan != LF)
               {
                  ++pszScan;
                  strcpy (szNewUpToEqual, CRLF);
                  InsertString (szNewUpToEqual,
                                pszScan,
                                pszScan);
                  ++pszScan;
               }
             ++pszScan;
            }
         }                                  /* No next line.  Add at end    */
      }
                                            /*                              */
    else                                    /* Otherwise value given is an  */
      pszScan = pszInsertAt;                /* ...actual pointer            */
                                            /*                              */
    BuildAssignment (usType, pszOptStr,     /* Build skeleton of new line   */
                     szNewUpToEqual    );   /*                              */
    strcat (szNewUpToEqual, CRLF);          /* Append end of line chars     */
                                            /* Insert the new assignment    */
    InsertString (szNewUpToEqual,           /* ...String to insert          */
                  pszScan,                  /* ...Insert location           */
                  pszScan        );         /* ...Keep all (insert)         */
    fNewLineCreated = TRUE;                 /* Indicate this is new line    */
   }
 else
    fNewLineCreated = FALSE;                                 /* Line exists*/

 pszUpdateBegin = pszScan;                  /* Save beginning of updated ln*/
 if((usType == CSYS_REM) ||
   (usType == CSYS_SET))
   pszScan = strchr(pszScan,' ') + 1;         /* Most likely update is placed*/
 else
   pszScan = strchr(pszScan,'=') + 1;         /* Most likely update is placed*/
                                            /* ...after the '=' assignment  */
                  /* whoops should the above be 1st non-blank after = ? */
                  /* (only if not new) */
                             /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                             /*³ Find where in the line the update goes    ³*/
                             /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
 switch (usType)
   {
    case CSYS_COMMAND:                     /* On these types we overlay any*/
    case CSYS_EV:                                      /* ...existing value*/
    case CSYS_DEVICE:
    case CSYS_RUN:
    case CSYS_IFS:
    case CSYS_20:
    case CSYS_DISKCACHE:
    case CSYS_CALL:
    case CSYS_REM:
    case CSYS_SET:
    case CSYS_BASEDEV:
    case CSYS_MEMMAN:
            InsertString (pszUpdate,
                          pszScan,
                          strpbrk (pszScan, EOFLINE) );
            break;

    case CSYS_PATH:                        /* For these types the new value*/
    case CSYS_DPATH:                           /* ...is appended (unless it*/
    case CSYS_LIBPATH:                                /* ...already exists)*/
    case CSYS_BOOKSHELF:                                             /* P3A*/
    case CSYS_AUTOSTART:                                             /* P3A*/
    case CSYS_PATH_EV:
    case CSYS_PROTSHELL:
    case CSYS_HELP:
                                            /* Check if new path is already */
                                            /* ...there.  If so no update   */
                                            /* ...is done                   */
            strcpy (szTempBuff, pszUpdate);
            strupr (szTempBuff);                   /* Uppercase user update*/
            if (strstr (szQueryResult, szTempBuff) == NULL)
                                            /* If new path not found in     */
              {                              /* ...query result then add it*/
               strcpy (szUpdateFormatted, pszUpdate);
               if (usType == CSYS_PROTSHELL)
                strcat (szUpdateFormatted, " ");
               else if(usType == CSYS_AUTOSTART)
                strcat (szUpdateFormatted, ",");
               else
                strcat (szUpdateFormatted, ";");
                                            /* Format the new entry so it   */
                                            /* ...is a path followed by ';' */


               if (pszInsertAt == CSYS_LAST)
                 {
                                            /* Add as last path entry       */
                                            /* Insert after last non white  */
                  strcpy  (szTempBuff, "");
                  strncat (szTempBuff,
                           pszScan,
                            strcspn (pszScan, EOFLINE) );
                                            /* Copy from past the '=' up to */
                                            /* ...the end of line into a    */
                                            /* ...temporary buffer          */
                  cbLenValue = strlen (szTempBuff);
                                            /* Keep the len of value portion*/
                  strrev (szTempBuff);      /* Reverse the string           */
                  cbEndJunk = strspn (szTempBuff, BLANK_TAB_EOFLINE);
                  pszScan += (cbLenValue - cbEndJunk);
                                            /* Find # of junk bytes at end  */
                  if (usType != CSYS_PROTSHELL)
                   if (*(szTempBuff+cbEndJunk) != ';')
                     InsertString (";",
                                   szUpdateFormatted,
                                   szUpdateFormatted);
                                            /* If no ';' at end tack it on  */
                 }                          /* End last entry               */

               else if (pszInsertAt != CSYS_FIRST)
                  pszScan = pszInsertAt;    /* Not 1st or last.  Value is an*/
                                            /* ...actual ptr to insert pos. */
                                            /* Else pszInsertAt == first    */
                                            /* ...pszScan already after '=' */

               InsertString (szUpdateFormatted,
                            pszScan,
                            pszScan    );

              }                             /* End if path not already there*/
                                            /* Else path exists..dont add   */
         break;
   }

 return(pszUpdateBegin);                    /* Return ptr to the beginning  */
                                            /* ...of the updated line       */
}                                           /* CsysUpdate                   */


/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: CsysDelete                                              º */
/* º                                                                      º */
/* º      Deletes entries in the CONFIG.SYS file                          º */
/* º                                                                      º */
/* º    Input parameters:                                                 º */
/* º      pszFileBuff      Pointer where the search is to start from      º */
/* º      usType           Type of the entry                              º */
/* º      pszVariable      Variable to the left of the '=' assignment     º */
/* º      pszDelValue      Optional delete string                         º */
/* º                                                                      º */
/* º    Output parameters:                                                º */
/* º      None                                                            º */
/* º                                                                      º */
/* º    Returns:           Pointer into the buffer for the start of the   º */
/* º                          line that was deleted, or NULL if not found.º */
/* º                                                                      º */
/* º                                                                      º */
/* º   Psuedo code:                                                       º */
/* º                                                                      º */
/* º        Query for the line                                            º */
/* º          if line is found                                            º */
/* º            save ptr to the line                                      º */
/* º            if the line is a path type                                º */
/* º               remove all entries that have DelValue by:              º */
/* º               uppercase DelValue and Query result...case insensitive º */
/* º               search return from query for DelValue                  º */
/* º               while matching path entry found                        º */
/* º                  save ptr to DelValue                                º */
/* º                  Find start of entry by locating last ; before ptr   º */
/* º                  if no ; before this is first..use = + 1             º */
/* º                  find last char in entry by getting ptr to next ';'  º */
/* º                    or last char of line                              º */
/* º                  delete from start to next (takes off trailing ;)    º */
/* º                    in both the file buffer and temp query result buffº */
/* º                  search for next matching path entry to delete       º */
/* º               end while                                              º */
/* º            else not path type                                        º */
/* º              loop removing all existing lines                        º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
PSZ CsysDelete(
                                  /* Returns pointer into the file buffer   */
                                  /* ...for the start of the line that      */
                                  /* ...was deleted, or NULL if not there   */

       PSZ    pszFileBuff,        /* File buffer (from ReadFileToBuffer)    */
       USHORT usType,             /* Type of the statement                  */
       PSZ    pszOptStr,          /* Optional search string                 */
       PSZ    pszDelValue )       /* Optional delete value                  */



{
 PSZ   pszDeleteBegin;                      /* Returned PSZ.  Beginning of  */
                                            /* ...deleted line              */
 PSZ   pszScan;                             /* Temp pointer                 */
 CHAR  pszDelUpr[512];                      /* Duplicate of pszDelValue P2C */
                                            /* ...converted to uppercase    */
 PSZ   pszValue;                            /* Ptr to first char after '='  */
                                            /* ...in caller buffer          */
 PSZ   pszDelLoc,                           /* Ptrs into the query buffer   */
       pszEntryStart,                       /* ...for located string, start */
       pszEntryEnd;                         /* ...and end of located entry  */
 PSZ   pszNextLine;                         /* Pointer to next line         */
 CHAR  szQueryResult[512];                  /* Result of query for existing */




   pszDeleteBegin = CsysQuery (pszFileBuff, usType, pszOptStr,szQueryResult);
   if (pszDeleteBegin != NULL)              /* If there's something to nuke */
     {                                      /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
                                            /*³ Path style statements      ³*/
                                            /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
      if (usType == CSYS_PATH      ||       /* For path type statements, we */
          usType == CSYS_DPATH     ||       /* ...delete all matched entries*/
          usType == CSYS_LIBPATH   ||       /* ...within the one line.      */
          usType == CSYS_PATH_EV   ||       /* ...(If more than one line do */
          usType == CSYS_BOOKSHELF ||       /* ...the last since it is used)*/
          usType == CSYS_AUTOSTART ||       /* ...the last since it is used)*/
          usType == CSYS_HELP      ||       /* ...the last since it is used)*/
          usType == CSYS_PROTSHELL   )      /*                          P3C */
        {                                   /*                              */
//       pszDelUpr = strdup(pszDelValue);   /* Duplicate users path and P2D */
         strcpy(pszDelUpr,pszDelValue);                              //P2A
         strupr(pszDelUpr);                 /* ...uppercase                 */
         pszValue = strstr(pszDeleteBegin, szQueryResult);
                                            /* Get pointer in buffer to     */
                                            /* ...the start of the result   */
                                            /* Changes are made directly to */
                                            /* ...the buffer rather than    */
                                            /* ...changing and replacing the*/
                                            /* ...query result.  This is    */
                                            /* ...because delete is case    */
                                            /* ...insensitive, so the       */
         strupr(szQueryResult);             /* ...query result is uppercased*/
         pszDelLoc = strstr(szQueryResult, pszDelUpr);
                                            /* Search the (uppercased) query*/
                                            /* ...result for the (uppercase)*/
                                            /* ...delete path               */
         if (pszDelLoc == NULL)             /* If we found the path line    */
            pszDeleteBegin = NULL;          /* ...but no matching entry     */
                                            /* ...(a.k.a. no delete) ret NUL*/
         while (pszDelLoc != NULL)          /* While found loop deletes all */
           {                                /* ...entries that match        */
                                            /* Find start and end of path   */
                                            /* ...entry                     */
            pszEntryStart = szQueryResult;  /* Init start                   */
            pszEntryEnd = NULL;             /* End of entry not found yet   */
            pszScan = szQueryResult;        /* Search the query result      */
            while (pszEntryEnd == NULL)     /* Search through path entries  */
              {                             /* ...until we get the end of   */
                                            /* ...the located entry         */
               if (usType == CSYS_PROTSHELL)
                pszScan = strchr (pszScan, BLANK);
               else if(usType == CSYS_AUTOSTART)
                pszScan = strchr (pszScan, ',');
               else
                pszScan = strchr (pszScan, SEMICOLON);
                                            /* Find ptr to first ';'        */
               if (pszScan != NULL)
                 {
                  if (pszScan < pszDelLoc)  /* If it's before delete match  */
                     pszEntryStart = pszScan + 1;
                                            /* Set (or reset) start of entry*/
                                            /* ...to char after the ';'     */
                  else                      /* Otherwise ';' marks the next */
                     {                      /* ...entry or end              */
                     pszEntryEnd = pszScan; /* We'll strip ';' at end       */
                     }
                 }
               else                         /* No ';' after.  This is the   */
                 {                          /* ...last or only path entry   */
                  pszEntryEnd = szQueryResult + strlen(szQueryResult) -1;
                 }                          /* Set to last of query result  */
               pszScan++;                   /* Check next after ';'         */
              }
                                            /* Now we have begin and end    */
                                            /* ...ptrs in query buffer.  We */
                                            /* ...need to translate to the  */
                                            /* ...file buffer then delete   */
            ScootString
             (pszValue + (pszEntryEnd +1 - szQueryResult ),
              (SHORT)(pszEntryStart - (pszEntryEnd +1))
             );
                                            /* Delete by scooting the string*/
                                            /* ...Move beginning of value   */
                                            /* ...in file buff + offset of  */
                                            /* ...entry end in query buff   */
                                            /* ...# of chars to move is     */
                                            /* ...len of entry (negative to */
                                            /* ...delete)                   */
            ScootString
             (pszEntryEnd+1,
              (SHORT)(pszEntryStart - (pszEntryEnd +1))
             );                             /* Also adjust query buffer to  */
                                            /* ...keep them in synch        */
            pszDelLoc = strstr(szQueryResult, pszDelUpr);
                                            /* Now see if there's another   */
           }                                /* end loop for path entries    */
                                            /* ...on this line              */
        }                                   /* End path style delete        */
      else                                  /*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿*/
        {                                   /*³ Non-path statements        ³*/
         pszScan = pszDeleteBegin;          /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
         while (pszScan != NULL)
           {
            pszNextLine = NextLine(pszScan);   /* Find pointer to next line    */
            if (pszNextLine == NULL)           /* If this is the last line then*/
               pszNextLine = strrchr(pszScan,EOFFILE);
                                            /* ...get ptr to end of file    */
            InsertString
                  ("",                         /* ...String to insert          */
                   pszScan,                    /* ...Insert location           */
                   pszNextLine);               /* ...Keep all (insert)         */

            if (usType != CSYS_DEVICE &&     /* Update psz to ret to caller*/
                usType != CSYS_RUN    &&    /* ...Since certain queries ret*/
                usType != CSYS_IFS    &&      /* ...the last this loop will*/
                usType != CSYS_CALL   &&      /* ...the last this loop will*/
                usType != CSYS_REM    &&      /* ...the last this loop will*/
                usType != CSYS_SET    &&      /* ...the last this loop will*/
                usType != CSYS_BASEDEV &&     /* ...the last this loop will*/
                usType != CSYS_MEMMAN &&    /* ...the last this loop will   */
                usType != CSYS_20       )
               pszDeleteBegin = pszScan;    /* ...then return the first     */
            pszScan = CsysQuery (pszFileBuff, usType,
                                 pszOptStr,szQueryResult);
                                            /* Try to find duplicates       */
           }                                /* end loop to delete non-path  */
        }                                   /* end else for non-path type   */
     }                                      /* end found line match         */

 return(pszDeleteBegin);                    /* Return ptr to the beginning  */
                                            /* ...of the delete line        */
}                                           /* CsysDelete                   */


/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: BuildAssignment                                         º */
/* º                                                                      º */
/* º      Builds an assignment statement for CONFIG.SYS.                  º */
/* º      This is the part of the line to the left of and including       º */
/* º      the '=' assignment.                                             º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
VOID BuildAssignment(
     USHORT usType,                    /* Type of the statement            */
     PSZ    pszOptStr,               /* Optional string for environment var*/
     PSZ    pszAssignment )        /* Caller supplied buffer for the result*/
{
 BOOL bNoEqual=FALSE;                  /* ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿   */
                                            /*³ Build string to left of =  ³*/
                                            /*ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ*/
 strcpy (pszAssignment,"");                                  /* Init string*/
 switch (usType)                                 /* Build according to type*/
   {                                        /*                              */
   case CSYS_EV:                            /*                              */
   case CSYS_PATH_EV:                       /*                              */
      strcat (pszAssignment,"SET ");          /* 'SET ' first, then tack on*/
                                            /* ...environment variable below*/
   case CSYS_COMMAND:                       /*                              */
      strcat (pszAssignment,pszOptStr);     /*                              */
      strupr (pszAssignment);                   /* OS/2 is case insensitive*/
      break;                                   /* ... on env var & commands*/
   case CSYS_PATH:                          /*                              */
      strcat (pszAssignment,"SET PATH");    /*                              */
      break;                                /*                              */
   case CSYS_DPATH:                         /*                              */
      strcat (pszAssignment,"SET DPATH");   /*                              */
      break;                                /*                              */
   case CSYS_LIBPATH:                       /*                              */
      strcat (pszAssignment,"LIBPATH");     /*                              */
      break;                                /*                              */
   case CSYS_BOOKSHELF:                                              /* P3A*/
      strcat (pszAssignment,"SET BOOKSHELF");                        /* P3A*/
      break;                                                         /* P3A*/
   case CSYS_AUTOSTART:                     /*                              */
      strcat (pszAssignment,"SET AUTOSTART");  /*                           */
      break;                                /*                              */
   case CSYS_DEVICE:                        /*                              */
      strcat (pszAssignment,"DEVICE");      /*                              */
      break;                                /*                              */
   case CSYS_RUN:                           /*                              */
      strcat (pszAssignment,"RUN");         /*                              */
      break;                                /*                              */
   case CSYS_IFS:                           /*                              */
      strcat (pszAssignment,"IFS");         /*                              */
      break;                                /*                              */
   case CSYS_20:                            /*                              */
      strcat (pszAssignment,"20");          /*                              */
      break;                                /*                              */
   case CSYS_PROTSHELL:                     /*                              */
      strcat (pszAssignment,"PROTSHELL");   /*                              */
      break;                                /*                              */
   case CSYS_HELP:                          /*                              */
      strcat (pszAssignment,"SET HELP");    /*                              */
      break;                                /*                              */
   case CSYS_CALL:                          /*                              */
      strcat (pszAssignment,"CALL");        /*                              */
      break;                                /*                              */
   case CSYS_REM:                           /*                              */
      strcat (pszAssignment,"REM");         /*                              */
      bNoEqual=TRUE;
      break;                                /*                              */
   case CSYS_SET:                           /*                              */
      strcat (pszAssignment,"SET");         /*                              */
      bNoEqual=TRUE;
      break;                                /*                              */
   case CSYS_DISKCACHE:
      strcat(pszAssignment,OS2FAT);
      break;
   case CSYS_BASEDEV:
      strcat(pszAssignment,"BASEDEV");
      break;
   case CSYS_MEMMAN:
      strcat(pszAssignment,"MEMMAN");
      break;
   }                                        /*                              */
   if(bNoEqual)
     strcat (pszAssignment," ");              /*                              */
   else
     strcat (pszAssignment,"=");              /*                              */
                                            /*                              */
 return;
}                                                        /* BuildAssignment*/


/* ÖÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ· */
/* º    Function: FormatLine                                              º */
/* º                                                                      º */
/* º      Formats a line of CONFIG.SYS by uppercasing it and removing     º */
/* º      selected white space.  Statements other than SET will           º */
/* º      be put in the following format                                  º */
/* º        'COMMAND='value                                               º */
/* º      where value is whatever currently trails the '=' (including     º */
/* º      white space).  SET statements are formatted as                  º */
/* º         'SET 'environment variable'='value                            º */
/* º      where environment variable is whatever is currently between     º */
/* º      the first non-white space after 'SET ' and '=' (including       º */
/* º      white space).                                                   º */
/* º                                                                      º */
/* º      As far as I can tell from testing, statements other than        º */
/* º      SET can have blanks and tabs anywhere before or after the       º */
/* º      '=' assignment.  In other words, the following are valid:       º */
/* º                                                                      º */
/* º               LIBPATH=xxxxx                                          º */
/* º               LIBPATH  =xxxx                                         º */
/* º               LIBPATH=  xxxxx                                        º */
/* º                 LIBPATH=xxxx                                         º */
/* º                                                                      º */
/* º      The set statement behaves differently, since there are          º */
/* º      two words before the '=' assignment.  It is essentially         º */
/* º      the same as above, with the exception that between the          º */
/* º      start of the second word and the '=' the blanks and tabs        º */
/* º      are treated like other characters.  For example, in the         º */
/* º      following two lines:                                            º */
/* º               SET PROMPT=                                            º */
/* º               SET PROMPT =                                           º */
/* º      'PROMPT' and 'PROMPT ' are considered two valid but distinct    º */
/* º      environment variables.  Only the first changes the OS/2         º */
/* º      command prompt.                                                 º */
/* º                                                                      º */
/* º      To the right of the '=' assignment, it appears white spaces     º */
/* º      are valid provided file names aren't split.                     º */
/* º      For example, the following are valid:                           º */
/* º         DEVICE=   C:\OS2\DOS.SYS                                     º */
/* º         PROTSHELL=   C:\OS2\PMSHELL.EXE      C:\OS2\OS2.INI          º */
/* º      However, values that are in path format don't appear to         º */
/* º      allow for white space.  The following is NOT valid:             º */
/* º         SET PATH=  C:\OS2;C:\MUGLIB  ;   C:\CMLIB  ;                 º */
/* º      There doesn't seem to be a clear consistency on this, since     º */
/* º      in some cases it seems to work.  We will opt not to allow it.   º */
/* º                                                                      º */
/* ÓÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ½ */
VOID FormatLine(
     PSZ    pszPos,                    /* Beginning of the unformatted line */
     PSZ    pszFormatted  )            /* Formatted line buffer provided    */
                                       /* ...by the caller                  */
{
 PSZ   pszStartDel;                    /* Where to start stipping whitespace*/
 PSZ   pszNextWhite;                   /* Next white space in line          */
 PSZ   pszEqualChar;                   /* First '=' assignment in line      */

 CopyLine(pszPos,pszFormatted);             /* Copy only contents of line   */
                                            /* ...(no CRLF or EOF)          */
 strupr (pszFormatted);                     /* Uppercase the thing          */
 StripFrontWhite (pszFormatted);            /* If blanks or tabs at front   */
                                            /* ...strip them off            */

 if (strncmp (pszFormatted, "SET", strlen("SET") ) == 0  &&
     strpbrk(pszFormatted, BLANK_TAB) == (pszFormatted+strlen("SET"))
    )
   {
    pszStartDel = pszFormatted+strlen("SET")+1;
   }
 else
    pszStartDel = pszFormatted;
 pszEqualChar = strchr(pszFormatted, EQUAL);/* Get ptr to 1st '='           */
 pszNextWhite = strpbrk(pszStartDel,        /* Get ptr to first white space */
                        BLANK_TAB);
 if (pszNextWhite < pszEqualChar &&         /* If there is white space      */
     pszNextWhite != NULL        &&         /* ...before the '=' and        */
     pszEqualChar != NULL              )
   {
    StripFrontWhite (pszNextWhite);
   }

 return;
}                                           /* FormatLine                   */
