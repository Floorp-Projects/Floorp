/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include "secrng.h"
#include "prerror.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <sys/stat.h>

static BOOL clockTickTime(unsigned long *phigh, unsigned long *plow)
{
    APIRET rc = NO_ERROR;
    QWORD qword = {0,0};

    rc = DosTmrQueryTime(&qword);
    if (rc != NO_ERROR)
       return FALSE;

    *phigh = qword.ulHi;
    *plow  = qword.ulLo;

    return TRUE;
}

size_t RNG_GetNoise(void *buf, size_t maxbuf)
{
    unsigned long high = 0;
    unsigned long low  = 0;
    clock_t val = 0;
    int n = 0;
    int nBytes = 0;
    time_t sTime;

    if (maxbuf <= 0)
       return 0;

    clockTickTime(&high, &low);

    /* get the maximally changing bits first */
    nBytes = sizeof(low) > maxbuf ? maxbuf : sizeof(low);
    memcpy(buf, &low, nBytes);
    n += nBytes;
    maxbuf -= nBytes;

    if (maxbuf <= 0)
       return n;

    nBytes = sizeof(high) > maxbuf ? maxbuf : sizeof(high);
    memcpy(((char *)buf) + n, &high, nBytes);
    n += nBytes;
    maxbuf -= nBytes;

    if (maxbuf <= 0)
       return n;

    /* get the number of milliseconds that have elapsed since application started */
    val = clock();

    nBytes = sizeof(val) > maxbuf ? maxbuf : sizeof(val);
    memcpy(((char *)buf) + n, &val, nBytes);
    n += nBytes;
    maxbuf -= nBytes;

    if (maxbuf <= 0)
       return n;

    /* get the time in seconds since midnight Jan 1, 1970 */
    time(&sTime);
    nBytes = sizeof(sTime) > maxbuf ? maxbuf : sizeof(sTime);
    memcpy(((char *)buf) + n, &sTime, nBytes);
    n += nBytes;

    return n;
}

static BOOL
EnumSystemFiles(void (*func)(const char *))
{
    APIRET              rc;
    ULONG               sysInfo = 0;
    char                bootLetter[2];
    char                sysDir[_MAX_PATH] = "";
    char                filename[_MAX_PATH];
    HDIR                hdir = HDIR_CREATE;
    ULONG               numFiles = 1;
    FILEFINDBUF3        fileBuf = {0};
    ULONG               buflen = sizeof(FILEFINDBUF3);

    if (DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, (PVOID)&sysInfo,
                        sizeof(ULONG)) == NO_ERROR)
    {
      bootLetter[0] = sysInfo + 'A' -1;
      bootLetter[1] = '\0';
      strcpy(sysDir, bootLetter);
      strcpy(sysDir+1, ":\\OS2\\");

      strcpy( filename, sysDir );
      strcat( filename, "*.*" );
    }

    rc =DosFindFirst( filename, &hdir, FILE_NORMAL, &fileBuf, buflen,
                      &numFiles, FIL_STANDARD );
    if( rc == NO_ERROR )
    {
      do {
        // pass the full pathname to the callback
        sprintf( filename, "%s%s", sysDir, fileBuf.achName );
        (*func)(filename);

        numFiles = 1;
        rc = DosFindNext( hdir, &fileBuf, buflen, &numFiles );
        if( rc != NO_ERROR && rc != ERROR_NO_MORE_FILES )
          printf( "DosFindNext errod code = %d\n", rc );
      } while ( rc == NO_ERROR );

      rc = DosFindClose(hdir);
      if( rc != NO_ERROR )
        printf( "DosFindClose error code = %d", rc );
    }
    else
      printf( "DosFindFirst error code = %d", rc );

    return TRUE;
}

static int    dwNumFiles, dwReadEvery, dwFileToRead=0;

static void
CountFiles(const char *file)
{
    dwNumFiles++;
}

static void
ReadFiles(const char *file)
{
    if ((dwNumFiles % dwReadEvery) == 0)
        RNG_FileForRNG(file);

    dwNumFiles++;
}

static void 
ReadSingleFile(const char *filename)
{
    unsigned char buffer[1024];
    FILE *file; 
    
    file = fopen((char *)filename, "rb");
    if (file != NULL) {
	while (fread(buffer, 1, sizeof(buffer), file) > 0) 
	    ;
	fclose(file);
    }
}

static void
ReadOneFile(const char *file)
{
    if (dwNumFiles == dwFileToRead) {
        ReadSingleFile(file);
    }

    dwNumFiles++;
}

static void
ReadSystemFiles(void)
{
    // first count the number of files
    dwNumFiles = 0;
    if (!EnumSystemFiles(CountFiles))
        return;

    RNG_RandomUpdate(&dwNumFiles, sizeof(dwNumFiles));

    // now read 10 files
    if (dwNumFiles == 0)
        return;

    dwReadEvery = dwNumFiles / 10;
    if (dwReadEvery == 0)
        dwReadEvery = 1;  // less than 10 files

    dwNumFiles = 0;
    EnumSystemFiles(ReadFiles);
}

void RNG_SystemInfoForRNG(void)
{
   unsigned long *plong = 0;
   PTIB ptib;
   PPIB ppib;
   APIRET rc = NO_ERROR;
   DATETIME dt;
   COUNTRYCODE cc = {0};
   COUNTRYINFO ci = {0};
   unsigned long actual = 0;
   char path[_MAX_PATH]="";
   char fullpath[_MAX_PATH]="";
   unsigned long pathlength = sizeof(path);
   FSALLOCATE fsallocate;
   FILESTATUS3 fstatus;
   unsigned long defaultdrive = 0;
   unsigned long logicaldrives = 0;
   unsigned long sysInfo[QSV_MAX] = {0};
   char buffer[20];
   int nBytes = 0;

   nBytes = RNG_GetNoise(buffer, sizeof(buffer));
   RNG_RandomUpdate(buffer, nBytes);
   
   /* allocate memory and use address and memory */
   plong = (unsigned long *)malloc(sizeof(*plong));
   RNG_RandomUpdate(&plong, sizeof(plong));
   RNG_RandomUpdate(plong, sizeof(*plong));
   free(plong);

   /* process info */
   rc = DosGetInfoBlocks(&ptib, &ppib);
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(ptib, sizeof(*ptib));
      RNG_RandomUpdate(ppib, sizeof(*ppib));
   }

   /* time */
   rc = DosGetDateTime(&dt);
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(&dt, sizeof(dt));
   }

   /* country */
   rc = DosQueryCtryInfo(sizeof(ci), &cc, &ci, &actual);
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(&cc, sizeof(cc));
      RNG_RandomUpdate(&ci, sizeof(ci));
      RNG_RandomUpdate(&actual, sizeof(actual));
   }

   /* current directory */
   rc = DosQueryCurrentDir(0, path, &pathlength);
   strcat(fullpath, "\\");
   strcat(fullpath, path);
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(fullpath, strlen(fullpath));
      // path info
      rc = DosQueryPathInfo(fullpath, FIL_STANDARD, &fstatus, sizeof(fstatus));
      if (rc == NO_ERROR)
      {
         RNG_RandomUpdate(&fstatus, sizeof(fstatus));
      }
   }

   /* file system info */
   rc = DosQueryFSInfo(0, FSIL_ALLOC, &fsallocate, sizeof(fsallocate));
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(&fsallocate, sizeof(fsallocate));
   }

   /* drive info */
   rc = DosQueryCurrentDisk(&defaultdrive, &logicaldrives);
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(&defaultdrive, sizeof(defaultdrive));
      RNG_RandomUpdate(&logicaldrives, sizeof(logicaldrives));
   }

   /* system info */
   rc = DosQuerySysInfo(1L, QSV_MAX, (PVOID)&sysInfo, sizeof(ULONG)*QSV_MAX);
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(&sysInfo, sizeof(sysInfo));
   }

   // now let's do some files
   ReadSystemFiles();

   /* more noise */
   nBytes = RNG_GetNoise(buffer, sizeof(buffer));
   RNG_RandomUpdate(buffer, nBytes);
}

void RNG_FileForRNG(const char *filename)
{
    struct stat stat_buf;
    unsigned char buffer[1024];
    FILE *file = 0;
    int nBytes = 0;
    static int totalFileBytes = 0;
    
    if (stat((char *)filename, &stat_buf) < 0)
       return;

    RNG_RandomUpdate((unsigned char*)&stat_buf, sizeof(stat_buf));
    
    file = fopen((char *)filename, "r");
    if (file != NULL)
    {
       for (;;)
       {
           size_t bytes = fread(buffer, 1, sizeof(buffer), file);

           if (bytes == 0)
              break;

           RNG_RandomUpdate(buffer, bytes);
           totalFileBytes += bytes;
           if (totalFileBytes > 250000)
              break;
       }
       fclose(file);
    }

    nBytes = RNG_GetNoise(buffer, 20); 
    RNG_RandomUpdate(buffer, nBytes);
}

static void rng_systemJitter(void)
{
    dwNumFiles = 0;
    EnumSystemFiles(ReadOneFile);
    dwFileToRead++;
    if (dwFileToRead >= dwNumFiles) {
	dwFileToRead = 0;
    }
}

size_t RNG_SystemRNG(void *dest, size_t maxLen)
{
    return rng_systemFromNoise(dest,maxLen);
}
