/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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



#ifndef	_RDF_RDF_INT_H
#define	_RDF_RDF_INT_H


#define WINTEST 0
#define DBMTEST 1 
#define FSTEST 1


#define	RDF_BUF_SIZE	4096
#define LINE_SIZE     512
#define RDF_BOOKMARKS 128
#define RDF_MCF       129
#define RDF_XML       130

#include <stdlib.h>
#include <string.h>
#include "rdf.h"
#include "nspr.h"
#include "plhash.h"
#include "ntypes.h"
#include "net.h"
#include "xp.h"
#include "xp_str.h"

XP_BEGIN_PROTOS

#ifndef XP_WIN32
#include "prefapi.h"
#endif

#ifndef true
#define true PR_TRUE
#endif
#ifndef false
#define false PR_FALSE
#endif
#define null NULL
#define nullp(x) (((void*)x) == ((void*)0))
 

#define noRDFErr 0
#define noMoreValuesErr 1

#define MAX_ATTRIBUTES 10

#define RDF_RT 0
#define LFS_RT 1
#define FTP_RT  3
#define ES_RT 4
#define SEARCH_RT  5
#define HISTORY_RT 6
#define LDAP_RT 7
#define PM_RT 8
#define RDM_RT 9
#define	IM_RT	10
#define CACHE_RT 11
#define	ATALK_RT 12
#define	ATALKVIRTUAL_RT 13
#define COOKIE_RT       14
#ifdef TRANSACTION_RECEIPTS
#define RECEIPT_RT 15
#endif


#define CHECK_VAR(var, return_value) {if (var == NULL) {XP_ASSERT(var); return return_value;}}
#define CHECK_VAR1(var) {if (var == NULL) {XP_ASSERT(var); return;}}



#define MAX_URL_SIZE 300

#define copyString(source) (source != NULL ? XP_STRDUP(source) : NULL)
#define stringEquals(x, y) (strcasecomp(x, y) ==0)
/*#define stringAppend(x, y)   XP_AppendStr(x,y) */

struct RDF_ResourceStruct {
  char* url;
  uint8  type;  
  uint8  flags;
  struct RDF_AssertionStruct* rarg1;
  struct RDF_AssertionStruct* rarg2;
  struct RDF_ListStruct*          rdf;
} ;

#define RDF_GET_SLOT_VALUES_QUERY 0x01
#define RDF_GET_SLOT_VALUE_QUERY 0x02
#define RDF_FIND_QUERY 0x03
#define RDF_ARC_LABELS_IN_QUERY 0x04
#define RDF_ARC_LABELS_OUT_QUERY 0x05

struct RDF_CursorStruct {
  RDF_Resource u;
  RDF_Resource s;
  RDF_Resource match;
  void *value;
  struct RDF_CursorStruct* current;
  RDF    rdf;
  void* pdata;
  PRBool tv;
  PRBool inversep;
  RDF_ValueType type;
  int16 count;
  uint16 size;
  uint8 queryType;
};

typedef uint8 RDF_BT;

#define getMem(x) PR_Calloc(1,(x))
#define freeMem(x) PR_Free((x))

struct RDF_AssertionStruct {
  RDF_Resource u;
  RDF_Resource s;
  void* value;
  PRBool tv;
  RDF_ValueType type;
  uint8 tags;
  struct RDF_AssertionStruct* next;
  struct RDF_AssertionStruct* invNext;
  struct RDF_TranslatorStruct* db;
} ;


typedef struct RDF_AssertionStruct *Assertion;
typedef struct RDF_FileStruct *RDFFile;


RDF	newNavCenterDB();
void	walkThroughAllBookmarks (RDF_Resource u);
typedef PRBool (*assertProc)(RDFT r, RDF_Resource u, RDF_Resource  s, void* value, RDF_ValueType type, PRBool tv);
typedef PRBool (*hasAssertionProc)(RDFT r, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
typedef void* (*getSlotValueProc)(RDFT r, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv) ;
typedef RDF_Cursor (*getSlotValuesProc)(RDFT r, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep, PRBool tv);
typedef PRBool (*unassertProc)(RDFT r, RDF_Resource u, RDF_Resource s, void* value, RDF_ValueType type);
typedef void* (*nextItemProc)(RDFT r, RDF_Cursor c) ;
typedef RDF_Error (*disposeCursorProc)(RDFT r, RDF_Cursor c);
typedef RDF_Error (*disposeResourceProc)(RDFT r, RDF_Resource u);
typedef RDF_Error (*destroyProc)(struct RDF_TranslatorStruct*);
typedef RDF_Cursor (*arcLabelsOutProc)(RDFT r, RDF_Resource u);
typedef RDF_Cursor (*arcLabelsInProc)(RDFT r, RDF_Resource u);
typedef PRBool (*fAssert1Proc) (RDFFile  file, RDFT mcf,  RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv) ;
typedef void (*accessFileProc) (RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep) ;
struct RDF_ListStruct {
  struct RDF_DBStruct*   rdf;
  struct RDF_ListStruct* next;
} ;

typedef struct RDF_ListStruct *RDFL;

struct RDF_TranslatorStruct {
  RDFL             rdf;
  char*            url;
  void*            pdata;
  destroyProc      destroy;
  hasAssertionProc hasAssertion;
  unassertProc     unassert;
  assertProc       assert;
  getSlotValueProc getSlotValue;
  getSlotValuesProc getSlotValues;
  nextItemProc      nextValue;
  disposeCursorProc disposeCursor;
  disposeResourceProc disposeResource;
  arcLabelsInProc   arcLabelsIn;
  arcLabelsInProc   arcLabelsOut;
  accessFileProc    possiblyAccessFile;
};


extern     PLHashTable*  resourceHash;  
extern     PLHashTable*  dataSourceHash;  
struct RDF_DBStruct {
  int16 numTranslators;
  int16 translatorArraySize;
  RDFT*  translators;
  struct RDF_FileStruct* files;
  struct RDF_NotificationStruct* notifs;
};

extern RDFT gLocalStore;
extern RDFT gRemoteStore;
extern RDFT gSessionDB;
extern size_t gCoreVocabSize;
extern RDF_Resource* gAllVocab;

/* reading rdf */

#define HEADERS 1
#define BODY    2


struct XMLNameSpaceStruct {
  char* as;
  char* url;
  struct XMLNameSpaceStruct* next;
} ;

typedef struct XMLNameSpaceStruct *XMLNameSpace;

#define	RDF_MAX_NUM_FILE_TOKENS		8

struct	RDF_FileStructTokens {
	RDF_Resource	token;
	RDF_ValueType	type;
	int16		tokenNum;
	char		*data;
};

struct RDF_FileStruct {
  char* url;
  RDF_Resource currentResource;
  RDF_Resource top;
  RDF_Resource rtop;
  char*  currentSlot;
  PRBool genlAdded;
  PRBool localp;
  char* storeAway;
  char* line;
  uint16   status;
  uint16   resourceCount;
  uint16   resourceListSize;
  uint16   assertionCount;
  uint16   assertionListSize;
  RDF_Resource* resourceList;
  Assertion* assertionList;
  char* holdOver;
  PRTime lastReadTime;
  PRTime *expiryTime;
  RDF_Resource stack[16];
  uint16   depth ;
  uint8   fileType;
  PRBool  locked;
  RDF_Resource lastItem;
  int32   lineSize;
  struct RDF_FileStruct* next;
  PRBool tv;
  fAssert1Proc assert;
  RDFT     db;
  void* pdata;
  XMLNameSpace namespaces;
  int16	numFileTokens;
  struct RDF_FileStructTokens tokens[RDF_MAX_NUM_FILE_TOKENS];
};


RDFT NewRemoteStore (char* url);
RDF_Resource nextFindValue (RDF_Cursor c) ;
PRBool isTypeOf (RDF rdf, RDF_Resource u,  RDF_Resource v); 
RDF getRDFDB (void);
RDFFile readRDFFile (char* url, RDF_Resource top, PRBool localp, RDFT rdf);
void sendNotifications (RDF rdf, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv, char* ds);
void sendNotifications2 (RDFT rdf, RDF_EventType opType, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv) ;
RDF_Error exitRDF (RDF rdf);
void parseNextBkBlob(RDFFile f, char* blob, int32 blobSize);
void printAssertion(Assertion as);
RDF_Error addChildAfter (RDFT rdf, RDF_Resource parent, RDF_Resource child, RDF_Resource afterWhat);
RDF_Error addChildBefore (RDFT rdf, RDF_Resource parent, RDF_Resource child, RDF_Resource beforeWhat);
void nlclStoreKill(RDFT rdf, RDF_Resource r);
char* resourceID(RDF_Resource id);
PRBool containerp (RDF_Resource r);
void   setContainerp (RDF_Resource r, PRBool contp);
PRBool lockedp (RDF_Resource r);
void   setLockedp (RDF_Resource r, PRBool contp);
uint8  resourceType (RDF_Resource r);
void   setResourceType (RDF_Resource r, uint8 type);
char* getBaseURL (const char* url) ;
void freeNamespaces (RDFFile f) ;
PRBool		asEqual(RDFT r, Assertion as, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
Assertion	makeNewAssertion (RDFT r, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);

RDFT MakeRemoteStore (char* url);
PRDir * OpenDir(char *name);

void readResourceFile(RDF rdf, RDF_Resource u);
void possiblyGCResource(RDF_Resource u);
PLHashNumber idenHash(const void* key);

/* string utilities */

int16 charSearch(const char c,const  char* string);
int16 revCharSearch(const char c, const char* string);
PR_PUBLIC_API(PRBool) startsWith(const char* startPattern,const  char* string);
PRBool endsWith(const char* endPattern,const  char* string);
PRBool inverseTV(PRBool tv);
void createBootstrapResources();
PRBool urlEquals(const char* url1,const  char* url2);
char* append2Strings(const char* str1,const  char* str2);
PRBool substring(const char* pattern, const char* data);
void stringAppend(char* in, const char* append);

RDFFile makeRDFFile (char* url, RDF_Resource top, PRBool localp);
void initRDFFile (RDFFile ans) ;
void parseNextRDFLine (RDFFile f, char* line) ;
void parseNextRDFBlob (RDFFile f, char* blob, int32 size);
void finishRDFParse (RDFFile f) ;
void abortRDFParse (RDFFile f);
void unitTransition (RDFFile f) ;	
void assignHeaderSlot (RDFFile f, char* slot, char* value);			
RDF_Error getFirstToken (char* line, char* nextToken, int16* l) ;
void addSlotValue (RDFFile f, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void assignSlot (RDF_Resource u, char* slot, char* value, RDFFile f);
RDF_Error parseSlotValue (RDFFile f, RDF_Resource s, char* value, void** parsed_value, RDF_ValueType* data_type) ;
RDF_Resource resolveReference (char *tok, RDFFile f) ;
RDF_Resource resolveGenlPosReference(char* tok,  RDFFile f);
RDF_Resource resolveReference (char *tok, RDFFile f) ;
RDF_Resource resolveGenlPosReference(char* tok,  RDFFile f);
void beginReadingRDFFile(RDFFile f);
void gcRDFFile (RDFFile f);
#ifdef XP_MAC
char* unescapeURL(char* inURL);
#endif
char* convertFileURLToNSPRCopaceticPath(char* inURL);
PRFileDesc* CallPROpenUsingFileURL(char *fileURL, PRIntn flags, PRIntn mode);
DB *CallDBOpenUsingFileURL(char *fileURL, int flags,int mode, DBTYPE type, const void *openinfo);

PRBool nlocalStoreAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, 
			 RDF_ValueType type, PRBool tv) ;

void initLocalStore(RDFT rdf);
void freeAssertion(Assertion as);
Assertion localStoreAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv)  ;
Assertion localStoreRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type)  ;
 
Assertion remoteStoreAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv) ;
Assertion remoteStoreRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type) ;
void* remoteStoreGetSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv) ;
void ht_fprintf(PRFileDesc *file, const char *fmt, ...);

RDF_Resource createContainer (char* id);
PRBool isContainer (RDF_Resource r) ;
PRBool isSeparator (RDF_Resource r) ;
PRBool isLeaf (RDF_Resource r) ;
PRBool bookmarkSlotp (RDF_Resource s) ;

void basicAssertions (void) ;
char* makeResourceName (RDF_Resource node) ;
char* makeDBURL (char* name);

int rdf_GetURL (MWContext *cx, int method, Net_GetUrlExitFunc *exit_routine, RDFFile rdfFile);


PRBool nlocalStoreAssert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv) ;
PRBool nlocalStoreUnassert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type) ;
PRBool nlocalStoreHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv) ;
void* nlocalStoreGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv) ;
RDF_Cursor nlocalStoreGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv) ;
void* nlocalStoreNextValue (RDFT mcf, RDF_Cursor c) ;
RDF_Error nlocalStoreDisposeCursor (RDFT mcf, RDF_Cursor c) ;
void createCoreVocab () ;


PRBool remoteAssert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv) ;
PRBool remoteUnassert (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type) ;
PRBool remoteAssert1 (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv) ;
PRBool remoteAssert2 (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		     RDF_ValueType type, PRBool tv) ;
PRBool remoteUnassert1 (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
		       RDF_ValueType type) ;
PRBool remoteStoreHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv) ;
void* remoteStoreGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv) ;
RDF_Cursor remoteStoreGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv) ;
RDF_Cursor remoteStoreGetSlotValuesInt (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv) ;
RDF_Resource createSeparator(void);

void* remoteStoreNextValue (RDFT mcf, RDF_Cursor c) ;
RDF_Error remoteStoreDisposeCursor (RDFT mcf, RDF_Cursor c) ;
PRBool remoteStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv) ;
PRBool remoteStoreHasAssertionInt (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv) ;
PRBool  nlocalStoreAddChildAt(RDFT mcf, RDF_Resource obj, RDF_Resource ref, RDF_Resource _new, 
		      PRBool beforep);

RDFT MakeCookieStore (char* url);

char* advertURLOfContainer (RDF r, RDF_Resource u) ;
RDFT RDFTNamed (RDF rdf, char* name) ;



extern RDF_WDVocab gWebData;
extern RDF_NCVocab gNavCenter;
extern RDF_CoreVocab gCoreVocab;

XP_END_PROTOS
      
#endif
