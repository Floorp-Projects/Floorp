/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include <secrng.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stat.h>

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

void RNG_SystemInfoForRNG(void)
{
   unsigned long *plong = 0;
   PTIB ptib;
   PPIB ppib;
   APIRET rc = NO_ERROR;
   DATETIME dt;
   COUNTRYCODE cc;
   COUNTRYINFO ci;
   unsigned long actual;
   char path[_MAX_PATH]="";
   unsigned long pathlength = sizeof(path);
   FSALLOCATE fsallocate;
   FILESTATUS3 fstatus;
   unsigned long defaultdrive = 0;
   unsigned long logicaldrives = 0;
   unsigned long counter = 0;
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
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(path, strlen(path));
      // path info
      rc = DosQueryPathInfo(path, FIL_STANDARD, &fstatus, sizeof(fstatus));
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
   rc = DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &counter, sizeof(counter));
   if (rc == NO_ERROR)
   {
      RNG_RandomUpdate(&counter, sizeof(counter));
   }

   /* more noise */
   nBytes = RNG_GetNoise(buffer, sizeof(buffer));
   RNG_RandomUpdate(buffer, nBytes);
}

void RNG_FileForRNG(char *filename)
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
