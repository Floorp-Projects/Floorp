/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
  
/**
 * MODULE NOTES:
 * @update  jevering 06/18/98
 * 
 * This file contains the parser debugger object which aids in
 * walking links and reporting statistic information, reporting
 * bad vectors.
 */

#include "CNavDTD.h"
#include "nsHTMLTokens.h"
#include "nsParser.h"
#include "nsIParserDebug.h"
#include "nsCRT.h"
#include "prenv.h"  //this is here for debug reasons...
#include "prtypes.h"  //this is here for debug reasons...
#include "prio.h"
#include "plstr.h"
#include "prstrm.h"
#include <fstream.h>
#include <time.h>
#include "prmem.h"

#define CONTEXT_VECTOR_MAP	"/vector.map"
#define CONTEXT_VECTOR_STAT	"/vector.stat"
#define VECTOR_TABLE_HEADER "count  vector\r\n====== =============================================\r\n"    

// structure to store the vector statistic information

typedef struct vector_info {
    PRInt32 references;     // number of occurances counted
    PRInt32 count;          // number of tags in the vector
    PRBool  good_vector;    // is this a valid vector?
    eHTMLTags* vector;       // and the vector
} VectorInfo;

// the statistic vector table grows each time it exceeds this
// stepping value
#define TABLE_SIZE	128

class CParserDebug : public nsIParserDebug {
public:

    CParserDebug(char * aVerifyDir = 0);
    ~CParserDebug();

    NS_DECL_ISUPPORTS

    void SetVerificationDirectory(char * verify_dir);
    void SetRecordStatistics(PRBool bval);
    PRBool Verify(nsIDTD * aDTD,  nsParser * aParser, int ContextStackPos, eHTMLTags aContextStack[], char * aURLRef);
    void DumpVectorRecord(void);

    // global table for storing vector statistics and the size

private:
    VectorInfo ** mVectorInfoArray;
    PRInt32 mVectorCount;
    char * mVerificationDir;
    PRBool mRecordingStatistics;

    PRBool DebugRecord(char * path, char * pURLRef, char * filename);
    void NoteVector(eHTMLTags aTags[],PRInt32 count, PRBool good_vector);
    void MakeVectorString(char * vector_string, VectorInfo * pInfo);
};

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
static NS_DEFINE_IID(kIDebugParserIID, NS_IPARSERDEBUG_IID);

/**
 *  This method is defined in nsIParser. It is used to 
 *  cause the COM-like construction of an nsParser.
 *  
 *  @update  jevering 3/25/98
 *  @param   nsIParser** ptr to newly instantiated parser
 *  @return  NS_xxx error result
 */

NS_EXPORT nsresult NS_NewParserDebug(nsIParserDebug** aInstancePtrResult)
{
  CParserDebug *it = new CParserDebug();

  if (it == 0) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(kIDebugParserIID, (void **)aInstancePtrResult);
}

CParserDebug::CParserDebug(char * aVerifyDir)
{
   NS_INIT_REFCNT();
   mVectorInfoArray = 0;
   mVectorCount = 0;
   if (aVerifyDir)
     mVerificationDir = PL_strdup(aVerifyDir);
   else {
     char * pString = PR_GetEnv("VERIFY_PARSER");
     if (pString)
        mVerificationDir = PL_strdup(pString);
     else
        mVerificationDir = 0;
   }
   mRecordingStatistics = PR_TRUE;
}

CParserDebug::~CParserDebug()
{
   if (mVerificationDir)
      PL_strfree(mVerificationDir);
}

/**
 *  This method gets called as part of our COM-like interfaces.
 *  Its purpose is to create an interface to parser object
 *  of some type.
 *  
 *  @update   gess 4/8/98
 *  @param    nsIID  id of object to discover
 *  @param    aInstancePtr ptr to newly discovered interface
 *  @return   NS_xxx result code
 */
nsresult CParserDebug::QueryInterface(const nsIID& aIID, void** aInstancePtr)  
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      

  if(aIID.Equals(kISupportsIID))    {  //do IUnknown...
    *aInstancePtr = (nsIParserDebug*)(this);                                        
  }
  else if(aIID.Equals(kIDebugParserIID)) {  //do IParserDebug base class...
    *aInstancePtr = (nsIParserDebug*)(this);                                        
  }
  else {
    *aInstancePtr=0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;                                                        
}

NS_IMPL_ADDREF(CParserDebug)
NS_IMPL_RELEASE(CParserDebug)

void CParserDebug::SetVerificationDirectory(char * verify_dir)
{
   if (mVerificationDir) {
      PL_strfree(mVerificationDir);
      mVerificationDir = 0;
   }
	mVerificationDir = PL_strdup(verify_dir);
}

void CParserDebug::SetRecordStatistics(PRBool bval)
{
	mRecordingStatistics = bval;
}

/** 
 * This debug method records an invalid context vector and it's
 * associated context vector and URL in a simple flat file mapping which
 * resides in the verification directory and is named context.map
 *
 * @update  jevering 6/06/98
 * @param   path is the directory structure indicating the bad context vector
 * @param   pURLRef is the associated URL
 * @param   filename to record mapping to if not already recorded
 * @return  TRUE if it is already record (dont rerecord)
 */

PRBool CParserDebug::DebugRecord(char * path, char * pURLRef, char * filename)
{
   char recordPath[2048];
   PRIntn oflags = 0;

   // create the record file name from the verification director
   // and the default name.
   strcpy(recordPath,mVerificationDir);
   strcat(recordPath,CONTEXT_VECTOR_MAP);

   // create the file exists, only open for read/write
   // otherwise, create it
   if(PR_Access(recordPath,PR_ACCESS_EXISTS) != PR_SUCCESS)
      oflags = PR_CREATE_FILE;
   oflags |= PR_RDWR;

   // open the record file
   PRFileDesc * recordFile = PR_Open(recordPath,oflags,0);

   if (recordFile) {

      char * string = (char *)PR_Malloc(2048);
      PRBool found = PR_FALSE;

	  // vectors are stored on the format iof "URL vector filename"
	  // where the vector contains the verification path and
	  // the filename contains the debug source dump
      sprintf(string,"%s %s %s\r\n", pURLRef, path, filename);

	  // get the file size, read in the file and parse it line at
	  // a time to check to see if we have already recorded this
	  // occurance

      PRInt32 iSize = PR_Seek(recordFile,0,PR_SEEK_END);
      if (iSize) {

         char * buffer = (char*)PR_Malloc(iSize);
         char * stringbuf = (char*)PR_Calloc(sizeof(char*),2048);
         if (buffer!=NULL && string!=NULL) {
            PRInt32 ibufferpos, istringpos;

			// beginning of file for read
            PR_Seek(recordFile,0,PR_SEEK_SET);
            PR_Read(recordFile,buffer,iSize);

			// run through the file looking for a matching vector
            for (ibufferpos = istringpos = 0; ibufferpos < iSize; ibufferpos++)
            {
			   // compare string once we have hit the end of the line
               if (buffer[ibufferpos] == '\r') {
                  stringbuf[istringpos] = '\0';
                  istringpos = 0;
                  // skip newline and space
                  ibufferpos++;

                  if (PL_strlen(stringbuf)) {
					char * space;
   					// chop of the filename for compare
                    if ((space = PL_strrchr(stringbuf, ' '))!=NULL)
						*space = '\0';

					// we have already recorded this one, free up, and return
                    if (!PL_strncmp(string,stringbuf,PL_strlen(stringbuf))) {
						PR_Free(buffer);
                  PR_Free(stringbuf);
						PR_Free(string);
                        return PR_TRUE;
                    }
                  }
               }

               // build up the compare string
               else
                  stringbuf[istringpos++] = buffer[ibufferpos];
            }

            // throw away the record file data
            PR_Free(buffer);
            PR_Free(stringbuf);
         }
      }

      // if this bad vector was not recorded, add it to record file

      if (!found) {
         PR_Seek(recordFile,0,PR_SEEK_END);
         PR_Write(recordFile,string,PL_strlen(string));
      }

      PR_Close(recordFile);
	  PR_Free(string);
   }

   // vector was not recorded
   return PR_FALSE;
}

/**
 * compare function for quick sort.  Compares references and
 * sorts in decending order
 */

static int compare( const void *arg1, const void *arg2 )
{
	VectorInfo ** p1 = (VectorInfo**)arg1;
	VectorInfo ** p2 = (VectorInfo**)arg2;
	return (*p2)->references - (*p1)->references;
}

/**
 *  This debug routines stores statistical information about a
 *  context vector.  The context vector statistics are stored in
 *  a global array.  The table is resorted each time it grows to
 *  aid in lookup speed.  If a vector has already been noted, its
 *  reference count is bumped, otherwise it is added to the table
 *
 *  @update     jevering 6/11/98
 *  @param      aTags is the tag list (vector)
 *  @param      count is the size of the vector
 *  @return
 */

void CParserDebug::NoteVector(eHTMLTags aTags[],PRInt32 count, PRBool good_vector)
{
    // if the table doesn't exist, create it
	if (!mVectorInfoArray) {
		mVectorInfoArray = (VectorInfo**)PR_Calloc(TABLE_SIZE,sizeof(VectorInfo*));
	} 
	else {
        // attempt to look up the vector
		for (PRInt32 i = 0; i < mVectorCount; i++)

            // check the vector only if they are the same size, if they
            // match then just return without doing further work
			if (mVectorInfoArray[i]->count == count)
				if (!memcmp(mVectorInfoArray[i]->vector, aTags, sizeof(eHTMLTags)*count)) {

                    // bzzzt. and we have a winner.. bump the ref count
					mVectorInfoArray[i]->references++;
					return;
				}
	}

    // the context vector hasn't been noted, so allocate it and
    // initialize it one.. add it to the table
	VectorInfo * pVectorInfo = (VectorInfo*)PR_Malloc(sizeof(VectorInfo));
	pVectorInfo->references = 1;
	pVectorInfo->count = count;
	pVectorInfo->good_vector = good_vector;
	pVectorInfo->vector = (eHTMLTags*)PR_Malloc(count*sizeof(eHTMLTags));
	memcpy(pVectorInfo->vector,aTags,sizeof(eHTMLTags)*count);
	mVectorInfoArray[mVectorCount++] = pVectorInfo;

    // have we maxed out the table?  grow it.. sort it.. love it. 
	if ((mVectorCount % TABLE_SIZE) == 0) {
		mVectorInfoArray = (VectorInfo**)realloc(
			mVectorInfoArray,
			(sizeof(VectorInfo*)*((mVectorCount/TABLE_SIZE)+1)*TABLE_SIZE));
	  if (mVectorCount) {
		  qsort((void*)mVectorInfoArray,(size_t)mVectorCount,sizeof(VectorInfo*),compare);
	  }
	}
}

void CParserDebug::MakeVectorString(char * vector_string, VectorInfo * pInfo)
{
    sprintf (vector_string, "%6d ", pInfo->references);
    for (PRInt32 j = 0; j < pInfo->count; j++) {
	    PL_strcat(vector_string, "<");
	    PL_strcat(vector_string, (const char *)GetTagName(pInfo->vector[j]));
	    PL_strcat(vector_string, ">");
    }
    PL_strcat(vector_string,"\r\n");
}

/**
 *  This debug routine dumps out the vector statistics to a text
 *  file in the verification directory and defaults to the name
 *  "vector.stat".  It contains all parsed context vectors and there
 *  occurance count sorted in decending order.
 *  
 *  @update     jevering 6/11/98
 *  @param
 *  @return
 */

void CParserDebug::DumpVectorRecord(void)
{
    // do we have a table?
  if (mVectorCount) {

      // hopefully, they wont exceed 1K.
    char vector_string[1024];
    char path[1024];

    path[0] = '\0';

    // put in the verification directory.. else the root
    if (mVerificationDir)
       strcpy(path,mVerificationDir);

    strcat(path,CONTEXT_VECTOR_STAT);

    // open the stat file creaming any existing stat file
    PRFileDesc * statisticFile = PR_Open(path,PR_CREATE_FILE|PR_RDWR,0);
		if (statisticFile) {

      PRInt32 i;
      PRofstream ps;
      ps.attach(statisticFile);

      // oh what the heck, sort it again
      if (mVectorCount) {
	      qsort((void*)mVectorInfoArray,(size_t)mVectorCount,sizeof(VectorInfo*),compare);
      }

      // cute little header
      sprintf(vector_string,"Context vector occurance results. Processed %d unique vectors.\r\n\r\n", mVectorCount);
      ps << vector_string;

      ps << "Invalid context vector summary (see " CONTEXT_VECTOR_STAT ") for mapping.\r\n";
      ps << VECTOR_TABLE_HEADER;

      // dump out the bad vectors encountered
      for (i = 0; i < mVectorCount; i++) {
        if (!mVectorInfoArray[i]->good_vector) {
          MakeVectorString(vector_string, mVectorInfoArray[i]);
          ps << vector_string;
        }
      }

      ps << "\r\n\r\nValid context vector summary\r\n";
      ps << VECTOR_TABLE_HEADER;

      // take a big vector table dump (good vectors)
      for (i = 0; i < mVectorCount; i++) {
        if (mVectorInfoArray[i]->good_vector) {
          MakeVectorString(vector_string, mVectorInfoArray[i]);
          ps << vector_string;
        }
          // free em up.  they mean nothing to me now (I'm such a user)

        if (mVectorInfoArray[i]->vector)
           PR_Free(mVectorInfoArray[i]->vector);
        PR_Free(mVectorInfoArray[i]);
      } //for
      PR_Close(statisticFile);
    }//if

      // ok, we are done with the table, free it up as well
    PR_Free(mVectorInfoArray);
    mVectorInfoArray = 0;
    mVectorCount = 0;

  } //if
}


/**
 * This debug method allows us to determine whether or not 
 * we've seen (and can handle) the given context vector.
 *
 * @update  gess4/22/98
 * @param   tags is an array of eHTMLTags
 * @param   count represents the number of items in the tags array
 * @param   aDTD is the DTD we plan to ask for verification
 * @return  TRUE if we know how to handle it, else false
 */

PRBool CParserDebug::Verify(nsIDTD * aDTD,  nsParser * aParser, int aContextStackPos, eHTMLTags aContextStack[], char * aURLRef) 
{
   PRBool  result=PR_TRUE;

    //ok, now see if we understand this vector

   if(0!=mVerificationDir || mRecordingStatistics) {

      if(aDTD && aContextStackPos>1) {
         for (int i = 0; i < aContextStackPos-1; i++)
            if (!aDTD->CanContain(aContextStack[i],aContextStack[i+1])) {
               result = PR_FALSE;
               break;
            }
         }
   }

   if (mRecordingStatistics) {
	   NoteVector(aContextStack,aContextStackPos,result);
   }

   if(0!=mVerificationDir) {
      char    path[2048];
      strcpy(path,mVerificationDir);

      int i=0;      
      for(i=0;i<aContextStackPos;i++){
         strcat(path,"/");
         const char* name=GetTagName(aContextStack[i]);
         strcat(path,name);
         PR_MkDir(path,0);
      }
      if(PR_FALSE==result){
         static PRBool rnd_initialized = PR_FALSE;

         if (!rnd_initialized) {
            // seed randomn number generator to aid in temp file
            // creation.
            rnd_initialized = PR_TRUE;
            srand((unsigned)time(NULL));
         }

         // generate a filename to dump the html source into
         char filename[1024];
         do {
            // use system time to generate a temporary file name
            time_t ltime;
            time (&ltime);
            // add in random number so that we can create uniques names
            // faster than simply every second.
            ltime += (time_t)rand();
            sprintf(filename,"%s/%lX.html", path, ltime);
            // try until we find one we can create
         } while (PR_Access(filename,PR_ACCESS_EXISTS) == PR_SUCCESS);

         // check to see if we already recorded an instance of this particular
         // bad vector.  
         if (!DebugRecord(path, aURLRef, filename))
         {
            // save file to directory indicated by bad context vector
            PRFileDesc * debugFile = PR_Open(filename,PR_CREATE_FILE|PR_RDWR,0);
            // if we were able to open the debug file, then
            // write the true URL at the top of the file.
            if (debugFile) {
               // dump the html source into the newly created file.
               PRofstream ps;
               ps.attach(debugFile);
               if (aParser)
                  aParser->DebugDumpSource(ps);
               PR_Close(debugFile);
            }
         }
      }
   }

   return result;
}
