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

#include "leaky.h"
#include "intcnt.h"

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

  quiet = TRUE;
  showAddress = FALSE;
  stackDepth = 100000;

  mappedLogFile = -1;
  firstLogEntry = lastLogEntry = 0;

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
}

void leaky::usageError()
{
  fprintf(stderr, "Usage: %s prog log\n", (char*) applicationName);
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
	break;
      case 'A':
	showAddress = TRUE;
	break;
      case 'd':
	break;
      case 'R':
	break;
      case 'e':
	exclusions.add(optarg);
	break;
      case 'g':
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
	break;
      case 's':
	stackDepth = atoi(optarg);
	if (stackDepth < 2) {
	  stackDepth = 2;
	}
	break;
      case 'x':
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

  int fd = ::open(M_MAPFILE, O_RDONLY);
  if (fd < 0) {
    perror("open: " M_MAPFILE);
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
int leaky::findSymbolIndex(u_long addr)
{
  u_int base = 0;
  u_int limit = usefulSymbols - 1;
  Symbol* end = &externalSymbols[limit];
  while (base <= limit) {
    u_int midPoint = (base + limit)>>1;
    Symbol* sp = &externalSymbols[midPoint];
    if (addr < sp->address) {
      if (midPoint == 0) {
	return -1;
      }
      limit = midPoint - 1;
    } else {
      if (sp+1 < end) {
	if (addr < (sp+1)->address) {
	  return midPoint;
	}
      } else {
	return midPoint;
      }
      base = midPoint + 1;
    }
  }
  return -1;
}

Symbol* leaky::findSymbol(u_long addr)
{
  int idx = findSymbolIndex(addr);

  if(idx<0) {
    return NULL;
  } else {
    return &externalSymbols[idx];
  }
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

void leaky::dumpEntryToLog(malloc_log_entry* lep)
{
  printf("%ld\t", lep->delTime);
  printf(" --> ");
  displayStackTrace(stdout, lep);
}

void leaky::generateReportHTML(FILE *fp, int *countArray, int count)
{
  fprintf(fp,"<html><head><title>Jprof Profile Report</title></head><body>\n");
  fprintf(fp,"<h1><center>Jprof Profile Report</center></h1>\n");
  fprintf(fp,"<center>");
  fprintf(fp,"<A href=#flat>flat</A><b> | </b><A href=#hier>hierarchical</A>");
  fprintf(fp,"</center><P><P><P>\n");

  int *rankingTable = new int[usefulSymbols];

  for(int cnt=usefulSymbols; --cnt>=0; rankingTable[cnt]=cnt);

  // Drat.  I would use ::qsort() but I would need a global variable and my
  // into-pascal professor threatened to flunk anyone who used globals.
  // She dammaged me for life :-) (That was 1986.  See how much influence
  // she had.  I dont remember her name but I always feel guilty about globals)

  // Shell Sort. 581130733 is the max 31 bit value of h = 3h+1
  int mx, i, h;
  for(mx=usefulSymbols/9, h=581130733; h>0; h/=3) {
    if(h<mx) {
      for(i = h-1; i<usefulSymbols; i++) {
        int j, tmp=rankingTable[i], val = countArray[tmp];
	for(j = i; (j>=h) && (countArray[rankingTable[j-h]]<val); j-=h) {
	  rankingTable[j] = rankingTable[j-h];
	}
	rankingTable[j] = tmp;
      }
    }
  }

  // Ok, We are sorted now.  Lets go through the table until we get to
  // functions that were never called.  Right now we dont do much inside
  // this loop.  Later we can get callers and callees into it like gprof
  // does
  fprintf(fp,
  "<h2><A NAME=hier></A><center><a href=\"http://lxr.mozilla.org/mozilla/source/tools/jprof/README.html#hier\">Hierarchical Profile</a></center></h2><hr>\n");
  fprintf(fp, "<pre>\n");
  fprintf(fp, "%5s %5s    %4s %s\n",
  "index", "Count", "Hits", "Function Name");

  for(i=0; i<usefulSymbols && countArray[rankingTable[i]]>0; i++) {
    Symbol *sp=&externalSymbols[rankingTable[i]];
    
    sp->cntP.printReport(fp, this);

    fprintf(fp, "%6d %3d <a name=%d>%8d</a> <b>%s</b>\n", rankingTable[i],
    sp->timerHit, rankingTable[i], countArray[rankingTable[i]], sp->name);

    sp->cntC.printReport(fp, this);

    fprintf(fp, "<hr>\n");
  }
  fprintf(fp,"</pre>\n");

  // OK, Now we want to print the flat profile.  To do this we resort on
  // the hit count.

  // Cut-N-Paste Shell sort from above.  The Ranking Table has already been
  // populated, so we do not have to reinitialize it.
  for(mx=usefulSymbols/9, h=581130733; h>0; h/=3) {
    if(h<mx) {
      for(i = h-1; i<usefulSymbols; i++) {
	int j, tmp=rankingTable[i], val = externalSymbols[tmp].timerHit;
	for(j = i;
	  (j>=h) && (externalSymbols[rankingTable[j-h]].timerHit<val); j-=h) {
	  rankingTable[j] = rankingTable[j-h];
	}
	rankingTable[j] = tmp;
      }
    }
  }

  // Pre-count up total counter hits, to get a percentage.
  // I wanted the total before walking the list, if this
  // double-pass over externalSymbols gets slow we can
  // do single-pass and print this out after the loop finishes.
  int totalTimerHits = 0;
  for(i=0;
    i<usefulSymbols && externalSymbols[rankingTable[i]].timerHit>0; i++) {
    Symbol *sp=&externalSymbols[rankingTable[i]];
    totalTimerHits += sp->timerHit;
  }

  fprintf(fp,"<h2><A NAME=flat></A><center><a href=\"http://lxr.mozilla.org/mozilla/source/tools/jprof/README.html#flat\">Flat Profile</a></center></h2><br>\n");
  fprintf(fp, "<pre>\n");

  fprintf(fp, "Total hit count: %d\n", totalTimerHits);
  fprintf(fp, "Count %%Total  Function Name\n");
  // Now loop for as long as we have timer hits
  for(i=0;
    i<usefulSymbols && externalSymbols[rankingTable[i]].timerHit>0; i++) {

    Symbol *sp=&externalSymbols[rankingTable[i]];
    
    fprintf(fp, "<a href=\"#%d\">%3d   %-2.1f     %s</a>\n",
    rankingTable[i], sp->timerHit,
    ((float)sp->timerHit/(float)totalTimerHits)*100.0, sp->name);
  }

  fprintf(fp,"</pre></body></html>\n");
}

void leaky::analyze()
{
  int *countArray = new int[usefulSymbols];
  int *flagArray  = new int[usefulSymbols];

  //Zero our function call counter
  memset(countArray, 0, sizeof(countArray[0])*usefulSymbols);

  // The flag array is used to prevent counting symbols multiple times
  // if functions are called recursively.  In order to keep from having
  // to zero it on each pass through the loop, we mark it with the value
  // of stacks on each trip through the loop.  This means we can determine
  // if we have seen this symbol for this stack trace w/o having to reset
  // from the prior stacktrace.
  memset(flagArray, -1, sizeof(flagArray[0])*usefulSymbols);

  // This loop walks through all the call stacks we recorded
  stacks = 0;
  for(malloc_log_entry* lep=firstLogEntry; 
    lep < lastLogEntry;
    lep = reinterpret_cast<malloc_log_entry*>(&lep->pcs[lep->numpcs])) {

    if (excluded(lep) || !included(lep))
      continue;

    ++stacks; // How many stack frames did we collect

    // This loop walks through every symbol in the call stack.  By walking it
    // backwards we know who called the function when we get there.
    u_int n = (lep->numpcs < stackDepth) ? lep->numpcs : stackDepth;
    char** pcp = &lep->pcs[n-1];
    int idx=-1, parrentIdx=-1;  // Init idx incase n==0
    for(int i=n-1; i>=0; --i, --pcp, parrentIdx=idx) {
      idx = findSymbolIndex(reinterpret_cast<u_long>(*pcp));

      if(idx>=0) {
	// Skip over bogus __restore_rt frames that realtime profiling
	// can introduce.
	if (i > 0 && !strcmp(externalSymbols[idx].name, "__restore_rt")) {
	  --pcp;
	  --i;
	  idx = findSymbolIndex(reinterpret_cast<u_long>(*pcp));
	  if (idx < 0) {
	    continue;
	  }
	}
	
	// If we have not seen this symbol before count it and mark it as seen
	if(flagArray[idx]!=stacks && ((flagArray[idx]=stacks) || true)) {
	  ++countArray[idx];
	}

	// We know who we are and we know who our parrent is.  Count this
	if(parrentIdx>=0) {
	  externalSymbols[parrentIdx].regChild(idx);
	  externalSymbols[idx].regParrent(parrentIdx);
	}
      }
    }

    // idx should be the function that we were in when we recieved the signal.
    if(idx>=0) {
      ++externalSymbols[idx].timerHit;
    }
  }

  generateReportHTML(stdout, countArray, stacks);
}

void FunctionCount::printReport(FILE *fp, leaky *lk)
{
    const char *fmt = "             <A href=\"#%d\">%6d %s</A>\n";

    int nmax, tmax=((~0U)>>1);
    
    do {
	nmax=0;
	for(int j=getSize(); --j>=0;) {
	    int cnt = getCount(j);
	    if(cnt==tmax) {
		int idx = getIndex(j);
		fprintf(fp, fmt, idx, getCount(j),
		const_cast<char*>(lk->indexToName(idx)));
	    } else if(cnt<tmax && cnt>nmax) {
	        nmax=cnt;
	    }
	}
    } while((tmax=nmax)>0);
}
