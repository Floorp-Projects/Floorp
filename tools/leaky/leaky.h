// The contents of this file are subject to the Mozilla Public License
// Version 1.1 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

#ifndef __leaky_h_
#define __leaky_h_

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "dict.h"
#include "strset.h"
#include "xpcom-config.h" // for CPP_NEW_THROW

typedef unsigned int u_int;

struct Symbol;

struct TreeNode {
  TreeNode(Symbol* aSymbol) {
    symbol = aSymbol;
    nextSibling = NULL;
    descendants = NULL;
    nextRoot = NULL;
    bytesLeaked = 0;
    descendantBytesLeaked = 0;
  }

  TreeNode* GetDirectDescendant(Symbol* aSymbol);

  bool HasDescendants() const {
    return NULL != descendants;
  }

  TreeNode* AddDescendant(Symbol* aSymbol);

  TreeNode* descendants;
  TreeNode* nextSibling;
  TreeNode* nextRoot;
  Symbol* symbol;

  u_long bytesLeaked;
  u_long descendantBytesLeaked;

  void* operator new(size_t size) CPP_THROW_NEW ;
  void operator delete(void* ptr);

  static TreeNode* freeList;
};

struct Symbol {
  char* name;
  u_long address;
  TreeNode* root;

  void Init(const char* aName, u_long aAddress) {
    name = aName ? strdup(aName) : (char *)"";
    address = aAddress;
    root = NULL;
  }
};

struct LoadMapEntry {
  char* name;			// name of .so
  u_long address;		// base address where it was mapped in
  LoadMapEntry* next;
};

struct leaky {
  leaky();
  ~leaky();

  void initialize(int argc, char** argv);
  void open();

  char*  applicationName;
  char*  logFile;
  char*  progFile;

  int   dumpLeaks;
  int   dumpGraph;
  int   dumpHTML;
  int   quiet;
  int   dumpEntireLog;
  int   showAddress;
  bool  dumpRefcnts;
  u_int  stackDepth;

  int   mappedLogFile;
  malloc_log_entry* firstLogEntry;
  malloc_log_entry* lastLogEntry;
  u_int  buckets;
  MallocDict*  dict;
  MallocDict*  refcntDict;

  u_long  mallocs;
  u_long  reallocs;
  u_long  frees;
  u_long  totalMalloced;
  u_long  errors;
  u_long  totalLeaked;

  int sfd;
  Symbol* externalSymbols;
  u_int usefulSymbols;
  u_int numExternalSymbols;
  StrSet exclusions;
  u_long lowestSymbolAddr;
  u_long highestSymbolAddr;

  LoadMapEntry* loadMap;

  TreeNode* rootList;

  StrSet roots;
  StrSet includes;

  void usageError();

  void LoadMap();

  void analyze();

  void dumpLog();
  void dumpEntryToLog(malloc_log_entry* lep);

#if 0
  void dumpToTree();
  void dumpEntryToTree(FILE* fp, malloc_log_entry* lep);
#endif

  void insertAddress(u_long address, malloc_log_entry* lep);
  void removeAddress(u_long address, malloc_log_entry* lep);

  void displayStackTrace(FILE* out, malloc_log_entry* lep);

  void ReadSymbols(const char* fileName, u_long aBaseAddress);
  void ReadSharedLibrarySymbols();
  void setupSymbols(const char* fileName);
  Symbol* findSymbol(u_long address);
  bool excluded(malloc_log_entry* lep);
  bool included(malloc_log_entry* lep);

  void buildLeakGraph();
  Symbol* findLeakGraphRoot(Symbol* aStart, Symbol* aEnd);
  void dumpLeakGraph();
  void dumpLeakTree(TreeNode* aNode, int aIndent);

  bool ShowThisEntry(malloc_log_entry* lep);

  bool IsRefcnt(malloc_log_entry* lep) const {
    return (lep->type == malloc_log_addref) ||
      (lep->type == malloc_log_release);
  }

  static void indentBy(int aCount) {
    while (--aCount >= 0) fputs("  ", stdout);
  }
};

#endif /* __leaky_h_ */
