/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is nsCacheManager.h, released January 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Doug Turner
 *    Gordon Sheridan
 */


#include "nscore.h"
#include "nspr.h"
#include "mcom_db.h"
#include "nsString.h"

#ifdef WIN32
#define TMPDIR "c:\\temp\\"
#define DIRSEP "\\"
#else
#define TMPDIR "/tmp/"
#define DIRSEP "/"
#endif

#define DATASIZE 512
#define ENTRYCOUNT 512
#define USE_ENTRY_ID 1
#define EXTRA_LOOKUP 1
#define NUM_CYCLES 32

DB* myDB;
HASHINFO hash_info = {
    16*1024 , /* bucket size */
        0 ,       /* fill factor */
        0 ,       /* number of elements */
        0 ,       /* bytes to cache */
        0 ,       /* hash function */
        0} ;      /* byte order */




int writeDBM(int cycles)
{
    printf("writeDBM\n");
    DBT db_key, db_data ;
    PRIntervalTime time = PR_IntervalNow();

    while (cycles--) {        
        // create database file
        myDB = dbopen(TMPDIR "foodb",
                      O_RDWR | O_CREAT ,
                      0600 ,
                      DB_HASH ,
                      &hash_info) ;
        
        if (!myDB) { 
            printf("no db!\n");
            return -1;
        }
        
        // initalize data to write
        int x;
        char * data = (char*) malloc(DATASIZE);
        for (x=0; x<DATASIZE; x++)
            *data = '\0';
        for (x=1; x<=ENTRYCOUNT; x++) {
            nsCAutoString keyName("foo");
            keyName.AppendInt( x );
            
            db_key.data = NS_CONST_CAST(char*, keyName.get());
            db_key.size = keyName.Length();
            
            db_data.data = data;
            db_data.size = DATASIZE ;
            
            if(0 != (*myDB->put)(myDB, &db_key, &db_data, 0)) {
                printf("--> Error putting\n");
                return -1;
            }
#if USE_ENTRY_ID
            db_key.data = (void*)&x;
            db_key.size = sizeof(x);
            db_data.data = NS_CONST_CAST(char*, keyName.get());
            db_data.size = keyName.Length();
            
            if(0 != (*myDB->put)(myDB, &db_key, &db_data, 0)) {
                printf("--> Error putting\n");
                return -1;
            }
#endif
        }
        
        (*myDB->sync)(myDB, 0); 
        free(data);
    }
    return PR_IntervalToMilliseconds( PR_IntervalNow() - time);
}


int
readDBM(int cycles)
{
    printf("readDBM\n");
    // begin timing "lookups"
    int status = 0 ;
    DBT db_key, db_data ;
    PRIntervalTime time = PR_IntervalNow();
    
    while (cycles--) {
        for (int x=1; x<=ENTRYCOUNT; x++) {
#if USE_ENTRY_ID
            DBT entry_data;

            db_key.data = (void*)&x;
            db_key.size = sizeof(x) ;
            
            status = (*myDB->get)(myDB, &db_key, &entry_data, 0);            
            if(status != 0) {
                printf("Bad Status %d\n", status);
                return -1;
            }
            db_key.data = entry_data.data;
            db_key.size = entry_data.size;
#else
            nsCAutoString keyName("foo");
            keyName.AppendInt(x);
            db_key.data = (char*)keyName;
            db_key.size = keyName.Length();
#endif
            status = (*myDB->get)(myDB, &db_key, &db_data, 0);
            if(status != 0) {
                printf("Bad Status %d\n", status);
                return -1;
            }
#if EXTRA_LOOKUP
            db_key.data = (void*)&x;
            db_key.size = sizeof(x);

            status = (*myDB->get)(myDB, &db_key, &entry_data, 0);            
            if(status != 0) {
                printf("Bad Status %d\n", status);
                return -1;
            }

#endif
        }
    }
    (*myDB->sync)(myDB, 0); 
    (*myDB->close)(myDB); 
    
    return PR_IntervalToMilliseconds( PR_IntervalNow() - time);
}


int
writeFile(int cycles)
{
    printf("writeFile\n");
    PRFileDesc *fd;
    PRInt32 fStatus;
    int x;
    char * data = (char*) malloc(DATASIZE);
    for (x=0; x<DATASIZE; x++)
        *data = '\0';

    PRIntervalTime time = PR_IntervalNow();
    while (cycles--) {
        printf("writeFile [cycle=%d]\n", cycles);

        // create "cache" directories
        PR_MakeDir(TMPDIR "foo", 0755);
        
        for (x=0; x<32; x++) {
            nsCAutoString filename; filename.Assign(TMPDIR "foo" DIRSEP);
            filename.AppendInt(x);
            PR_MakeDir(filename.get(), 0755);
        }
        
        // create "cache" files
        for (x=1; x<=ENTRYCOUNT; x++) {
            nsCAutoString filename; filename.Assign(TMPDIR "foo" DIRSEP);
            filename.AppendInt( x % 32 );
            filename.Append(DIRSEP);
            filename.AppendInt( x );

            PRIntervalTime i1, i2, i3;
            i1 = PR_IntervalNow();

            fd = PR_OpenFile(filename.get(), PR_WRONLY|PR_TRUNCATE, 0644);
            if (!fd)
                printf("bad filename?  %s\n", filename.get());

            i2 = PR_IntervalNow();

            fStatus = PR_Write(fd, data, DATASIZE);  
            if (fStatus == -1) {
                printf("Bad fStatus %d\n", PR_GetError());
                exit(1);
            } 
            i3 = PR_IntervalNow();

            PR_Close(fd);

/*
            printf(" - writing %s [topen=%d twrite=%d]\n", 
                   filename.get(),
                PR_IntervalToMilliseconds(i2 - i1), 
                PR_IntervalToMilliseconds(i3 - i2));
*/
        }
    }
    return PR_IntervalToMilliseconds(PR_IntervalNow() - time);
}


int
readFile(int cycles)
{
    printf("readFile\n");
    PRFileDesc* fd;
    PRInt32 fStatus;
    
    // begin timing "lookups"
    PRIntervalTime time = PR_IntervalNow();
    while (cycles--) {
        for (int x=1; x<=ENTRYCOUNT; x++) {
            nsCAutoString filename; filename.Assign(TMPDIR "foo" DIRSEP);
            filename.AppendInt( x % 32 );
            filename.Append(DIRSEP);
            filename.AppendInt( x );
            
            fd = PR_OpenFile(filename.get(), PR_RDONLY, 0);
            
            PRInt32 size = PR_Available(fd);
            
            char* fdBuffer = (char*) malloc (size);
            
            PRIntervalTime i1, i2, i3;
            i1 = PR_IntervalNow();

            fStatus = PR_Read(fd, fdBuffer, size);  

            i2 = PR_IntervalNow();
            
            if (fStatus == -1) {
                printf("Bad fStatus %d\n", PR_GetError());
                exit(1);
            } 
            i3 = PR_IntervalNow();
            
            PR_Close(fd);
            free(fdBuffer);

/*
            printf(" - reading %s [topen=%d tread=%d]\n", 
                (const char *) filename,
                PR_IntervalToMilliseconds(i2 - i1), 
                PR_IntervalToMilliseconds(i3 - i2));
*/
        }
    }
    return PR_IntervalToMilliseconds(PR_IntervalNow() - time);
}


int
main(void)
{
    int totalDBMTime  = writeDBM(NUM_CYCLES);
    int totalFileTime = writeFile(NUM_CYCLES);

    printf("total write dbm IO  ---- > (%d) milliseconds\n", totalDBMTime);
    printf("total write file IO ---- > (%d) milliseconds\n", totalFileTime);

    totalDBMTime  = readDBM(NUM_CYCLES);
    totalFileTime = readFile(NUM_CYCLES);

    printf("\n");
    printf("total read dbm IO  ---- > (%d) milliseconds\n", totalDBMTime);
    printf("total read file IO ---- > (%d) milliseconds\n", totalFileTime);

    return 0;
}

