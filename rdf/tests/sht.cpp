/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
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

#include "rdf.h"
#include "rdfutil.h"
#include "prtypes.h"
#include "prmem.h"
#include "nscore.h"
#include "nsIOutputStream.h"
#include "plhash.h"
#include "plstr.h"

typedef struct _SerializedHashTable {
  FILE* file;
  PLHashTable* table;
  char*   buffer;
  size_t  maxEntrySize;
} SerializedHashTable;

typedef SerializedHashTable* SHT;

typedef struct _SHTEntry {
  void*       value;
  size_t      size;
  size_t      offset;
} SHTEntry;

typedef SHTEntry* SHTE;

void 
dumpHashItem (SHT table, char* key, size_t keyLen, void* value, size_t valueSize) {
  fprintf(table->file, "A %i\n%s\n%i\n", keyLen, key, valueSize);
  fwrite(value, 1, valueSize, table->file);
  fprintf(table->file, "\n");
}

PRBool
readNextItem(FILE *file, char** key, char** value, size_t *valueSize, PRBool *ignorep, 
	     int *offset) {
  char *xkey, *xvalue;
  int keySize;
  char ignoreChar;
  char tbuff[10];
  *offset = ftell(file);
  if (!fgets((char*)tbuff, 10, file)) return 0;
  if (tbuff[0] == 'X') return 0;
  sscanf(tbuff, "%c %i", &ignoreChar, &keySize);
  xkey = (char*) PR_Malloc(keySize + 1);
  if (!fgets(xkey, keySize+1, file)) return 0;
  fread(tbuff, 1, 1, file);
  memset(tbuff, '\0', 10);
  if (!fgets((char*)tbuff, 10, file)) return 0;
  sscanf(tbuff, "%i", valueSize);
  xvalue = (char*) PR_Malloc(*valueSize + 1);
  xvalue[*valueSize] = '\0';
  if (!fread(xvalue, *valueSize, 1, file)) return 0;
  fread(tbuff, 1, 1, file);
  if (ignoreChar == 'D') {
    PR_Free((void*)xvalue);
    PR_Free((void*)xkey);
    *ignorep = 1;
    return 1;
  } else {
    *ignorep = 0;
    *value = xvalue;
    *key = xkey;
    return 1;
  }
}


PRIntn 
SHTFlusher(PLHashEntry *he, PRIntn index, void *arg) {
  SHT table = (SHT)arg;
  SHTE entry = (SHTE)he->value;
  char* key = (char*)he->key;  
  char* v1  = (char*)entry->value;
  dumpHashItem(table, key, strlen(key), entry->value, entry->size);
  return HT_ENUMERATE_NEXT;
}
		
void 
FlushSHT (SHT table) {
  
  table->buffer = (char*)PR_Malloc(table->maxEntrySize + 256);
  fseek(table->file, 0, SEEK_SET);
  PL_HashTableEnumerateEntries(table->table, SHTFlusher, table);
  fwrite("X", 1, 1, table->file);
  fflush(table->file);
  PR_Free(table->buffer);
}

PLHashEntry* 
SHTAddInt (PLHashTable* table, size_t valueSize, void* value, char* key, int offset) {
  SHTE  ve    = (SHTE) PR_Malloc(sizeof(SHTEntry));
  ve->value = value;
  ve->size  = valueSize;
  ve->offset = offset;
  return PL_HashTableAdd(table, key, ve);
}

SHT
MakeSerializedHash (FILE* file, PRUint32 numBuckets, PLHashFunction keyHash,
		    PLHashComparator keyCompare, PLHashComparator valueCompare,
		    const PLHashAllocOps *allocOps, void *allocPriv) {
  SHT table = (SHT)PR_Malloc(sizeof(SerializedHashTable));
  char* key;
  char* value;
  size_t valueSize;
  PRBool ignorep = 0;
  int offset;
  table->table = PL_NewHashTable(numBuckets, keyHash,
				 keyCompare, valueCompare,
				 allocOps, allocPriv);
  table->file = file;
  table->maxEntrySize = 1024;
  while (readNextItem(file, &key, &value, &valueSize, &ignorep, &offset)) {
    SHTAddInt(table->table, valueSize, value, key, offset);
  }
  return table;
}
    
void
SHTAdd(SHT table,  char *key, void *value, size_t valueSize) {  
  fseek(table->file, 0, SEEK_END);
  SHTFlusher(SHTAddInt(table->table,valueSize, value, key, ftell(table->file)), 0, table);
  fflush(table->file);
}

void
SHTRemove(SHT table, char* key) {
  SHTE entry = (SHTE)PL_HashTableLookup(table->table, (void*) key);
  if (entry) {
	  fseek(table->file, entry->offset, SEEK_SET);
	  fwrite("D", 1, 1, table->file);
	  fflush(table->file);
	  PL_HashTableRemove(table->table, key);
	  PR_Free(entry); 
  }  
}

void*
SHTLookup(SHT ht, const char *key) {
  SHTE entry = (SHTE)PL_HashTableLookup(ht->table, (void*) key);
  if (entry) {
    return entry->value;
  } else return 0;
}

FILE*
MakeOrOpenFile (char* name) {
	FILE* ans;
	ans = fopen(name, "r+");
	if (ans) return ans;
	ans = fopen(name, "w");
	fclose(ans);
	return fopen(name, "r+");
}

void SHTtest () {
  FILE *f = MakeOrOpenFile("test");
  SHT table = MakeSerializedHash(f, 100, PL_HashString, PL_CompareStrings, 
                                 PL_CompareValues,   NULL, NULL);
  char buffer[1024];
  printf("\n>");
  while (fgets(buffer, 1023, stdin)) {
    if (strncmp(buffer, "x", 1) == 0) {
      FlushSHT(table);
      return;
    }
    if (strncmp(buffer, "a", 1) ==0) {
      char key[100], value[100];
	  char *xkey, *xvalue;
      sscanf(buffer, "a %s %s", key, value);
      xkey = (char*)PR_Malloc(strlen(key)+1);
	  xvalue = (char*)PR_Malloc(strlen(value)+1);
	  memset(xkey, '\0', strlen(key)+1);
	  memset(xvalue, '\0', strlen(value)+1);
	  memcpy(xkey, key, strlen(key));
	  memcpy(xvalue, value, strlen(value));
      SHTAdd(table, xkey, xvalue, strlen(xvalue));
    } 
    if (strncmp(buffer, "q", 1) ==0) {
      char key[100];
      char* ans;
      sscanf(buffer, "q %s", key);
      ans = (char*)SHTLookup(table, (char*)key);
      printf("-> %s\n", (ans ? ans : "null"));
    }
	if (strncmp(buffer, "r", 1) == 0) {
	  char key[100];
      sscanf(buffer, "r %s", key);
	  SHTRemove(table, key);
	}
    printf("\n>");
  }
}

    
    


  
