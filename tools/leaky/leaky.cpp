/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Kipp E.B. Hickman.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "leaky.h"
#include "dict.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#ifndef NTO
#include <getopt.h>
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef NTO
#include <mem.h>
#endif

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

  dumpLeaks = FALSE;
  dumpGraph = FALSE;
  dumpHTML = FALSE;
  quiet = FALSE;
  dumpEntireLog = FALSE;
  showAddress = FALSE;
  stackDepth = 100000;
  dumpRefcnts = false;

  mappedLogFile = -1;
  firstLogEntry = lastLogEntry = 0;
  buckets = DefaultBuckets;
  dict = NULL;
  refcntDict = NULL;

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
	  "Usage: %s [-aAEdfgqxR] [-e name] [-s depth] [-h hash-buckets] [-r root|-i symbol] prog log\n",
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
  while ((arg = getopt(argc, argv, "adEe:gh:i:r:Rs:tqx")) != -1) {
    switch (arg) {
      case '?':
	errflg++;
	break;
      case 'a':
	dumpEntireLog = TRUE;
	break;
      case 'A':
	showAddress = TRUE;
	break;
      case 'd':
	dumpLeaks = TRUE;
	if (dumpGraph) errflg++;
	break;
      case 'R':
	dumpRefcnts = true;
	break;
      case 'e':
	exclusions.add(optarg);
	break;
      case 'g':
	dumpGraph = TRUE;
	if (dumpLeaks) errflg++;
	break;
      case 'r':
	roots.add(optarg);
	if (!includes.IsEmpty()) {
	  errflg++;
	}
	break;
      case 'i':
	includes.add(optarg);
	if (!roots.IsEmpty()) {
	  errflg++;
	}
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
	dumpHTML = TRUE;
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
  if (dumpRefcnts) {
    refcntDict = new MallocDict(buckets);
  }
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
  mappedLogFile = ::open(logFile, O_RDONLY);
  if (mappedLogFile < 0) {
    perror("open");
    exit(-1);
  }
  off_t size;
  firstLogEntry = (malloc_log_entry*) mapFile(mappedLogFile, PROT_READ, &size);
  lastLogEntry = (malloc_log_entry*)((char*)firstLogEntry + size);

  analyze();

  if (dumpLeaks || dumpEntireLog || dumpRefcnts) {
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

bool leaky::excluded(malloc_log_entry* lep)
{
  if (exclusions.IsEmpty()) {
    return false;
  }

  char** pcp = &lep->pcs[0];
  u_int n = lep->numpcs;
  for (u_int i = 0; i < n; i++, pcp++) {
    Symbol* sp = findSymbol((u_long) *pcp);
    if (sp && exclusions.contains(sp->name)) {
      return true;
    }
  }
  return false;
}

bool leaky::included(malloc_log_entry* lep)
{
  if (includes.IsEmpty()) {
    return true;
  }

  char** pcp = &lep->pcs[0];
  u_int n = lep->numpcs;
  for (u_int i = 0; i < n; i++, pcp++) {
    Symbol* sp = findSymbol((u_long) *pcp);
    if (sp && includes.contains(sp->name)) {
      return true;
    }
  }
  return false;
}

//----------------------------------------------------------------------

void leaky::displayStackTrace(FILE* out, malloc_log_entry* lep)
{
  char** pcp = &lep->pcs[0];
  u_int n = (lep->numpcs < stackDepth) ? lep->numpcs : stackDepth;
  for (u_int i = 0; i < n; i++, pcp++) {
    u_long addr = (u_long) *pcp;
    Symbol* sp = findSymbol(addr);
    if (sp) {
      fputs(sp->name, out);
      if (showAddress) {
	fprintf(out, "[%p]", (char*)addr);
      }
    }
    else {
      fprintf(out, "<%p>", (char*)addr);
    }
    fputc(' ', out);
  }
  fputc('\n', out);
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
  printf("%-10s %08lx %5ld ",
	 typeFromLog[lep->type],
	 lep->address, lep->size);
  if (IsRefcnt(lep)) {
    printf("%08ld", lep->oldaddress);
  }
  else {
    printf("%08lx", lep->oldaddress);
  }
  printf(" --> ");
  displayStackTrace(stdout, lep);
}

bool leaky::ShowThisEntry(malloc_log_entry* lep)
{
  if ((!dumpRefcnts || IsRefcnt(lep)) && !excluded(lep) && included(lep)) {
    return true;
  }
  return false;
}

void leaky::dumpLog()
{
  if (dumpRefcnts) {
      malloc_log_entry* lep;
      refcntDict->rewind();
      while (NULL != (lep = refcntDict->next())) {
	if (ShowThisEntry(lep)) {
	  // Now we get slow...
	  u_long addr = lep->address;
	  malloc_log_entry* lep2 = firstLogEntry;
	  while (lep2 < lastLogEntry) {
	    if (lep2->address == addr) {
	      dumpEntryToLog(lep2);
	    }
	    lep2 = (malloc_log_entry*) &lep2->pcs[lep2->numpcs];
	  }
	}
      }
  }
  else {
    if (dumpEntireLog) {
      malloc_log_entry* lep = firstLogEntry;
      while (lep < lastLogEntry) {
	if (ShowThisEntry(lep)) {
	  dumpEntryToLog(lep);
	}
	lep = (malloc_log_entry*) &lep->pcs[lep->numpcs];
      }
    } else {
      malloc_log_entry* lep;
      dict->rewind();
      while (NULL != (lep = dict->next())) {
	if (ShowThisEntry(lep)) {
	  dumpEntryToLog(lep);
	}
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
      displayStackTrace(stdout, lep);
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
      displayStackTrace(stdout, lep);
    }
    errors++;
  } else {
    dict->remove(address);
  }
}

void leaky::analyze()
{
  malloc_log_entry* lep = firstLogEntry;
  while (lep < lastLogEntry) {
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
      case malloc_log_addref:
	if (dumpRefcnts) {
	  if (lep->size == 0) {
	    // Initial addref
	    u_long addr = (u_long) lep->address;
	    malloc_log_entry** lepp = refcntDict->find(addr);
	    if (!lepp) {
	      refcntDict->add(addr, lep);
	    }
	  }
	}
	break;
      case malloc_log_release:
	if (dumpRefcnts) {
	  if (lep->oldaddress == 0) {
	    // Final release
	    u_long addr = (u_long) lep->address;
	    malloc_log_entry** lepp = refcntDict->find(addr);
	    if (lepp) {
	      refcntDict->remove(addr);
	    }
	  }
	}
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
    if (ShowThisEntry(lep)) {
      char** basepcp = &lep->pcs[0];
      char** pcp = &lep->pcs[lep->numpcs - 1];

      // Find root for this allocation
      Symbol* sym = findSymbol((u_long) *pcp);
      TreeNode* node = sym->root;
      if (!node) {
	sym->root = node = new TreeNode(sym);

	// Add root to list of roots
	if (roots.IsEmpty()) {
	  node->nextRoot = rootList;
	  rootList = node;
	}
      }
      pcp--;

      // Build tree underneath the root
      for (; pcp >= basepcp; pcp--) {
	// Share nodes in the tree until there is a divergence
	sym = findSymbol((u_long) *pcp);
	if (!sym) {
	  break;
	}
	TreeNode* nextNode = node->GetDirectDescendant(sym);
	if (!nextNode) {
	  // Make a new node at the point of divergence
	  nextNode = node->AddDescendant(sym);
	}

	// See if the symbol is to be a user specified root. If it is,
	// and we haven't already stuck it on the root-list do so now.
	if (!sym->root && !roots.IsEmpty() && roots.contains(sym->name)) {
	  sym->root = nextNode;
	  nextNode->nextRoot = rootList;
	  rootList = nextNode;
	}

	if (pcp == basepcp) {
	  nextNode->bytesLeaked += lep->size;
	}
	else {
	  node->descendantBytesLeaked += lep->size;
	}

	node = nextNode;
      }
    }
  }
}

Symbol* leaky::findLeakGraphRoot(Symbol* aStart, Symbol* aEnd)
{
  while (aStart < aEnd) {
    if (aStart->root) {
      return aStart;
    }
    aStart++;
  }
  return NULL;
}

void leaky::dumpLeakGraph()
{ 
  if (dumpHTML) {
    printf("<html><head><title>Leaky Graph</title>\n");
    printf("<style src=\"resource:/res/leaky/leaky.css\"></style>\n");
    printf("<script src=\"resource:/res/leaky/leaky.js\"></script>\n");
    printf("</head><body><div class=\"key\">\n");
    printf("Key:<br>\n");
    printf("<span class=b>Bytes directly leaked</span><br>\n");
    printf("<span class=d>Bytes leaked by descendants</span></div>\n");
  }

#if 0
  Symbol* base = externalSymbols;
  Symbol* end = externalSymbols + usefulSymbols;
  while (base < end) {
    Symbol* sym = findLeakGraphRoot(base, end);
    if (!sym) break;
    dumpLeakTree(sym->root, 0);
    base = sym + 1;
  }
#else
  TreeNode* root = rootList;
  while (root) {
    dumpLeakTree(root, 0);
    root = root->nextRoot;
  }
#endif
  if (dumpHTML) {
    printf("</body></html>\n");
  }
}

void leaky::dumpLeakTree(TreeNode* aNode, int aIndent)
{
  Symbol* sym = aNode->symbol;
  if (dumpHTML) {
    printf("<div class=\"n\">\n");
    if (aNode->HasDescendants()) {
      printf("<img onmouseout=\"O(event);\" onmouseover=\"I(event);\" ");
      printf("onclick=\"C(event);\" src=\"resource:/res/leaky/%s.gif\">",
	     aIndent > 1 ? "close" : "open");
    }
    printf("<span class=s>%s</span><span class=b>%ld</span>",
	   sym->name,
	   aNode->bytesLeaked);
    printf("<span class=d>%ld</span>\n",
	   aNode->descendantBytesLeaked);
  }
  else {
    indentBy(aIndent);
    printf("%s bytesLeaked=%ld (%ld from kids)\n", 
	   sym->name,
	   aNode->bytesLeaked,
	   aNode->descendantBytesLeaked);
  }

  TreeNode* node = aNode->descendants;
  int kidNum = 0;
  while (node) {
    sym = node->symbol;
    dumpLeakTree(node, aIndent + 1);
    kidNum++;
    node = node->nextSibling;
  }

  if (dumpHTML) {
    printf("</div>");
  }
}

//----------------------------------------------------------------------

TreeNode* TreeNode::freeList;

void* TreeNode::operator new(size_t size) CPP_THROW_NEW
{
  if (!freeList) {
    TreeNode* newNodes = (TreeNode*) new char[sizeof(TreeNode) * 5000];
    if (!newNodes) {
      return NULL;
    }
    TreeNode* n = newNodes;
    TreeNode* end = newNodes + 5000 - 1;
    while (n < end) {
      n->nextSibling = n + 1;
      n++;
    }
    n->nextSibling = NULL;
    freeList = newNodes;
  }

  TreeNode* rv = freeList;
  freeList = rv->nextSibling;

  return (void*) rv;
}

void TreeNode::operator delete(void* ptr)
{
  TreeNode* node = (TreeNode*) ptr;
  if (node) {
    node->nextSibling = freeList;
    freeList = node;
  }
}

TreeNode* TreeNode::GetDirectDescendant(Symbol* aSymbol)
{
  TreeNode* node = descendants;
  while (node) {
    if (node->symbol == aSymbol) {
      return node;
    }
    node = node->nextSibling;
  }
  return NULL;
}

TreeNode* TreeNode::AddDescendant(Symbol* aSymbol)
{
  TreeNode* node = new TreeNode(aSymbol);
  node->nextSibling = descendants;
  descendants = node;
  return node;
}
