// The contents of this file are subject to the Mozilla Public License
// Version 1.0 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License
// at http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
// the License for the specific language governing rights and
// limitations under the License.
//
// The Initial Developer of the Original Code is Kipp E.B. Hickman.

#include "leaky.h"
#include "dict.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

static const u_int DefaultBuckets = 10007;	// arbitrary, but prime
static const u_int MaxBuckets = 1000003;	// arbitrary, but prime

//----------------------------------------------------------------------

int main(int argc, char** argv)
{
  leaky* l = new leaky;

  l->initialize(argc, argv);
  l->open();
  return 0;
}

leaky::leaky()
{
  applicationName = NULL;
  logFile = NULL;
  progFile = NULL;

  sortByFrequency = FALSE;
  dumpAll = FALSE;
  dumpGraph = FALSE;
  dumpXML = FALSE;
  quiet = FALSE;
  showAll = FALSE;
  showAddress = FALSE;
  stackDepth = 100000;

  fd = -1;
  base = last = 0;
  buckets = DefaultBuckets;
  dict = 0;

  mallocs = 0;
  reallocs = 0;
  frees = 0;
  totalMalloced = 0;
  errors = 0;
  totalLeaked = 0;

  sfd = -1;
  externalSymbols = 0;
  usefulSymbols = 0;
  numExternalSymbols = 0;
  lowestSymbolAddr = 0;
  highestSymbolAddr = 0;

  loadMap = NULL;
}

leaky::~leaky()
{
  delete dict;
}

void leaky::usageError()
{
  fprintf(stderr,
	  "Usage: %s [-aAEdfgqx] [-e name] [-s depth] [-h hash-buckets] prog log\n",
	  (char*) applicationName);
  exit(-1);
}

void leaky::initialize(int argc, char** argv)
{
  applicationName = argv[0];
  applicationName = strrchr(applicationName, '/');
  if (!applicationName) {
    applicationName = argv[0];
  } else {
    applicationName++;
  }

  int arg;
  int errflg = 0;
  while ((arg = getopt(argc, argv, "adEe:fgh:s:tqx")) != -1) {
    switch (arg) {
      case '?':
	errflg++;
	break;
      case 'a':
	showAll = TRUE;
	break;
      case 'A':
	showAddress = TRUE;
	break;
      case 'd':
	dumpAll = TRUE;
	if (dumpGraph) errflg++;
	break;
      case 'e':
	exclusions.add(optarg);
	break;
      case 'f':
	sortByFrequency = TRUE;
	break;
      case 'g':
	dumpGraph = TRUE;
	if (dumpAll) errflg++;
	break;
      case 'h':
	buckets = atoi(optarg);
	if ((buckets < 0) || (buckets > MaxBuckets)) {
	  buckets = MaxBuckets;
	  fprintf(stderr, "%s: buckets is invalid, using %d\n",
		  (char*) applicationName,
		  buckets);
	}
	break;
      case 's':
	stackDepth = atoi(optarg);
	if (stackDepth < 2) {
	  stackDepth = 2;
	}
	break;
      case 'x':
	dumpXML = TRUE;
	break;
      case 'q':
	quiet = TRUE;
	break;
    }
  }
  if (errflg || ((argc - optind) < 2)) {
    usageError();
  }
  progFile = argv[optind++];
  logFile = argv[optind];

  dict = new MallocDict(buckets);
}

static void* mapFile(int fd, u_int flags, off_t* sz)
{
  struct stat sb;
  if (fstat(fd, &sb) < 0) {
    perror("fstat");
    exit(-1);
  }
  void* base = mmap(0, (int)sb.st_size, flags, MAP_PRIVATE, fd, 0);
  if (!base) {
    perror("mmap");
    exit(-1);
  }
  *sz = sb.st_size;
  return base;
}

void leaky::LoadMap()
{
  malloc_map_entry mme;
  char name[1000];

  int fd = ::open("malloc-map", O_RDONLY);
  if (fd < 0) {
    perror("open: malloc-map");
    exit(-1);
  }
  for (;;) {
    int nb = read(fd, &mme, sizeof(mme));
    if (nb != sizeof(mme)) break;
    nb = read(fd, name, mme.nameLen);
    if (nb != (int)mme.nameLen) break;
    name[mme.nameLen] = 0;
    if (!quiet) {
      printf("%s @ %lx\n", name, mme.address);
    }

    LoadMapEntry* lme = new LoadMapEntry;
    lme->address = mme.address;
    lme->name = strdup(name);
    lme->next = loadMap;
    loadMap = lme;
  }
  close(fd);
}

void leaky::open()
{
  LoadMap();

  setupSymbols(progFile);

  // open up the log file
  fd = ::open(logFile, O_RDONLY);
  if (fd < 0) {
    perror("open");
    exit(-1);
  }
  off_t size;
  base = (malloc_log_entry*) mapFile(fd, PROT_READ, &size);
  last = (malloc_log_entry*)((char*)base + size);

  analyze();

  if (dumpAll) {
    dumpLog();
  }
  else if (dumpGraph) {
    buildLeakGraph();
    dumpLeakGraph();
  }
  exit(0);
}

//----------------------------------------------------------------------


static ptrdiff_t symbolOrder(void const* a, void const* b)
{
  Symbol const* ap = (Symbol const *)a;
  Symbol const* bp = (Symbol const *)b;
  return ap->address - bp->address;
}

void leaky::ReadSharedLibrarySymbols()
{
  LoadMapEntry* lme = loadMap;
  while (NULL != lme) {
    ReadSymbols(lme->name, lme->address);
    lme = lme->next;
  }
}

void leaky::setupSymbols(const char *fileName)
{
  // Read in symbols from the program
  ReadSymbols(fileName, 0);

  // Read in symbols from the .so's
  ReadSharedLibrarySymbols();

  if (!quiet) {
    printf("A total of %d symbols were loaded\n", usefulSymbols);
  }

  // Now sort them
  qsort(externalSymbols, usefulSymbols, sizeof(Symbol), symbolOrder);
  lowestSymbolAddr = externalSymbols[0].address;
  highestSymbolAddr = externalSymbols[usefulSymbols-1].address;
}

// Binary search the table, looking for a symbol that covers this
// address.
Symbol* leaky::findSymbol(u_long addr)
{
  u_int base = 0;
  u_int limit = usefulSymbols - 1;
  Symbol* end = &externalSymbols[limit];
  while (base <= limit) {
    u_int midPoint = (base + limit)>>1;
    Symbol* sp = &externalSymbols[midPoint];
    if (addr < sp->address) {
      if (midPoint == 0) {
	return NULL;
      }
      limit = midPoint - 1;
    } else {
      if (sp+1 < end) {
	if (addr < (sp+1)->address) {
	  return sp;
	}
      } else {
	return sp;
      }
      base = midPoint + 1;
    }
  }
  return NULL;
}

//----------------------------------------------------------------------

int leaky::excluded(malloc_log_entry* lep)
{
  char** pcp = &lep->pcs[0];
  u_int n = lep->numpcs;
  for (u_int i = 0; i < n; i++, pcp++) {
    Symbol* sp = findSymbol((u_long) *pcp);
    if (sp && exclusions.contains(sp->name)) {
      return TRUE;
    }
  }
  return FALSE;
}

//----------------------------------------------------------------------

void leaky::displayStackTrace(malloc_log_entry* lep)
{
  char** pcp = &lep->pcs[0];
  u_int n = (lep->numpcs < stackDepth) ? lep->numpcs : stackDepth;
  for (u_int i = 0; i < n; i++, pcp++) {
    u_long addr = (u_long) *pcp;
    static char buf[20];
    char* symbolName;
    Symbol* sp = findSymbol(addr);
    if (sp) {
      symbolName = sp->name;
    }
    else {
      sprintf(buf, "<0x%lx>", addr);
      symbolName = buf;
    }
    if (showAddress) {
      printf("%s[%p] ", symbolName, *pcp);
    }
    else {
      printf("%s ", symbolName);
    }
  }
  printf("\n");
}

char* typeFromLog[] = {
    "malloc",
    "realloc",
    "free",
    "new",
    "delete",
    "addref",
    "release"
};

void leaky::dumpEntryToLog(malloc_log_entry* lep)
{
  printf("%-10s %08lx %5ld %08lx (%ld)-->",
	 typeFromLog[lep->type],
	 lep->address, lep->size, lep->oldaddress,
	 lep->numpcs);
  displayStackTrace(lep);
}

void leaky::dumpLog()
{
  if (showAll) {
    malloc_log_entry* lep = base;
    while (lep < last) {
      dumpEntryToLog(lep);
      lep = (malloc_log_entry*) &lep->pcs[lep->numpcs];
    }
  } else {
    malloc_log_entry* lep;
    dict->rewind();
    while (NULL != (lep = dict->next())) {
      if (!excluded(lep)) {
	dumpEntryToLog(lep);
      }
    }
  }
}

//----------------------------------------------------------------------

void leaky::insertAddress(u_long address, malloc_log_entry* lep)
{
  malloc_log_entry** lepp = dict->find(address);
  if (lepp) {
    assert(*lepp);
    if (!quiet) {
      printf("Address %lx allocated twice\n", address);
      displayStackTrace(lep);
    }
    errors++;
  } else {
    dict->add(address, lep);
  }
}

void leaky::removeAddress(u_long address, malloc_log_entry* lep)
{
  malloc_log_entry** lepp = dict->find(address);
  if (!lepp) {
    if (!quiet) {
      printf("Free of unallocated %lx\n", address);
      displayStackTrace(lep);
    }
    errors++;
  } else {
    dict->remove(address);
  }
}

void leaky::analyze()
{
  malloc_log_entry* lep = base;
  while (lep < last) {
    switch (lep->type) {
      case malloc_log_malloc:
      case malloc_log_new:
	mallocs++;
	if (lep->address) {
	  totalMalloced += lep->size;
	  insertAddress((u_long) lep->address, lep);
	}
	break;

      case malloc_log_realloc:
	if (lep->oldaddress) {
	  removeAddress((u_long) lep->oldaddress, lep);
	}
	if (lep->address) {
	  insertAddress((u_long) lep->address, lep);
	}
	reallocs++;
	break;

      case malloc_log_free:
      case malloc_log_delete:
	if (lep->address) {
	  removeAddress((u_long) lep->address, lep);
	}
	frees++;
	break;
    }
    lep = (malloc_log_entry*) &lep->pcs[lep->numpcs];
  }

  dict->rewind();
  while (NULL != (lep = dict->next())) {
    totalLeaked += lep->size;
  }

  if (!quiet) {
    printf("# of mallocs = %ld\n", mallocs);
    printf("# of reallocs = %ld\n", reallocs);
    printf("# of frees = %ld\n", frees);
    printf("# of errors = %ld\n", errors);
    printf("Total bytes allocated = %ld\n", totalMalloced);
    printf("Total bytes leaked = %ld\n", totalLeaked);
    printf("Average bytes per malloc = %g\n",
	   float(totalMalloced)/mallocs);
  }
}

void leaky::buildLeakGraph()
{
  // For each leak
  malloc_log_entry* lep;
  dict->rewind();
  while (NULL != (lep = dict->next())) {
    Symbol* prevSymbol = NULL;

    // For each pc in the leak
    char** pcp = &lep->pcs[0];
    u_int n = (lep->numpcs < stackDepth) ? lep->numpcs : stackDepth;
    for (u_int i = 0; i < n; i++, pcp++) {
      Symbol* currentSymbol = findSymbol((u_long) *pcp);
      if (currentSymbol) {
	currentSymbol->leaker = true;
	currentSymbol->calls++;
	if (i == 0) {
	  currentSymbol->bytesDirectlyLeaked += lep->size;
	}
	else {
	  currentSymbol->childBytesLeaked += lep->size;
	}
	if (prevSymbol) {
	  currentSymbol->AddChild(prevSymbol);
	  prevSymbol->AddParent(currentSymbol);
	}
      }
      prevSymbol = currentSymbol;
    }
  }
}

Symbol* leaky::findLeakGraphRoot(Symbol* aStart, Symbol* aEnd)
{
  while (aStart < aEnd) {
    if (aStart->leaker && !aStart->parents) {
      return aStart;
    }
    aStart++;
  }
  return NULL;
}

void leaky::dumpLeakGraph()
{ 
  if (dumpXML) {
#ifdef USE_XML
    printf("<?xml version=\"1.0\"?>\n");
    printf("<?xml-stylesheet href=\"http://klink/leaky/leaky.css\" type=\"text/css\"?>\n");
    printf("<root xmlns:html=\"http://www.w3.org/TR/REC-html40\">\n");
    printf("<html:script src=\"http://klink/leaky/leaky.js\"/>\n");
    printf("<key>\n");
    printf("Key:<html:br/>\n");
    printf("<b>Bytes directly leaked</b><html:br/>\n");
    printf("<k>Bytes leaked by descendants</k></key>\n");
#else
    printf("<html><head><title>Leaky Graph</title>\n");
    printf("<style src=\"http://klink/leaky/leaky.css\"></style>\n");
    printf("<script src=\"http://klink/leaky/leaky.js\"/></script>\n");
    printf("</head><body><div class=\"key\">\n");
    printf("Key:<br>\n");
    printf("<span class=b>Bytes directly leaked</span><br>\n");
    printf("<span class=d>Bytes leaked by descendants</span></div>\n");
#endif
  }
  Symbol* base = externalSymbols;
  Symbol* end = externalSymbols + usefulSymbols;
  while (base < end) {
    Symbol* root = findLeakGraphRoot(base, end);
    if (!root) break;
    if (root->NotDumped()) {
      root->SetDumped();
      dumpLeakTree(root, 0, true);
    }
    base = root + 1;
  }
  if (dumpXML) {
#ifdef USE_XML
    printf("</root>\n");
#else
    printf("</body></html>\n");
#endif
  }
}

void leaky::dumpLeakTree(Symbol* aSymbol, int aIndent, bool aEven)
{
#if 0
  float avgBytesLeaked =
    (float) (aSymbol->bytesDirectlyLeaked + aSymbol->childBytesLeaked) /
    (float) aSymbol->calls;
#endif

  bool haveVisibleDescendants = false;
  SymbolNode* node = aSymbol->children;
  while (node) {
    Symbol* kid = node->symbol;
    if (kid && kid->NotDumped()) {
      haveVisibleDescendants = true;
      break;
    }
    node = node->next;
  }

  if (dumpXML) {
#ifdef USE_XML
    printf("<n class=\"%s\">", aEven ? "e" : "o");
    if (haveVisibleDescendants) {
      printf("<html:img onmouseout=\"O(event);\" onmouseover=\"I(event);\" onclick=\"C(event);\" src=\"http://klink/leaky/%s.gif\"/>"
	     aIndent > 1 ? "close" : "open");
    }
    printf("<s>%s</s><b>%ld</b><k>%ld</k>\n",
	   aSymbol->name,
	   aSymbol->bytesDirectlyLeaked,
	   aSymbol->childBytesLeaked);
#else
    printf("<div class=\"n %c\">\n", aEven ? 'e' : 'o');
    if (haveVisibleDescendants) {
      printf("<img onmouseout=\"O(event);\" onmouseover=\"I(event);\" onclick=\"C(event);\" src=\"http://klink/leaky/%s.gif\">",
	     aIndent > 1 ? "close" : "open");
    }
    printf("<span class=s>%s</span><span class=b>%ld</span><span class=d>%ld</span>\n",
	   aSymbol->name,
	   aSymbol->bytesDirectlyLeaked,
	   aSymbol->childBytesLeaked);
#endif
  }
  else {
    indentBy(aIndent);
    printf("%s bytesLeaked=%ld (%ld from kids)\n", 
	   aSymbol->name,
	   aSymbol->bytesDirectlyLeaked,
	   aSymbol->childBytesLeaked);
  }

  node = aSymbol->children;
  int kidNum = 0;
  while (node) {
    Symbol* kid = node->symbol;
    if (kid && kid->NotDumped()) {
      kid->SetDumped();
      dumpLeakTree(kid, aIndent + 1, 0 == (kidNum & 1));
      kidNum++;
    }
    node = node->next;
  }

  if (dumpXML) {
#ifdef USE_XML
    printf("</n>");
#else
    printf("</div>");
#endif
  }
}

//----------------------------------------------------------------------

SymbolNode* SymbolNode::freeList;

void* SymbolNode::operator new(size_t size)
{
  if (!freeList) {
    SymbolNode* newNodes = (SymbolNode*) new char[sizeof(SymbolNode) * 5000];
    if (!newNodes) {
      return NULL;
    }
    SymbolNode* n = newNodes;
    SymbolNode* end = newNodes + 5000 - 1;
    while (n < end) {
      n->next = n + 1;
      n++;
    }
    n->next = NULL;
    freeList = newNodes;
  }

  SymbolNode* rv = freeList;
  freeList = rv->next;

  return (void*) rv;
}

void SymbolNode::operator delete(void* ptr)
{
  SymbolNode* node = (SymbolNode*) ptr;
  if (node) {
    node->next = freeList;
    freeList = node;
  }
}

//----------------------------------------------------------------------

void Symbol::Init(const char* aName, u_long aAddress)
{
  name = aName ? strdup(aName) : "";
  address = aAddress;
  dumped = false;
  leaker = false;
  calls = 0;
  parents = NULL;
  children = NULL;
  bytesDirectlyLeaked = 0;
  childBytesLeaked = 0;
}

void Symbol::AddParent(Symbol* aParent)
{
  SymbolNode* node = new SymbolNode(aParent);
  if (node) {
    node->next = parents;
    parents = node;
  }
}

void Symbol::AddChild(Symbol* aChild)
{
  SymbolNode* node = new SymbolNode(aChild);
  if (node) {
    node->next = children;
    children = node;
  }
}

