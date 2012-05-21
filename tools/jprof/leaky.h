/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __leaky_h_
#define __leaky_h_

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "libmalloc.h"
#include "strset.h"
#include "intcnt.h"

typedef unsigned int u_int;

struct Symbol;
struct leaky;

class FunctionCount : public IntCount
{
public:
  void printReport(FILE *fp, leaky *lk, int parent, int total);
};

struct Symbol {
  char* name;
  u_long address;
  int    timerHit;
  FunctionCount cntP, cntC;

  int regChild(int id) {return cntC.countAdd(id, 1);}
  int regParrent(int id) {return cntP.countAdd(id, 1);}
  void regClear() {cntC.clear(); cntP.clear();}

  Symbol() : timerHit(0) {}
  void Init(const char* aName, u_long aAddress) {
    name = aName ? strdup(aName) : (char *)"";
    address = aAddress;
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
  void open(char *arg);

  char*  applicationName;
  int    logFileIndex;
  int    numLogFiles;
  char*  progFile;
  FILE*  outputfd;

  bool  quiet;
  bool  showAddress;
  bool  showThreads;
  bool  cleo;
  u_int stackDepth;
  int   onlyThread;
  char* output_dir;

  int   mappedLogFile;
  malloc_log_entry* firstLogEntry;
  malloc_log_entry* lastLogEntry;

  int    stacks;

  int sfd;
  Symbol* externalSymbols;
  int     usefulSymbols;
  int     numExternalSymbols;
  StrSet exclusions;
  u_long lowestSymbolAddr;
  u_long highestSymbolAddr;

  LoadMapEntry* loadMap;

  bool collect_last;
  int  collect_start;
  int  collect_end;

  StrSet roots;
  StrSet includes;

  void usageError();

  void LoadMap();

  void analyze(int thread);

  void dumpEntryToLog(malloc_log_entry* lep);

  void insertAddress(u_long address, malloc_log_entry* lep);
  void removeAddress(u_long address, malloc_log_entry* lep);

  void displayStackTrace(FILE* out, malloc_log_entry* lep);

  void ReadSymbols(const char* fileName, u_long aBaseAddress);
  void ReadSharedLibrarySymbols();
  void setupSymbols(const char* fileName);
  Symbol* findSymbol(u_long address);
  bool excluded(malloc_log_entry* lep);
  bool included(malloc_log_entry* lep);
  const char* indexToName(int idx) {return externalSymbols[idx].name;}

  private:
  void generateReportHTML(FILE *fp, int *countArray, int count, int thread);
  int  findSymbolIndex(u_long address);
};

#endif /* __leaky_h_ */
