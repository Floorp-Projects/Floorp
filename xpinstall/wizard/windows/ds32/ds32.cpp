/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Sean Su <ssu@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <io.h>

#define MAXLEVEL 30000
#define MAX_BUF  4096

/* PP: Parse Path */
#define PP_FILENAME_ONLY                1
#define PP_PATH_ONLY                    2
#define PP_ROOT_ONLY                    3

void GetDirSize(         char *InputName,
                unsigned int   Level,
                ULONGLONG      Sector,
                         int   AddDirectorySize,
                         int   AutoInfoFormat,
                unsigned int   CurrentLevel,
                ULONGLONG     *TotalDirSize,
                ULONGLONG     *TotalDirCount,
                ULONGLONG     *TotalFileSize,
                ULONGLONG     *TotalAllocSize);

char DirDepth[MAX_BUF];

struct Params
{
    unsigned long ArgNum;
    unsigned int  Level;
    ULONGLONG     Sector;
             int  SuppressHeader;
             int  AutoInfoFormat;
             int  AddDirectorySize;
    char         *DirName;
};

void RemoveBackSlash(LPSTR szInput)
{
  int   iCounter;
  DWORD dwInputLen;

  if(szInput != NULL)
  {
    dwInputLen = lstrlen(szInput);

    for(iCounter = dwInputLen -1; iCounter >= 0 ; iCounter--)
    {
      if(szInput[iCounter] == '\\')
        szInput[iCounter] = '\0';
      else
        break;
    }
  }
}

void AppendBackSlash(LPSTR szInput, DWORD dwInputSize)
{
  if(szInput != NULL)
  {
    if(*szInput == '\0')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
    else if(szInput[strlen(szInput) - 1] != '\\')
    {
      if(((DWORD)lstrlen(szInput) + 1) < dwInputSize)
      {
        lstrcat(szInput, "\\");
      }
    }
  }
}

void ParsePath(LPSTR szInput, LPSTR szOutput, DWORD dwOutputSize, DWORD dwType)
{
  int   iCounter;
  DWORD dwCounter;
  DWORD dwInputLen;
  BOOL  bFound;

  if((szInput != NULL) && (szOutput != NULL))
  {
    bFound        = TRUE;
    dwInputLen    = lstrlen(szInput);
    ZeroMemory(szOutput, dwOutputSize);

    if(dwInputLen < dwOutputSize)
    {
      switch(dwType)
      {
        case PP_FILENAME_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, &szInput[iCounter + 1]);
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

          break;

        case PP_PATH_ONLY:
          for(iCounter = dwInputLen - 1; iCounter >= 0; iCounter--)
          {
            if(szInput[iCounter] == '\\')
            {
              lstrcpy(szOutput, szInput);
              szOutput[iCounter + 1] = '\0';
              bFound = TRUE;
              break;
            }
          }
          if(bFound == FALSE)
            lstrcpy(szOutput, szInput);

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
void GetDirSize(         char *InputName,
                unsigned int   Level,
                ULONGLONG      Sector,
                         int   AddDirectorySize,
                         int   AutoInfoFormat,
                unsigned int   CurrentLevel,
                ULONGLONG     *TotalDirSize,
                ULONGLONG     *TotalDirCount,
                ULONGLONG     *TotalFileSize,
                ULONGLONG     *TotalAllocSize)
{
    HANDLE          hDir;
    WIN32_FIND_DATA wfdFindFileData;
    ULONGLONG       llFileSize;
    ULONGLONG       TmpTotalFileSize;
    ULONGLONG       TmpTotalAllocSize;
    int             InputNameLen = strlen(InputName);
    int             DirNameLen   = InputNameLen;
    char            DirName[MAX_BUF];
    char            szBuf[MAX_BUF];

    memset(DirName, '\0', sizeof(DirName));
    memcpy(DirName, InputName, InputNameLen);

    *TotalFileSize  = 0;
    *TotalAllocSize = 0;

    AppendBackSlash(DirName, sizeof(DirName));
    lstrcat(DirName, "*.*");
    if((hDir = FindFirstFile(DirName, &wfdFindFileData)) != INVALID_HANDLE_VALUE)
    {
        do
        {
            if((wfdFindFileData.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
              continue;

            if(wfdFindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if((lstrcmpi(wfdFindFileData.cFileName, ".") != 0) && (lstrcmpi(wfdFindFileData.cFileName, "..") != 0))
                {
                    ParsePath(DirName, szBuf, sizeof(szBuf), PP_PATH_ONLY);
                    AppendBackSlash(szBuf, sizeof(szBuf));
                    strcat(szBuf, wfdFindFileData.cFileName);

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
                llFileSize      = (wfdFindFileData.nFileSizeHigh * MAXDWORD) + wfdFindFileData.nFileSizeLow;
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
        } while(FindNextFile(hDir, &wfdFindFileData));

        FindClose(hDir);
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
    ArgList->DirName        = (char *)calloc(MAX_BUF, 1);

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
    ULONGLONG     SectorsPerCluster = 0;
    ULONGLONG     FreeClusters      = 0;
    ULONGLONG     Clusters          = 0;
    ULONGLONG     BytesPerSector    = 0;
    unsigned int  CurrentLevel      = 0;
    ULONGLONG     TotalDirSize      = 0;
    ULONGLONG     TotalDirCount     = 0;
    ULONGLONG     TotalFileSize     = 0;
    ULONGLONG     TotalAllocSize    = 0;
    char          UNCPath[MAX_BUF];
    char          szBuf[MAX_BUF];
    struct Params ArgList;

    memset(UNCPath, 0, sizeof(UNCPath));

    if(ParseArgumentList(&ArgList, argc, argv))
        return(1);
    else
    {
        if(ArgList.DirName[strlen(ArgList.DirName) - 1] == '\\')
            ArgList.DirName[strlen(ArgList.DirName) - 1] = '\0';

        if(ArgList.DirName[0] == '\0')
        {
            if(GetCurrentDirectory(MAX_BUF, ArgList.DirName) == NULL)
            {
                perror("GetCurrentDirectory()");
                printf("Error: could not get current working directory\n");
                exit(1);
            }
        }

        if((ArgList.DirName[1] == ':') || ((ArgList.DirName[0] == '\\') && (ArgList.DirName[1] == '\\')))
            ParseForUNCSourceDrivePath(ArgList.DirName, UNCPath);

        if((ArgList.DirName[1] != ':') && (ArgList.DirName[0] != '\\'))
        {
            if(GetCurrentDirectory(sizeof(szBuf), szBuf) == NULL)
            {
                perror("GetCurrentDirectory()");
                printf("Error: could not get current working directory\n");
                exit(1);
            }
            ParsePath(szBuf, UNCPath, sizeof(UNCPath), PP_ROOT_ONLY);
        }

        AppendBackSlash(UNCPath, sizeof(UNCPath));

        if(ArgList.Sector == -1)
        {
            GetDiskFreeSpace(UNCPath, (PDWORD)&SectorsPerCluster, (PDWORD)&BytesPerSector, (PDWORD)&FreeClusters, (PDWORD)&Clusters);
            ArgList.Sector = SectorsPerCluster * BytesPerSector;
        }

        if(ArgList.SuppressHeader == 0)
        {
            printf("  Root Path Name           : %s\n",   UNCPath);
            printf("  Sectors Per Cluster      : %I64u\n",   SectorsPerCluster);
            printf("  Bytes Per Sectors        : %I64u\n",   BytesPerSector);
            printf("  Bytes Per Cluster       *: %I64u\n",   ArgList.Sector);
            printf("  Free Clusters            : %I64u\n",   FreeClusters);
            printf("  Total Number Of Clusters : %I64u\n\n", Clusters);
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
                printf("                       %20.0I64u - total size of directories\n", TotalDirSize);
                printf("                       %20.0I64u - number of directories\n",    TotalDirCount);
            }

            printf("--------------------   --------------------\n");
            printf("         total files             allocation\n");
        }
    }
    free(ArgList.DirName);
    return(0);
}
