/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 *     IBM Corp.
 */
#define INCL_DOSFILEMGR
#define INCL_DOSERRORS

#include <os2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unidef.h>
#include <ctype.h>
#include <math.h>
#include <io.h>

#define MAXLEVEL 30000
#define MAX_BUF  4096

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

BOOL GetDiskFreeSpace(PSZ lpRootPathName, PULONG, PULONG, PULONG, PULONG);

// get directory size 
void GetPathInfo( char *InputName,
     unsigned int   Level,
     ULONG      Sector,
     int   AddDirectorySize,
     int   AutoInfoFormat,
     unsigned int   CurrentLevel,
     ULONG     *TotalDirSize,
     ULONG     *TotalDirCount,
     ULONG     *TotalFileSize,
     ULONG     *TotalAllocSize);

char DirDepth[MAX_BUF];

struct Params
{
     unsigned long ArgNum;
     unsigned int  Level;
     ULONG     Sector;
     int  SuppressHeader;
     int  AutoInfoFormat;
     int  AddDirectorySize;
     char         *DirName;
};

void RemoveBackSlash(PSZ szInput)
{
  int   iCounter;
  ULONG dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = strlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '\\')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendBackSlash(PSZ szInput, ULONG dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((ULONG)strlen(szInput) + 1) < dwInputSize)
      {
        strcat(szInput, "\\");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((ULONG)strlen(szInput) + 1) < dwInputSize)
      {
        strcat(szInput, "\\");
      }
    }
  }
}

void ParsePath(PSZ szInput, PSZ szOutput, ULONG dwOutputSize, ULONG dwType)
{
     int   iCounter;
     ULONG dwCounter;
     ULONG dwInputLen;
     BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = TRUE;
    dwInputLen    = strlen(szInput);
    memset(szOutput, 0, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              strcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            strcpy(szOutput, szInput);

          break;

        case PP_ROOT_ONLY:
          if(szInput[1] == ':')
          {
            szOutput[0] = szInput[0];
            szOutput[1] = szInput[1];
            AppendBackSlash(szOutput, dwOutputSize);
          }
          else if(szInput[1] == '\\')
          {
            int iFoundBackSlash = 0;
            for(dwCounter = 0; dwCounter < dwInputLen; dwCounter++)
            {
              if(szInput[dwCounter] == '\\')
              {
                szOutput[dwCounter] = szInput[dwCounter];
                ++iFoundBackSlash;
              }

              if(iFoundBackSlash == 3)
                break;
            }

            if(iFoundBackSlash != 0)
              AppendBackSlash(szOutput, dwOutputSize);
          }
          break;
      }
    }
  }
}

// returns size of a directory
void GetDirSize(  char *InputName,
    unsigned int   Level,
    ULONG           Sector,
    int                AddDirectorySize,
    int                AutoInfoFormat,
    unsigned int   CurrentLevel,
    ULONG     *TotalDirSize,
    ULONG     *TotalDirCount,
    ULONG     *TotalFileSize,
    ULONG     *TotalAllocSize)
{
    HDIR          hDir;
    FILEFINDBUF   ffbFindFileBuffer;
    ULONG       llFileSize;
    ULONG       TmpTotalFileSize;
    ULONG       TmpTotalAllocSize;
    int             InputNameLen = strlen(InputName);
    int             DirNameLen   = InputNameLen;
    char          DirName[MAX_BUF];
    char          szBuf[MAX_BUF];

    ULONG         ulFindCount    = 1;

    memset(DirName, '\0', sizeof(DirName));
    memcpy(DirName, InputName, InputNameLen);

    *TotalFileSize  = 0;
    *TotalAllocSize = 0;

    AppendBackSlash(DirName, sizeof(DirName));
    strcat(DirName, "*.*");
    if((DosFindFirst(DirName, &hDir, FILE_NORMAL, &ffbFindFileBuffer, sizeof(FILEFINDBUF3), &ulFindCount, FIL_STANDARD)) != ERROR_INVALID_HANDLE)
    {
        do
        {
            if((ffbFindFileBuffer.attrFile & (MUST_HAVE_SYSTEM | MUST_HAVE_HIDDEN)))
              continue;

            if(ffbFindFileBuffer.attrFile & MUST_HAVE_DIRECTORY)
            {
                if((strcmpi(&ffbFindFileBuffer. achName[CCHMAXPATHCOMP], ".") != 0) && (strcmpi(&ffbFindFileBuffer.achName[CCHMAXPATHCOMP], "..") != 0))
                {
                    ParsePath(DirName, szBuf, sizeof(szBuf), PP_PATH_ONLY);
                    AppendBackSlash(szBuf, sizeof(szBuf));
                    strcat(szBuf, &ffbFindFileBuffer. achName[CCHMAXPATHCOMP]);

                    TmpTotalFileSize  = *TotalFileSize;
                    TmpTotalAllocSize = *TotalAllocSize;
                    GetDirSize(szBuf,
                               Level,
                               Sector,
                               AddDirectorySize,
                               AutoInfoFormat,
                               CurrentLevel + 1,
                               TotalDirSize,
                               TotalDirCount,
                               TotalFileSize,
                               TotalAllocSize);
                    TmpTotalFileSize  += *TotalFileSize;
                    TmpTotalAllocSize += *TotalAllocSize;

                    *TotalFileSize  = TmpTotalFileSize;
                    *TotalAllocSize = TmpTotalAllocSize;
                }
            }
            else
            {
                llFileSize      = ffbFindFileBuffer.cbFile;
                *TotalFileSize += llFileSize;

                if(llFileSize <= Sector)
                    *TotalAllocSize += Sector;
                else
                {
                    // get the ceiling of llFileSize/Sector
                    if((llFileSize % Sector) == 0)
                        *TotalAllocSize += Sector * ((llFileSize / Sector));
                    else
                        *TotalAllocSize += Sector * ((llFileSize / Sector) + 1);
                }
            }
        } while(DosFindNext(hDir, &ffbFindFileBuffer, sizeof(FILEFINDBUF3), &ulFindCount));

        DosFindClose(hDir);
    }

    *TotalDirSize   += Sector;
    *TotalDirCount  += 1;
    if(AddDirectorySize == 1)
        *TotalAllocSize += Sector;

    if(CurrentLevel <= Level)
    {
        if(*TotalFileSize == 0)
        {
            if(AutoInfoFormat == 0)
            {
                printf("                   0 -                    0 - %s\n", InputName);
            }
            else
            {
                if(Level == 0)
                {
                    printf("0\n");
                }
                else
                {
                    printf("                   0\n");
                }
            }
        }
        else
        {
            if(AutoInfoFormat == 0)
            {
                printf("%20.0I64u - %20.0I64u - %s\n", *TotalFileSize, *TotalAllocSize, InputName);
            }
            else
            {
                if(Level == 0)
                {
                    printf("%I64u\n", *TotalAllocSize);
                }
                else
                {
                    printf("%20.0I64u\n", *TotalAllocSize);
                }
            }
        }
    }
}

void PrintUsage()
{
    printf("Usage: ds32.exe [/? /h /L <number> /C <number> /S /A directory]\n\n");
    printf("       /?          : this usage\n");
    printf("       /h          : this usage\n");
    printf("       /L <number> : the depth of directory to display info for\n");
    printf("       /C <number> : calculate allocation space using this number\n");
    printf("       /S          : Suppress header information\n");
    printf("       /A          : Allocation information only (bare format)\n");
    printf("       /D          : Add size of directories to calculation (1 directory takes up a cluster size of space)\n");
    printf("       directory   : the directory to calcualte\n");
}

void PrintBadArg(char *String)
{
    printf("Error: bad argument %s\n", String);
    PrintUsage();
    exit(1);
}

void PrintBadDir(char *String)
{
    printf("Error: bad directory %s\n", String);
    PrintUsage();
    exit(1);
}

char *ParseForUNCSourceDrivePath(char *DirName, char *NewDirName)
{
    int QuitCountingFlag;
    int NameAndVolume;
    int BackSlashCntr;
    int charposition;
    int count;
    int numchars;

    // Test for UNC path
    if((DirName[0] == '\\') && (DirName[1] == '\\'))
    {
        QuitCountingFlag = 1;               // Stop incrementing NameAndVolume
        NameAndVolume    = 0;               // Initialize string length
        BackSlashCntr    = 2;               // Backslash count is two
        charposition     = 1;               // Initialize position in string to the second position
        numchars         = strlen(DirName); //Get the total length of UNC Path

        // Count the character position and the number of backslashes
        for(count = 2; count < numchars; count++)
        {
            ++charposition;
            if(DirName[charposition] == '\\')
                ++BackSlashCntr;

            // Save the number of chars equal to the Computer name and volume
            if((BackSlashCntr == 4) && QuitCountingFlag)
            {
                NameAndVolume    = charposition;
                QuitCountingFlag = 0;
            }
        }
        if(NameAndVolume == 0)
            NameAndVolume = numchars;

        // Save the UNC path
        memcpy(NewDirName, DirName, NameAndVolume);
    }
    else
        memcpy(NewDirName, DirName, 2);

    return(NewDirName);
}


int ParseArgumentList(struct Params *ArgList, int argc, char *argv[])
{
    int i;

    ArgList->ArgNum         = argc;
    ArgList->Level          = -1;
    ArgList->Sector         = -1;
    ArgList->SuppressHeader = 0;
    ArgList->AutoInfoFormat = 0;
    ArgList->DirName        = (char *)malloc(sizeof(char)*MAX_BUF);
    memset(ArgList->DirName, 0, MAX_BUF);

    for(i = 1; i < argc; i++)
    {
        // Check for /L parameter
        if((argv[i][0] == '/') || (argv[i][0] == '-'))
        {
            switch(tolower(argv[i][1]))
            {
                case 'l':
                    if(argv[i][2] != '\0')
                    {
                        if(ArgList->Level == -1)
                            if((ArgList->Level = atol(&(argv[i][2]))) == 0)
                                if(isdigit(argv[i][2]) == 0)
                                    PrintBadArg(argv[i]);
                    }
                    else
                    {
                        ++i;
                        if(ArgList->Level == -1)
                            if((ArgList->Level = atol(argv[i])) == 0)
                                if(isdigit(argv[i][0]) == 0)
                                    PrintBadArg(argv[--i]);
                    }
                    continue;

                case 'c':
                    if(argv[i][2] != '\0')
                    {
                        if(ArgList->Sector == -1)
                            if((ArgList->Sector = atol(&(argv[i][2]))) == 0)
                                PrintBadArg(argv[i]);
                    }
                    else
                    {
                        ++i;
                        if(ArgList->Sector == -1)
                            if((ArgList->Sector = atol(argv[i])) == 0)
                                PrintBadArg(argv[--i]);
                    }
                    continue;

                case 's':
                    if(argv[i][2] == '\0')
                    {
                        if(ArgList->SuppressHeader == 0)
                            ArgList->SuppressHeader = 1;
                    }
                    else
                    {
                        PrintBadArg(argv[i]);
                    }
                    continue;

                case 'd':
                    if(argv[i][2] == '\0')
                    {
                        ArgList->AddDirectorySize = 1;
                    }
                    else
                    {
                        PrintBadArg(argv[i]);
                    }
                    continue;

                case 'a':
                    if(argv[i][2] == '\0')
                    {
                        if(ArgList->AutoInfoFormat == 0)
                            ArgList->AutoInfoFormat = 1;
                    }
                    else
                    {
                        PrintBadArg(argv[i]);
                    }
                    continue;

                case 'h':
                case '?':
                    PrintUsage();
                    exit(0);
            }
        }

        // Check for existance of directory or file
        if(access(argv[i], 0) == 0)
        {
            if(ArgList->DirName[0] == '\0')
                memcpy(ArgList->DirName, argv[i], strlen(argv[i]));
        }
        else
            PrintBadDir(argv[i]);
    }
    return(0);
}

int main(int argc, char *argv[])
{
    ULONG     SectorsPerCluster = 0;
    ULONG     FreeClusters      = 0;
    ULONG     Clusters          = 0;
    ULONG     BytesPerSector    = 0;
    unsigned int  CurrentLevel      = 0;
    ULONG     TotalDirSize      = 0;
    ULONG     TotalDirCount     = 0;
    ULONG     TotalFileSize     = 0;
    ULONG     TotalAllocSize    = 0;
    char          UNCPath[256];
    UCHAR          szBuf[256] = "";
    struct Params ArgList;
    int rc;

    ULONG   cbDirPathLen    = 0;
    ULONG   ulDriveNum   = 0;

    memset(UNCPath, 0, sizeof(UNCPath));

    if(ParseArgumentList(&ArgList, argc, argv))
        return(1);
    else
    {
        if(ArgList.DirName[strlen(ArgList.DirName) - 1] == '\\')
            ArgList.DirName[strlen(ArgList.DirName) - 1] = '\0';

        if(ArgList.DirName[0] == '\0')
        {   
            cbDirPathLen = (ULONG) sizeof(szBuf);
            rc = DosQueryCurrentDir(ulDriveNum, ArgList.DirName, &cbDirPathLen);
            if(rc != NO_ERROR)
            {
                perror("GetCurrentDirectory()");
                printf("Error: could not get current working directory rc=%d\n", rc);
                exit(1);
            }
        }

        if((ArgList.DirName[1] == ':') || ((ArgList.DirName[0] == '\\') && (ArgList.DirName[1] == '\\')))
            ParseForUNCSourceDrivePath(ArgList.DirName, UNCPath);

        if((ArgList.DirName[1] != ':') && (ArgList.DirName[0] != '\\'))
        {
            cbDirPathLen = (ULONG) sizeof(szBuf);
            rc = DosQueryCurrentDir(ulDriveNum, (PBYTE)szBuf, &cbDirPathLen);
            if(rc != NO_ERROR)
            {
                perror("GetCurrentDirectory()");
                printf("Error: could not get current working directory rc=%d len=%d\n", rc, cbDirPathLen);
                exit(1);
            }
            ParsePath((PSZ)szBuf, UNCPath, sizeof(UNCPath), PP_ROOT_ONLY);
        }

        AppendBackSlash(UNCPath, sizeof(UNCPath));

        if(ArgList.Sector == -1)
        {
            GetDiskFreeSpace(UNCPath, &SectorsPerCluster, &BytesPerSector, &FreeClusters, &Clusters);
            ArgList.Sector = SectorsPerCluster * BytesPerSector;
        }

        if(ArgList.SuppressHeader == 0)
        {
            printf("  Root Path Name           : %s\n",   UNCPath);
            printf("  Sectors Per Cluster      : %d\n",   SectorsPerCluster);
            printf("  Bytes Per Sectors        : %d\n",   BytesPerSector);
            printf("  Bytes Per Cluster       *: %d\n",   ArgList.Sector);
            printf("  Free Clusters            : %d\n",   FreeClusters);
            printf("  Total Number Of Clusters : %d\n\n", Clusters);
        }

        GetDirSize(ArgList.DirName,
                   ArgList.Level,
                   ArgList.Sector,
                   ArgList.AddDirectorySize,
                   ArgList.AutoInfoFormat,
                   CurrentLevel,
                   &TotalDirSize,
                   &TotalDirCount,
                   &TotalFileSize,
                   &TotalAllocSize);

        if(ArgList.AutoInfoFormat == 0)
        {
            if(ArgList.AddDirectorySize == 1)
            {
                printf("                       %d - total size of directories\n", TotalDirSize);
                printf("                       %d - number of directories\n",    TotalDirCount);
            }

            printf("--------------------   --------------------\n");
            printf("         total files             allocation\n");
        }
    }
    free(ArgList.DirName);
    return(0);
}

BOOL GetDiskFreeSpace(PSZ lpRootPathName, PULONG lpSectorsPerCluster,
                      PULONG lpBytesPerSector, PULONG lpNumberOfFreeClusters,
                      PULONG lpTotalNumberOfClusters)
{
    FSALLOCATE fsAlloc;       // Returned file system info.
    ULONG ulDiskNum;          // Disk to query.
    BOOL rc = FALSE;          // Return code.
    char * drive;

    if (lpRootPathName && *lpRootPathName)
    {
        // convert drive name to drive number.
        // a=1, b=2 c=3, etc...
        drive = strupr((char*)lpRootPathName);
        if (drive[0] >= 'A' && drive[0] <= 'Z')
        {
                          ulDiskNum = drive[0] - 'A';
                          ulDiskNum += 1 ;
        }
        else
        {
           // Use the default drive.
           ulDiskNum = 0;
        }

        // Call DosQueryFSInfo to retrieve level 1 information (fsil_alloc)
        // about the disk.  The info is returned in an FSALLOCATE structure.
        rc = DosQueryFSInfo(ulDiskNum, FSIL_ALLOC, &fsAlloc, sizeof(fsAlloc));
        if (rc == NO_ERROR)
        {
            printf( "fsAlloc:\ncSectorUnit=%d, cbSector=%d, cUnitAvail=%d\n", fsAlloc.cSectorUnit, fsAlloc.cbSector, fsAlloc.cUnitAvail );
            *lpSectorsPerCluster       = fsAlloc.cSectorUnit;
            *lpBytesPerSector          = fsAlloc.cbSector;
            *lpNumberOfFreeClusters    = fsAlloc.cUnitAvail;
            *lpTotalNumberOfClusters   = fsAlloc.cUnit;
        }
        else
        {
            *lpSectorsPerCluster       = 0;
            *lpBytesPerSector          = 0;
            *lpNumberOfFreeClusters    = 0;
            *lpTotalNumberOfClusters   = 0;
        }
    }
    return(rc);
}

