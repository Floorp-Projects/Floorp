/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
Subject: 
    Date: 
        Thu, 11 Jan 2001 16:38:10 -0800
   From: 
        Doug Turner <dougt@z.netscape.com>
     To: 
        gordon@netscape.com
*/



#include "nscore.h"
#include "nspr.h"
#include "mcom_db.h"
#include "nsString.h"

DB* myDB;

#define DATASIZE 512
#define ENTRYCOUNT 512
#define USE_ENTRY_ID 1

int
testDBM(int cycles)
{

    // create database file
    HASHINFO hash_info = {
        16*1024 , /* bucket size */
            0 ,       /* fill factor */
            0 ,       /* number of elements */
            0 ,       /* bytes to cache */
            0 ,       /* hash function */
            0} ;      /* byte order */
    
    
    myDB = dbopen("/tmp/foodb",
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
    
    
    // write key-data pairs to database
    DBT db_key, db_data ;
    
    for (x=1; x<=ENTRYCOUNT; x++) {
        nsCAutoString keyName("foo");
        keyName.AppendInt( x );
        
        db_key.data = (char*)keyName;
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
        db_data.data = (char*)keyName;
        db_data.size = keyName.Length();

        if(0 != (*myDB->put)(myDB, &db_key, &db_data, 0)) {
            printf("--> Error putting\n");
            return -1;
        }
#endif
    }
    
    (*myDB->sync)(myDB, 0); 
    free(data);

    // begin timing "lookups"
    int status = 0 ;
    PRIntervalTime time = PR_IntervalNow();
    
    while (cycles--) {
        for (x=1; x<=ENTRYCOUNT; x++) {
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
        }
    }
    (*myDB->sync)(myDB, 0); 
    (*myDB->close)(myDB); 
    
    return PR_IntervalToMilliseconds( PR_IntervalNow() - time);
}


int
testFile(int cycles)
{
    FILE* file;
    int fStatus;
    int x;
    char * data = (char*) malloc(DATASIZE);
    for (x=0; x<DATASIZE; x++)
        *data = '\0';
    
    // create "cache" directories
    mkdir("/tmp/foo", 0755);
    
    for (x=0; x<32; x++) {
        nsCAutoString filename; filename.Assign("/tmp/foo/");
        filename.AppendInt(x);
        mkdir(filename, 0755);
    }

    // create "cache" files
    for (x=1; x<=ENTRYCOUNT; x++) {
        nsCAutoString filename; filename.Assign("/tmp/foo/");
        filename.AppendInt( x % 32 );
        filename.Append("/");
        filename.AppendInt( x );
        
        file = fopen(filename, "w");
        if (!file)
            printf("bad filename?  %s\n", (char*)filename);
        
        fStatus = fwrite (data, DATASIZE, 1, file);  
        if (fStatus == -1) {
            printf("Bad fStatus %d\n", errno);
            exit(1);
        } 
        fclose(file);
    }

    // begin timing "lookups"
    PRIntervalTime time = PR_IntervalNow();
    while (cycles--) {
        for (x=1; x<=ENTRYCOUNT; x++) {
            nsCAutoString filename; filename.Assign("/tmp/foo/");
            filename.AppendInt( x % 32 );
            filename.Append("/");
            filename.AppendInt( x );
            
            file = fopen(filename, "a+");
            
            int size = ftell(file);
            rewind(file);
            
            char* fdBuffer = (char*) malloc (size);
            
            fStatus = fread (fdBuffer, size, 1, file);  
            
            if (fStatus == -1) {
                printf("Bad fStatus %d\n", errno);
                exit(1);
            } 
            
            fclose(file);
            free (fdBuffer);
        }
    }
    return PR_IntervalToMilliseconds( PR_IntervalNow() - time);
}


void
main(void)
{
    int totalDBMTime  = testDBM(32);
    int totalFileTime = testFile(32);

    printf("total dbm IO  ---- > (%d) milliseconds\n", totalDBMTime);
    printf("total file IO ---- > (%d) milliseconds\n", totalFileTime);
}

