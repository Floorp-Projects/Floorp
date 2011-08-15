/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Randell Jesup (recent improvements, threads, etc)
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
  l->outputfd = stdout;

  for (int i = 0; i < l->numLogFiles; i++) {
    if (l->output_dir || l->numLogFiles > 1) {
      char name[2048]; // XXX fix
      if (l->output_dir)
        snprintf(name,sizeof(name),"%s/%s.html",l->output_dir,argv[l->logFileIndex + i]);
      else
        snprintf(name,sizeof(name),"%s.html",argv[l->logFileIndex + i]);

      fprintf(stderr,"opening %s\n",name);
      l->outputfd = fopen(name,"w");
      // if an error we won't process the file
    }
    if (l->outputfd) { // paranoia
      l->open(argv[l->logFileIndex + i]);

      if (l->outputfd != stderr) {
        fclose(l->outputfd);
        l->outputfd = NULL;
      }
    }
  }

  return 0;
}

char *
htmlify(const char *in)
{
  const char *p = in;
  char *out, *q;
  int n = 0;
  size_t newlen;

  // Count the number of '<' and '>' in the input.
  while ((p = strpbrk(p, "<>")))
  {
    ++n;
    ++p;
  }

  // Knowing the number of '<' and '>', we can calculate the space
  // needed for the output string.
  newlen = strlen(in) + n * 3 + 1;
  out = new char[newlen];

  // Copy the input to the output, with substitutions.
  p = in;
  q = out;
  do
  {
    if (*p == '<') 
    {
      strcpy(q, "&lt;");
      q += 4;
    }
    else if (*p == '>')
    {
      strcpy(q, "&gt;");
      q += 4;
    }
    else
    {
      *q++ = *p;
    }
    p++;
  } while (*p);
  *q = '\0';

  return out;
}

leaky::leaky()
{
  applicationName = NULL;
  progFile = NULL;

  quiet = true;
  showAddress = false;
  showThreads = false;
  stackDepth = 100000;
  onlyThread = 0;

  mappedLogFile = -1;
  firstLogEntry = lastLogEntry = 0;

  sfd = -1;
  externalSymbols = 0;
  usefulSymbols = 0;
  numExternalSymbols = 0;
  lowestSymbolAddr = 0;
  highestSymbolAddr = 0;

  loadMap = NULL;

  collect_last  = false;
  collect_start = -1;
  collect_end   = -1;
}

leaky::~leaky()
{
}

void leaky::usageError()
{
  fprintf(stderr, "Usage: %s [-v] [-t] [-e exclude] [-i include] [-s stackdepth] [--last] [--all] [--start n [--end m]] [--output-dir dir] prog log [log2 ...]\n", (char*) applicationName);
  fprintf(stderr, 
          "\t-v: verbose\n"
          "\t-t | --threads: split threads\n"
          "\t--only-thread n: only profile thread N\n"
          "\t-i include-id: stack must include specified id\n"
          "\t-e exclude-id: stack must NOT include specified id\n"
          "\t-s stackdepth: Limit depth looked at from captured stack frames\n"
          "\t--last: only profile the last capture section\n"
          "\t--start n [--end m]: profile n to m (or end) capture sections\n"
          "\t--output-dir dir: write output files to dir\n"
          "\tIf there's one log, output goes to stdout unless --output-dir is set\n"
          "\tIf there are more than one log, output files will be named with .html added\n"
          );
  exit(-1);
}

static struct option longopts[] = {
    { "threads", 0, NULL, 't' },
    { "only-thread", 1, NULL, 'T' },
    { "last", 0, NULL, 'l' },
    { "start", 1, NULL, 'x' },
    { "end", 1, NULL, 'n' },
    { "output-dir", 1, NULL, 'd' },
    { NULL, 0, NULL, 0 },
};

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
  int longindex = 0;

  onlyThread = 0;
  output_dir = NULL;

  // XXX tons of cruft here left over from tracemalloc
  // XXX The -- options shouldn't need short versions, or they should be documented
  while (((arg = getopt_long(argc, argv, "adEe:gh:i:r:Rs:tT:qvx:ln:",longopts,&longindex)) != -1)) {
    switch (arg) {
      case '?':
      default:
        fprintf(stderr,"error: unknown option %c\n",optopt);
	errflg++;
	break;
      case 'a':
	break;
      case 'A': // not implemented
	showAddress = true;
	break;
      case 'd':
        output_dir = optarg; // reference to an argv pointer
	break;
      case 'R':
	break;
      case 'e':
	exclusions.add(optarg);
	break;
      case 'g':
	break;
      case 'r': // not implemented
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
        // --start
        collect_start = atoi(optarg);
	break;
      case 'n':
        // --end
        collect_end = atoi(optarg);
        break;
      case 'l':
        // --last
        collect_last = true;
        break;
      case 'q':
        break;
      case 'v':
        quiet = !quiet;
        break;
      case 't':
        showThreads = true;
	break;
      case 'T':
        showThreads = true;
        onlyThread = atoi(optarg);
	break;
    }
  }
  if (errflg || ((argc - optind) < 2)) {
    usageError();
  }
  progFile = argv[optind++];
  logFileIndex = optind;
  numLogFiles  = argc - optind;
  if (!quiet)
    fprintf(stderr,"numlogfiles = %d\n",numLogFiles);
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

  if (!loadMap) {
    // all files use the same map
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
        fprintf(stderr,"%s @ %lx\n", name, mme.address);
      }

      LoadMapEntry* lme = new LoadMapEntry;
      lme->address = mme.address;
      lme->name = strdup(name);
      lme->next = loadMap;
      loadMap = lme;
    }
    close(fd);
  }
}

void leaky::open(char *logFile)
{
  int threadArray[100]; // should auto-expand
  int last_thread = -1;
  int numThreads = 0;
  int section = -1;
  bool collecting = false;

  LoadMap();

  setupSymbols(progFile);

  // open up the log file
  if (mappedLogFile)
    ::close(mappedLogFile);

  mappedLogFile = ::open(logFile, O_RDONLY);
  if (mappedLogFile < 0) {
    perror("open");
    exit(-1);
  }
  off_t size;
  firstLogEntry = (malloc_log_entry*) mapFile(mappedLogFile, PROT_READ, &size);
  lastLogEntry = (malloc_log_entry*)((char*)firstLogEntry + size);

  if (!collect_last || collect_start < 0) {
    collecting = true;
  }

  // First, restrict it to the capture sections specified (all, last, start/end)
  // This loop walks through all the call stacks we recorded
  for (malloc_log_entry* lep=firstLogEntry;
       lep < lastLogEntry;
       lep = reinterpret_cast<malloc_log_entry*>(&lep->pcs[lep->numpcs])) {

    if (lep->flags & JP_FIRST_AFTER_PAUSE) {
      section++;
      if (collect_last) {
        firstLogEntry = lep;
        numThreads = 0;
        collecting = true;
      }
      if (collect_start == section) {
        collecting = true;
        firstLogEntry = lep;
      }
      if (collect_end == section) {
        collecting = false;
        lastLogEntry = lep;
      }
      if (!quiet)
        fprintf(stderr,"New section %d: first=%p, last=%p, collecting=%d\n",
                section,(void*)firstLogEntry,(void*)lastLogEntry,collecting);
    }

    // Capture thread info at the same time

    // Find all the threads captured

    // pthread/linux docs say the signal can be delivered to any thread in
    // the process.  In practice, it appears in Linux that it's always
    // delivered to the thread that called setitimer(), and each thread can
    // have a separate itimer.  There's a support library for gprof that
    // overlays pthread_create() to set timers in any threads you spawn.
    if (showThreads && collecting) {
      if (lep->thread != last_thread)
      {
        int i;
        for (i=0; i<numThreads; i++)
        {
          if (lep->thread == threadArray[i])
            break;
        }
        if (i == numThreads &&
            i < (int) (sizeof(threadArray)/sizeof(threadArray[0])))
        {
          threadArray[i] = lep->thread;
          numThreads++;
          if (!quiet)
            fprintf(stderr,"new thread %d\n",lep->thread);
        }
      }
    }
  }  
  if (!quiet)
    fprintf(stderr,"Done collecting: sections %d: first=%p, last=%p, numThreads=%d\n",
            section,(void*)firstLogEntry,(void*)lastLogEntry,numThreads);

  fprintf(outputfd,"<html><head><title>Jprof Profile Report</title></head><body>\n");
  fprintf(outputfd,"<h1><center>Jprof Profile Report</center></h1>\n");

  if (showThreads)
  {
    fprintf(stderr,"Num threads %d\n",numThreads);

    fprintf(outputfd,"<hr>Threads:<p><pre>\n");
    for (int i=0; i<numThreads; i++)
    {
      fprintf(outputfd,"   <a href=\"#thread_%d\">%d</a>  ",
              threadArray[i],threadArray[i]);
    }
    fprintf(outputfd,"</pre>");

    for (int i=0; i<numThreads; i++)
    {
      if (!onlyThread || onlyThread == threadArray[i])
        analyze(threadArray[i]);
    }
  }
  else
  {
    analyze(0);
  }

  fprintf(outputfd,"</pre></body></html>\n");
}

//----------------------------------------------------------------------


static int symbolOrder(void const* a, void const* b)
{
  Symbol const* ap = (Symbol const *)a;
  Symbol const* bp = (Symbol const *)b;
  return ap->address == bp->address ? 0 :
    (ap->address > bp->address ? 1 : -1);
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
  if (usefulSymbols == 0) {
    // only read once!

    // Read in symbols from the program
    ReadSymbols(fileName, 0);

    // Read in symbols from the .so's
    ReadSharedLibrarySymbols();

    if (!quiet) {
      fprintf(stderr,"A total of %d symbols were loaded\n", usefulSymbols);
    }

    // Now sort them
    qsort(externalSymbols, usefulSymbols, sizeof(Symbol), symbolOrder);
    lowestSymbolAddr = externalSymbols[0].address;
    highestSymbolAddr = externalSymbols[usefulSymbols-1].address;
  }
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
  displayStackTrace(outputfd, lep);
}

void leaky::generateReportHTML(FILE *fp, int *countArray, int count, int thread)
{
  fprintf(fp,"<center>");
  if (showThreads)
  {
    fprintf(fp,"<hr><A NAME=thread_%d><b>Thread: %d</b></A><p>",
            thread,thread);
  }
  fprintf(fp,"<A href=#flat_%d>flat</A><b> | </b><A href=#hier_%d>hierarchical</A>",
          thread,thread);
  fprintf(fp,"</center><P><P><P>\n");

  int totalTimerHits = count;
  int *rankingTable = new int[usefulSymbols];

  for(int cnt=usefulSymbols; --cnt>=0; rankingTable[cnt]=cnt);

  // Drat.  I would use ::qsort() but I would need a global variable and my
  // intro-pascal professor threatened to flunk anyone who used globals.
  // She damaged me for life :-) (That was 1986.  See how much influence
  // she had.  I don't remember her name but I always feel guilty about globals)

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

  // Ok, We are sorted now.  Let's go through the table until we get to
  // functions that were never called.  Right now we don't do much inside
  // this loop.  Later we can get callers and callees into it like gprof
  // does
  fprintf(fp,
	  "<h2><A NAME=hier_%d></A><center><a href=\"http://lxr.mozilla.org/mozilla/source/tools/jprof/README.html#hier\">Hierarchical Profile</a></center></h2><hr>\n",
          thread);
  fprintf(fp, "<pre>\n");
  fprintf(fp, "%6s %6s         %4s      %s\n",
          "index", "Count", "Hits", "Function Name");

  for(i=0; i<usefulSymbols && countArray[rankingTable[i]]>0; i++) {
    Symbol *sp=&externalSymbols[rankingTable[i]];
    
    sp->cntP.printReport(fp, this, rankingTable[i], totalTimerHits);

    char *symname = htmlify(sp->name);
    fprintf(fp, "%6d %6d (%3.1f%%)%s <a name=%d>%8d (%3.1f%%)</a>%s <b>%s</b>\n", 
            rankingTable[i],
            sp->timerHit, (sp->timerHit*1000/totalTimerHits)/10.0,
            (sp->timerHit*1000/totalTimerHits)/10.0 >= 10.0 ? "" : " ",
            rankingTable[i], countArray[rankingTable[i]],
            (countArray[rankingTable[i]]*1000/totalTimerHits)/10.0,
            (countArray[rankingTable[i]]*1000/totalTimerHits)/10.0 >= 10.0 ? "" : " ",
            symname);
    delete [] symname;

    sp->cntC.printReport(fp, this, rankingTable[i], totalTimerHits);

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
  totalTimerHits = 0;
  for(i=0;
    i<usefulSymbols && externalSymbols[rankingTable[i]].timerHit>0; i++) {
    Symbol *sp=&externalSymbols[rankingTable[i]];
    totalTimerHits += sp->timerHit;
  }
  if (totalTimerHits == 0)
    totalTimerHits = 1;

  if (totalTimerHits != count)
    fprintf(stderr,"Hit count mismatch: count=%d; totalTimerHits=%d",
            count,totalTimerHits);

  fprintf(fp,"<h2><A NAME=flat_%d></A><center><a href=\"http://lxr.mozilla.org/mozilla/source/tools/jprof/README.html#flat\">Flat Profile</a></center></h2><br>\n",
          thread);
  fprintf(fp, "<pre>\n");

  fprintf(fp, "Total hit count: %d\n", totalTimerHits);
  fprintf(fp, "Count %%Total  Function Name\n");
  // Now loop for as long as we have timer hits
  for(i=0;
    i<usefulSymbols && externalSymbols[rankingTable[i]].timerHit>0; i++) {

    Symbol *sp=&externalSymbols[rankingTable[i]];
    
    char *symname = htmlify(sp->name);
    fprintf(fp, "<a href=\"#%d\">%3d   %-2.1f     %s</a>\n",
            rankingTable[i], sp->timerHit,
            ((float)sp->timerHit/(float)totalTimerHits)*100.0, symname);
    delete [] symname;
  }
}

void leaky::analyze(int thread)
{
  int *countArray = new int[usefulSymbols];
  int *flagArray  = new int[usefulSymbols];

  //Zero our function call counter
  memset(countArray, 0, sizeof(countArray[0])*usefulSymbols);

  // reset hit counts
  for(int i=0; i<usefulSymbols; i++) {
    externalSymbols[i].timerHit = 0;
    externalSymbols[i].regClear();
  }

  // The flag array is used to prevent counting symbols multiple times
  // if functions are called recursively.  In order to keep from having
  // to zero it on each pass through the loop, we mark it with the value
  // of stacks on each trip through the loop.  This means we can determine
  // if we have seen this symbol for this stack trace w/o having to reset
  // from the prior stacktrace.
  memset(flagArray, -1, sizeof(flagArray[0])*usefulSymbols);

  // This loop walks through all the call stacks we recorded
  // --last, --start and --end can restrict it, as can excludes/includes
  stacks = 0;
  for(malloc_log_entry* lep=firstLogEntry; 
    lep < lastLogEntry;
    lep = reinterpret_cast<malloc_log_entry*>(&lep->pcs[lep->numpcs])) {

    if ((thread != 0 && lep->thread != thread) ||
        excluded(lep) || !included(lep))
    {
      continue;
    }

    ++stacks; // How many stack frames did we collect

    // This loop walks through every symbol in the call stack.  By walking it
    // backwards we know who called the function when we get there.
    u_int n = (lep->numpcs < stackDepth) ? lep->numpcs : stackDepth;
    char** pcp = &lep->pcs[n-1];
    int idx=-1, parrentIdx=-1;  // Init idx incase n==0
    for (int i=n-1; i>=0; --i, --pcp) {
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
        // inside if() so an unknown in the middle of a stack won't break
        // the link!
        parrentIdx=idx;
      }
    }

    // idx should be the function that we were in when we received the signal.
    if(idx>=0) {
      ++externalSymbols[idx].timerHit;
    }
  }

  generateReportHTML(outputfd, countArray, stacks, thread);
}

void FunctionCount::printReport(FILE *fp, leaky *lk, int parent, int total)
{
    const char *fmt = "                      <A href=\"#%d\">%8d (%3.1f%%)%s %s</A>%s\n";

    int nmax, tmax=((~0U)>>1);
    
    do {
	nmax=0;
	for(int j=getSize(); --j>=0;) {
	    int cnt = getCount(j);
	    if(cnt==tmax) {
		int idx = getIndex(j);
		char *symname = htmlify(lk->indexToName(idx));
                fprintf(fp, fmt, idx, getCount(j),
                        getCount(j)*100.0/total,
                        getCount(j)*100.0/total >= 10.0 ? "" : " ",
                        symname,
                        parent == idx ? " (self)" : "");
		delete [] symname;
	    } else if(cnt<tmax && cnt>nmax) {
	        nmax=cnt;
	    }
	}
    } while((tmax=nmax)>0);
}
