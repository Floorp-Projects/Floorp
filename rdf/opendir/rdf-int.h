/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rdf.h"
#define true 1
#define false 0

#define null NULL
#define nullp(x) (((void*)x) == ((void*)0))
#define noRDFErr 0
#define noMoreValuesErr 1

#define LookupResource(x) ((RDF_Resource)PL_HashTableLookup(resourceHash, x));
#define stringEquals(x, y) (strcmp(x, y) ==0)

#define wsCharp(c) ((c == '\r') || (c == '\t') || (c == ' ') || (c == '\n'))
#define RDF_BUF_SIZE (4096 * 16)
#define MAX_ATTRIBUTES 256
#define EXPECTING_OBJECT 1
#define EXPECTING_PROPERTY 2
#define  GROW_LIST_INCR 1000

typedef struct _RDF_ResourceStruct {
  char* url;
  struct _RDF_AssertionStruct* rarg1;
  struct _RDF_AssertionStruct* rarg2;
} RDF_ResourceStruct ;




typedef enum {
  GET_SLOT_VALUES,
  ARC_LABELS_IN,
  ARC_LABELS_OUT,
  SEARCH
} QueryType;


typedef struct _RDF_FileStruct {
  char* url;
  int   status;
  char* storeAway;
  char* line;
  char* holdOver;
  RDF_Resource stack[256];
  RDF_Resource lastItem;
  int   depth ;
  int   tagDepth;
  int   assertionListCount;
  int   assertionListSize;
  struct _RDF_AssertionStruct** assertionList;
  char* nsList[20];
  int   nsStackPoint;
} RDF_FileStruct;



typedef struct _RDF_CursorStruct {
  RDF_Resource u; 
  RDF_Resource s;
  void *value;
  void* pdata;
  void* pdata1;
  RDF_Resource* pdata2;
  int inversep;
  RDF_ValueType type;
  size_t count;
  size_t off;
  size_t off1;
  RDFT db;
  QueryType queryType;
  char* searchString;
} RDF_CursorStruct;



typedef struct _RDF_AssertionStruct {
  RDF_Resource u;
  RDF_Resource s;
  void* value;
  RDF_ValueType type;
  struct _RDF_AssertionStruct* next;
  struct _RDF_AssertionStruct* invNext;
  RDFT db;
} RDF_AssertionStruct;


typedef RDF_AssertionStruct* Assertion;
typedef struct _HashTableStruct* HashTable;

HashTable  NewHashTable(int size) ;
void* HashLookup(HashTable ht, char* key) ;
void HashAdd (HashTable ht, char* key, void* value) ;

void readRDFFile (char* file) ;
RDF_Resource getResource(char* url, int createp);
char* getMem(size_t n);
char* fgetMem (size_t size) ;
void  freeMem(void* item);
RDFT initFileStruct (char* url) ;
void rdf_init();
int startsWith (const char* pattern, const char* uuid);
char decodeEntityRef (char* string, int* stringIndexPtr, int len);
char * copyStringIgnoreWhiteSpace(char* string);
char * getHref(char** attlist);
int parseNextRDFXMLBlobInt(RDFT f, char* blob, int size) ;
char * getAttributeValue (char** attlist, char* elName);
int tagEquals (RDFT f, char* tag1, char* tag2);
void addElementProps (char** attlist, char* elementName, RDFT f, RDF_Resource obj);
int knownObjectElement (char* eln);
char *possiblyMakeAbsolute (RDFT f, char* url);
int containerTagp (RDFT f, char* elementName);
RDF_Resource ResourceFromElementName (RDFT f, char* elementName);
int parseNextRDFToken (RDFT f, char* token);
int tokenizeElement (char* attr, char** attlist, char** elementName);
void addSlotValue (RDFT f, RDF_Resource u, RDF_Resource s, void* v, 
                   RDF_ValueType type, char* op);
char* copyString(char* str) ;
char*  fcopyString (char* str) ;




int asEqual(RDFT r, Assertion as, RDF_Resource u, RDF_Resource s, void* v, 
	       RDF_ValueType type);
Assertion makeNewAssertion (RDFT r, RDF_Resource u, RDF_Resource s, void* v, 			
    RDF_ValueType type, int tv);
void freeAssertion (Assertion as);
Assertion remoteStoreAdd (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v,
 RDF_ValueType type, int tv);
Assertion remoteStoreRemove (RDFT mcf, RDF_Resource u, RDF_Resource s,  void* v, 
RDF_ValueType type);
int remoteStoreHasAssertion (RDFT mcf, RDF_Resource u, RDF_Resource s, void* v, 
RDF_ValueType type, int tv);
void * getSlotValue (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, 
int inversep,  int tv);
RDF_Cursor getSlotValues (RDFT mcf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  
int inversep, int tv);
void* nextValue (RDF_Cursor c) ;
void disposeCursor (RDF_Cursor c);
void unloadRDFT (RDFT f);
int rdf_DigestNewStuff (char* fileName, char* data, int len) ;
RDFT getRDFT(char* url, int createp);
char** processRDFQuery (char* query);
