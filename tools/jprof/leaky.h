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
#include "libmalloc.h"
#include "strset.h"
#include "intcnt.h"

typedef unsigned int u_int;

struct Symbol;
struct leaky;

class FunctionCount : public IntCount
{
public:
    void printReport(FILE *fp, leaky *lk);
};

struct Symbol {
  char* name;
  u_long address;
  int    timerHit;
  FunctionCount cntP, cntC;

  int regChild(int id) {return cntC.countAdd(id, 1);}
  int regParrent(int id) {return cntP.countAdd(id, 1);}

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
  void open();

  char*  applicationName;
  char*  logFile;
  char*  progFile;

  int   quiet;
  int   showAddress;
  u_int  stackDepth;

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

  StrSet roots;
  StrSet includes;

  void usageError();

  void LoadMap();

  void analyze();

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
  void generateReportHTML(FILE *fp, int *countArray, int count);
  int  findSymbolIndex(u_long address);
};

#endif /* __leaky_h_ */
