/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/VisualEventTracer.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "nscore.h"
#include "prthread.h"
#include "prprf.h"
#include "prio.h"
#include "prenv.h"
#include "plstr.h"
#include "nsThreadUtils.h"

namespace mozilla { namespace eventtracer {

#ifdef MOZ_VISUAL_EVENT_TRACER

namespace {

const PRUint32 kBatchSize = 0x1000;
const char kTypeChars[eventtracer::eLast] = {' ','N','S','W','E','D'};

// Flushing thread and records queue monitor
mozilla::Monitor * gMonitor = nullptr;

// Accessed concurently but since this flag is not functionally critical
// for optimization purposes is not accessed under a lock
bool gInitialized = false;

// Record of a single event
class Record {
public:
  Record() 
    : mType(::mozilla::eventtracer::eNone)
    , mTime(0)
    , mItem(nullptr)
    , mText(nullptr)
    , mText2(nullptr) 
  {
    MOZ_COUNT_CTOR(Record);
  } 
  ~Record() 
  {
    PL_strfree(mText); 
    PL_strfree(mText2);
    MOZ_COUNT_DTOR(Record);
  }

  PRUint32 mType;
  double mTime;
  void * mItem;
  char * mText;
  char * mText2;
};

char * DupCurrentThreadName()
{
  if (NS_IsMainThread())
    return PL_strdup("Main Thread");

  PRThread * currentThread = PR_GetCurrentThread();
  const char * name = PR_GetThreadName(currentThread);
  if (name)
    return PL_strdup(name);

  char buffer[128];
  PR_snprintf(buffer, 127, "Nameless %p", currentThread);

  return PL_strdup(buffer);
}

// An array of events, each thread keeps its own private instance
class RecordBatch {
public:
  RecordBatch()
    : mRecordsHead(new Record[kBatchSize])
    , mRecordsTail(mRecordsHead + kBatchSize)
    , mNextRecord(mRecordsHead)
    , mNextBatch(nullptr)
    , mThreadNameCopy(DupCurrentThreadName())
  {
    MOZ_COUNT_CTOR(RecordBatch);
  }

  ~RecordBatch()
  {
    delete [] mRecordsHead;
    PL_strfree(mThreadNameCopy);
    MOZ_COUNT_DTOR(RecordBatch);
  }

  static void FlushBatch(void * aData);

  Record * mRecordsHead;
  Record * mRecordsTail;
  Record * mNextRecord;

  RecordBatch * mNextBatch;
  char * mThreadNameCopy;
};

// Protected by gMonitor, accessed concurently
// Linked list of batches threads want to flush on disk
RecordBatch * gLogHead = nullptr;
RecordBatch * gLogTail = nullptr;

// Registered as thread private data destructor
void
RecordBatch::FlushBatch(void * aData)
{
  RecordBatch * threadLogPrivate = static_cast<RecordBatch *>(aData);

  MonitorAutoLock mon(*gMonitor);

  if (!gInitialized) {
    delete threadLogPrivate;
    return;
  }

  if (!gLogHead)
    gLogHead = threadLogPrivate;
  else // gLogTail is non-null
    gLogTail->mNextBatch = threadLogPrivate;
  gLogTail = threadLogPrivate;

  mon.Notify();  
}

// Helper class for filtering events by MOZ_PROFILING_EVENTS
class EventFilter
{
public:
  static EventFilter * Build(const char * filterVar);
  bool EventPasses(const char * eventName);

  ~EventFilter()
  {
    delete mNext;
    PL_strfree(mFilter);
    MOZ_COUNT_DTOR(EventFilter);
  }

private:
  EventFilter(const char * eventName, EventFilter * next)
    : mFilter(PL_strdup(eventName))
    , mNext(next)
  {
    MOZ_COUNT_CTOR(EventFilter);
  }

  char * mFilter;
  EventFilter * mNext;
};

// static
EventFilter *
EventFilter::Build(const char * filterVar)
{
  if (!filterVar || !*filterVar)
    return nullptr;

  // Reads a comma serpatated list of events.

  // Copied from nspr logging code (read of NSPR_LOG_MODULES)
  char eventName[64];
  PRIntn evlen = strlen(filterVar), pos = 0, count, delta = 0;

  // Read up to a comma or EOF -> get name of an event first in the list
  count = sscanf(filterVar, "%63[^,]%n", eventName, &delta);
  if (count == 0) 
    return nullptr;

  pos = delta;

  // Skip a comma, if present, accept spaces around it
  count = sscanf(filterVar + pos, " , %n", &delta);
  if (count != EOF)
    pos += delta;

  // eventName contains name of the first event in the list
  // second argument recursively parses the rest of the list string and
  // fills mNext of the just created EventFilter object chaining the objects
  return new EventFilter(eventName, Build(filterVar + pos));
}

bool
EventFilter::EventPasses(const char * eventName)
{
  if (!strcmp(eventName, mFilter))
    return true;

  if (mNext)
    return mNext->EventPasses(eventName);

  return false;
}

// State var to stop the flushing thread
bool gStopFlushingThread = false;

// State and control variables, initialized in Init() method, after it 
// immutable and read concurently.
EventFilter * gEventFilter = nullptr;
const char * gLogFilePath = nullptr;
PRThread * gFlushingThread = nullptr;
PRUintn gThreadPrivateIndex;
mozilla::TimeStamp gProfilerStart;

// To prevent any major I/O blockade caused by call to eventtracer::Mark() 
// we buffer the data produced by each thread and write it to disk
// in a separate low-priority thread.

// static
void FlushingThread(void * aArg)
{
  PRFileDesc * logFile = PR_Open(gLogFilePath, 
                         PR_WRONLY | PR_TRUNCATE | PR_CREATE_FILE,
                         0644);

  MonitorAutoLock mon(*gMonitor);

  if (!logFile) {
    gInitialized = false;
    return;
  }

  PRInt32 rv;
  bool ioError = false;

  const char logHead[] = "{\n\"version\": 1,\n\"records\":[\n";
  rv = PR_Write(logFile, logHead, sizeof(logHead) - 1);
  ioError |= (rv < 0);

  bool firstBatch = true;
  while (!gStopFlushingThread || gLogHead) {
    if (ioError) {
      gInitialized = false;
      break;
    }

    mon.Wait();

    // Grab the current log list head and start a new blank global list
    RecordBatch * batch = gLogHead;
    gLogHead = nullptr;
    gLogTail = nullptr;

    MonitorAutoUnlock unlock(*gMonitor); // no need to block on I/O :-)

    while (batch) {
      if (!firstBatch) {
        const char threadDelimiter[] = ",\n";
        rv = PR_Write(logFile, threadDelimiter, sizeof(threadDelimiter) - 1);
        ioError |= (rv < 0);
      }
      firstBatch = false;

      static const PRIntn kBufferSize = 2048;
      char buf[kBufferSize];

      PR_snprintf(buf, kBufferSize, "{\"thread\":\"%s\",\"log\":[\n", 
                  batch->mThreadNameCopy);

      rv = PR_Write(logFile, buf, strlen(buf));
      ioError |= (rv < 0);

      for (Record * record = batch->mRecordsHead;
           record < batch->mNextRecord && !ioError;
           ++record) {

        // mType carries both type and flags, separate type 
        // as lower 16 bits and flags as higher 16 bits.
        // The json format expects this separated.
        PRUint32 type = record->mType & 0xffffUL;
        PRUint32 flags = record->mType >> 16;
        PR_snprintf(buf, kBufferSize, 
          "{\"e\":\"%c\",\"t\":%f,\"f\":%d,\"i\":\"%p\",\"n\":\"%s%s\"}%s\n",
          kTypeChars[type],
          record->mTime,
          flags,
          record->mItem,
          record->mText,
          record->mText2 ? record->mText2 : "",
          (record == batch->mNextRecord - 1) ? "" : ",");

        rv = PR_Write(logFile, buf, strlen(buf));
        ioError |= (rv < 0);
      }

      const char threadTail[] = "]}\n";
      rv = PR_Write(logFile, threadTail, sizeof(threadTail) - 1);
      ioError |= (rv < 0);

      RecordBatch * next = batch->mNextBatch;
      delete batch;
      batch = next;
    }
  }

  const char logTail[] = "]}\n";
  rv = PR_Write(logFile, logTail, sizeof(logTail) - 1);
  ioError |= (rv < 0);

  PR_Close(logFile);

  if (ioError)
    PR_Delete(gLogFilePath);
}

// static
bool CheckEventFilters(PRUint32 aType, void * aItem, const char * aText)
{
  if (!gEventFilter)
    return true;

  if (aType == eName)
    return true;

  if (aItem == gFlushingThread) // Pass events coming from the tracer
    return true;

  return gEventFilter->EventPasses(aText);
}

} // anon namespace

#endif //MOZ_VISUAL_EVENT_TRACER

// static 
void Init()
{
#ifdef MOZ_VISUAL_EVENT_TRACER
  const char * logFile = PR_GetEnv("MOZ_PROFILING_FILE");
  if (!logFile || !*logFile)
    return;

  gLogFilePath = logFile;

  const char * logEvents = PR_GetEnv("MOZ_PROFILING_EVENTS");
  if (logEvents && *logEvents)
    gEventFilter = EventFilter::Build(logEvents);

  gProfilerStart = mozilla::TimeStamp::Now();

  PRStatus status = PR_NewThreadPrivateIndex(&gThreadPrivateIndex, 
                                             &RecordBatch::FlushBatch);
  if (status != PR_SUCCESS)
    return;

  gMonitor = new mozilla::Monitor("Profiler");
  if (!gMonitor)
    return;

  gFlushingThread = PR_CreateThread(PR_USER_THREAD, 
                                    &FlushingThread,
                                    nullptr,
                                    PR_PRIORITY_LOW,
                                    PR_LOCAL_THREAD,
                                    PR_JOINABLE_THREAD,
                                    32768);
  if (!gFlushingThread)
    return;
    
  gInitialized = true;

  MOZ_EVENT_TRACER_NAME_OBJECT(gFlushingThread, "Profiler");
  MOZ_EVENT_TRACER_MARK(gFlushingThread, "Profiling Start");
#endif
}

// static 
void Shutdown()
{
#ifdef MOZ_VISUAL_EVENT_TRACER
  MOZ_EVENT_TRACER_MARK(gFlushingThread, "Profiling End");

  // This must be called after all other threads had been shut down 
  // (i.e. their private data had been 'released').

  // Release the private data of this thread to flush all the remaning writes.
  PR_SetThreadPrivate(gThreadPrivateIndex, nullptr);

  if (gFlushingThread) {
    {
      MonitorAutoLock mon(*gMonitor);
      gInitialized = false;
      gStopFlushingThread = true;
      mon.Notify();
    }

    PR_JoinThread(gFlushingThread);
    gFlushingThread = nullptr;
  }

  if (gMonitor) {
    delete gMonitor;
    gMonitor = nullptr;
  }

  if (gEventFilter) {
    delete gEventFilter;
    gEventFilter = nullptr;
  }
#endif
}

// static 
void Mark(PRUint32 aType, void * aItem, const char * aText, const char * aText2)
{
#ifdef MOZ_VISUAL_EVENT_TRACER
  if (!gInitialized)
    return;

  if (aType == eNone)
    return;

  if (!CheckEventFilters(aType, aItem, aText)) // Events use just aText
    return;

  RecordBatch * threadLogPrivate = static_cast<RecordBatch *>(
      PR_GetThreadPrivate(gThreadPrivateIndex));
  if (!threadLogPrivate) {
    // Deletion is made by the flushing thread
    threadLogPrivate = new RecordBatch();
    PR_SetThreadPrivate(gThreadPrivateIndex, threadLogPrivate);
  }

  Record * record = threadLogPrivate->mNextRecord;
  record->mType = aType;
  record->mTime = (mozilla::TimeStamp::Now() - gProfilerStart).ToMilliseconds();
  record->mItem = aItem;
  record->mText = PL_strdup(aText);
  record->mText2 = aText2 ? PL_strdup(aText2) : nullptr;

  ++threadLogPrivate->mNextRecord;
  if (threadLogPrivate->mNextRecord == threadLogPrivate->mRecordsTail) {
    // This calls RecordBatch::FlushBatch(threadLogPrivate)
    PR_SetThreadPrivate(gThreadPrivateIndex, nullptr);
  }
#endif
}

} // eventtracer
} // mozilla
