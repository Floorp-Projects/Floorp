/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/VisualEventTracer.h"
#include "mozilla/Monitor.h"
#include "mozilla/TimeStamp.h"
#include "nscore.h"
#include "prthread.h"
#include "prprf.h"
#include "prenv.h"
#include "plstr.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace eventtracer {

#ifdef MOZ_VISUAL_EVENT_TRACER

namespace {

const uint32_t kBatchSize = 256;
const char kTypeChars[eventtracer::eLast] = {' ', 'N', 'S', 'W', 'E', 'D'};

// Flushing thread and records queue monitor
mozilla::Monitor* gMonitor = nullptr;

// gInitialized and gCapture can be accessed from multiple threads
// simultaneously without any locking.  However, since they are only ever
// *set* from the main thread, the chance of races manifesting is small
// and unlikely to be a problem in practice.
bool gInitialized;

// Flag to allow capturing
bool gCapture;

// Time stamp of the epoch we have started to capture
mozilla::TimeStamp* gProfilerStart;

// Duration of the log to keep up to
mozilla::TimeDuration* gMaxBacklogTime;


// Record of a single event
class Record
{
public:
  Record()
    : mType(::mozilla::eventtracer::eNone)
    , mItem(nullptr)
    , mText(nullptr)
    , mText2(nullptr)
  {
    MOZ_COUNT_CTOR(Record);
  }

  Record& operator=(const Record& aOther)
  {
    mType = aOther.mType;
    mTime = aOther.mTime;
    mItem = aOther.mItem;
    mText = PL_strdup(aOther.mText);
    mText2 = aOther.mText2 ? PL_strdup(aOther.mText2) : nullptr;
    return *this;
  }

  ~Record()
  {
    PL_strfree(mText2);
    PL_strfree(mText);
    MOZ_COUNT_DTOR(Record);
  }

  uint32_t mType;
  TimeStamp mTime;
  void* mItem;
  char* mText;
  char* mText2;
};

char* DupCurrentThreadName()
{
  if (NS_IsMainThread()) {
    return PL_strdup("Main Thread");
  }

  PRThread* currentThread = PR_GetCurrentThread();
  const char* name = PR_GetThreadName(currentThread);
  if (name) {
    return PL_strdup(name);
  }

  char buffer[128];
  PR_snprintf(buffer, 127, "Nameless %p", currentThread);

  return PL_strdup(buffer);
}

// An array of events, each thread keeps its own private instance
class RecordBatch
{
public:
  RecordBatch(size_t aLength = kBatchSize,
              char* aThreadName = DupCurrentThreadName())
    : mRecordsHead(new Record[aLength])
    , mRecordsTail(mRecordsHead + aLength)
    , mNextRecord(mRecordsHead)
    , mNextBatch(nullptr)
    , mThreadNameCopy(aThreadName)
    , mClosed(false)
  {
    MOZ_COUNT_CTOR(RecordBatch);
  }

  ~RecordBatch()
  {
    delete [] mRecordsHead;
    PL_strfree(mThreadNameCopy);
    MOZ_COUNT_DTOR(RecordBatch);
  }

  void Close()
  {
    mClosed = true;
  }

  size_t Length() const
  {
    return mNextRecord - mRecordsHead;
  }
  bool CanBeDeleted(const TimeStamp& aUntil) const;

  static RecordBatch* Register();
  static void Close(void* aData);  // Registered on freeing thread data
  static RecordBatch* Clone(RecordBatch* aLog, const TimeStamp& aSince);
  static void Delete(RecordBatch* aLog);

  static RecordBatch* CloneLog();
  static void GCLog(const TimeStamp& aUntil);
  static void DeleteLog();

  Record* mRecordsHead;
  Record* mRecordsTail;
  Record* mNextRecord;

  RecordBatch* mNextBatch;
  char* mThreadNameCopy;
  bool mClosed;
};

// Protected by gMonitor, accessed concurently
// Linked list of batches threads want to flush on disk
RecordBatch* gLogHead = nullptr;
RecordBatch* gLogTail = nullptr;

// Registers the batch in the linked list
// static
RecordBatch*
RecordBatch::Register()
{
  MonitorAutoLock mon(*gMonitor);

  if (!gInitialized) {
    return nullptr;
  }

  if (gLogHead) {
    RecordBatch::GCLog(TimeStamp::Now() - *gMaxBacklogTime);
  }

  RecordBatch* batch = new RecordBatch();
  if (!gLogHead) {
    gLogHead = batch;
  } else { // gLogTail is non-null
    gLogTail->mNextBatch = batch;
  }
  gLogTail = batch;

  mon.Notify();
  return batch;
}

void
RecordBatch::Close(void* aData)
{
  RecordBatch* batch = static_cast<RecordBatch*>(aData);
  batch->Close();
}

// static
RecordBatch*
RecordBatch::Clone(RecordBatch* aOther, const TimeStamp& aSince)
{
  if (!aOther) {
    return nullptr;
  }

  size_t length = aOther->Length();
  size_t min = 0;
  size_t max = length;
  Record* record = nullptr;

  // Binary search for record with time >= aSince
  size_t i;
  while (min < max) {
    i = (max + min) / 2;

    record = aOther->mRecordsHead + i;
    if (record->mTime >= aSince) {
      max = i;
    } else {
      min = i + 1;
    }
  }
  i = (max + min) / 2;

  // How many Record's to copy?
  size_t toCopy = length - i;
  if (!toCopy) {
    return RecordBatch::Clone(aOther->mNextBatch, aSince);
  }

  // Clone
  RecordBatch* clone = new RecordBatch(toCopy, PL_strdup(aOther->mThreadNameCopy));
  for (; i < length; ++i) {
    record = aOther->mRecordsHead + i;
    *clone->mNextRecord = *record;
    ++clone->mNextRecord;
  }
  clone->mNextBatch = RecordBatch::Clone(aOther->mNextBatch, aSince);

  return clone;
}

// static
void
RecordBatch::Delete(RecordBatch* aLog)
{
  while (aLog) {
    RecordBatch* batch = aLog;
    aLog = aLog->mNextBatch;
    delete batch;
  }
}

// static
RecordBatch*
RecordBatch::CloneLog()
{
  TimeStamp startEpoch = *gProfilerStart;
  TimeStamp backlogEpoch = TimeStamp::Now() - *gMaxBacklogTime;

  TimeStamp since = (startEpoch > backlogEpoch) ? startEpoch : backlogEpoch;

  MonitorAutoLock mon(*gMonitor);

  return RecordBatch::Clone(gLogHead, since);
}

// static
void
RecordBatch::GCLog(const TimeStamp& aUntil)
{
  // Garbage collect all unreferenced and old batches
  gMonitor->AssertCurrentThreadOwns();

  RecordBatch* volatile* referer = &gLogHead;
  gLogTail = nullptr;

  RecordBatch* batch = *referer;
  while (batch) {
    if (batch->CanBeDeleted(aUntil)) {
      // The batch is completed and thus unreferenced by the thread
      // and the most recent record has time older then the time
      // we want to save records for, hence delete it.
      *referer = batch->mNextBatch;
      delete batch;
      batch = *referer;
    } else {
      // We walk the whole list, so this will end up filled with
      // the very last valid element of it.
      gLogTail = batch;
      // The current batch is active, examine the next in the list.
      batch = batch->mNextBatch;
      // When the next batch is found expired, we must extract it
      // from the list, shift the referer.
      referer = &((*referer)->mNextBatch);
    }
  }
}

// static
void
RecordBatch::DeleteLog()
{
  RecordBatch* batch;
  {
    MonitorAutoLock mon(*gMonitor);
    batch = gLogHead;
    gLogHead = nullptr;
    gLogTail = nullptr;
  }

  RecordBatch::Delete(batch);
}

bool
RecordBatch::CanBeDeleted(const TimeStamp& aUntil) const
{
  if (mClosed) {
    // This flag is set when a thread releases this batch as
    // its private data.  It happens when the list is full or
    // when the thread ends its job.  We must not delete this
    // batch from memory while it's held by a thread.

    if (!Length()) {
      // There are no records, just get rid of this empty batch.
      return true;
    }

    if ((mNextRecord - 1)->mTime <= aUntil) {
      // Is the last record older then the time we demand records
      // for?  If not, this batch has expired.
      return true;
    }
  }

  // Not all conditions to close the batch met, keep it.
  return false;
}

// Helper class for filtering events by MOZ_PROFILING_EVENTS
class EventFilter
{
public:
  static EventFilter* Build(const char* aFilterVar);
  bool EventPasses(const char* aFilterVar);

  ~EventFilter()
  {
    delete mNext;
    PL_strfree(mFilter);
    MOZ_COUNT_DTOR(EventFilter);
  }

private:
  EventFilter(const char* aEventName, EventFilter* aNext)
    : mFilter(PL_strdup(aEventName))
    , mNext(aNext)
  {
    MOZ_COUNT_CTOR(EventFilter);
  }

  char* mFilter;
  EventFilter* mNext;
};

// static
EventFilter*
EventFilter::Build(const char* aFilterVar)
{
  if (!aFilterVar || !*aFilterVar) {
    return nullptr;
  }

  // Reads a comma serpatated list of events.

  // Copied from nspr logging code (read of NSPR_LOG_MODULES)
  char eventName[64];
  int pos = 0, count, delta = 0;

  // Read up to a comma or EOF -> get name of an event first in the list
  count = sscanf(aFilterVar, "%63[^,]%n", eventName, &delta);
  if (count == 0) {
    return nullptr;
  }

  pos = delta;

  // Skip a comma, if present, accept spaces around it
  count = sscanf(aFilterVar + pos, " , %n", &delta);
  if (count != EOF) {
    pos += delta;
  }

  // eventName contains name of the first event in the list
  // second argument recursively parses the rest of the list string and
  // fills mNext of the just created EventFilter object chaining the objects
  return new EventFilter(eventName, Build(aFilterVar + pos));
}

bool
EventFilter::EventPasses(const char* aEventName)
{
  if (!strcmp(aEventName, mFilter)) {
    return true;
  }

  if (mNext) {
    return mNext->EventPasses(aEventName);
  }

  return false;
}

// State and control variables, initialized in Init() method, after it
// immutable and read concurently.
EventFilter* gEventFilter = nullptr;
unsigned gThreadPrivateIndex;

// static
bool
CheckEventFilters(uint32_t aType, void* aItem, const char* aText)
{
  if (!gEventFilter) {
    return true;
  }

  if (aType == eName) {
    return true;
  }

  return gEventFilter->EventPasses(aText);
}

} // anon namespace

#endif //MOZ_VISUAL_EVENT_TRACER

// static
void
Init()
{
#ifdef MOZ_VISUAL_EVENT_TRACER
  const char* logEvents = PR_GetEnv("MOZ_PROFILING_EVENTS");
  if (logEvents && *logEvents) {
    gEventFilter = EventFilter::Build(logEvents);
  }

  PRStatus status = PR_NewThreadPrivateIndex(&gThreadPrivateIndex,
                                             &RecordBatch::Close);
  if (status != PR_SUCCESS) {
    return;
  }

  gMonitor = new mozilla::Monitor("Profiler");
  if (!gMonitor) {
    return;
  }

  gProfilerStart = new mozilla::TimeStamp();
  gMaxBacklogTime = new mozilla::TimeDuration();

  gInitialized = true;
#endif
}

// static
void
Shutdown()
{
#ifdef MOZ_VISUAL_EVENT_TRACER
  gCapture = false;
  gInitialized = false;

  RecordBatch::DeleteLog();

  delete gMonitor;
  gMonitor = nullptr;

  delete gEventFilter;
  gEventFilter = nullptr;

  delete gProfilerStart;
  gProfilerStart = nullptr;

  delete gMaxBacklogTime;
  gMaxBacklogTime = nullptr;
#endif
}

// static
void
Mark(uint32_t aType, void* aItem, const char* aText, const char* aText2)
{
#ifdef MOZ_VISUAL_EVENT_TRACER
  if (!gInitialized || !gCapture) {
    return;
  }

  if (aType == eNone) {
    return;
  }

  if (!CheckEventFilters(aType, aItem, aText)) { // Events use just aText
    return;
  }

  RecordBatch* threadLogPrivate =
    static_cast<RecordBatch*>(PR_GetThreadPrivate(gThreadPrivateIndex));
  if (!threadLogPrivate) {
    threadLogPrivate = RecordBatch::Register();
    if (!threadLogPrivate) {
      return;
    }

    PR_SetThreadPrivate(gThreadPrivateIndex, threadLogPrivate);
  }

  Record* record = threadLogPrivate->mNextRecord;
  record->mType = aType;
  record->mTime = mozilla::TimeStamp::Now();
  record->mItem = aItem;
  record->mText = PL_strdup(aText);
  record->mText2 = aText2 ? PL_strdup(aText2) : nullptr;

  ++threadLogPrivate->mNextRecord;
  if (threadLogPrivate->mNextRecord == threadLogPrivate->mRecordsTail) {
    // Calls RecordBatch::Close(threadLogPrivate) that marks
    // the batch as OK to be deleted from memory when no longer needed.
    PR_SetThreadPrivate(gThreadPrivateIndex, nullptr);
  }
#endif
}


#ifdef MOZ_VISUAL_EVENT_TRACER

// The scriptable classes

class VisualEventTracerLog MOZ_FINAL: public nsIVisualEventTracerLog
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIVISUALEVENTTRACERLOG

  VisualEventTracerLog(RecordBatch* aBatch)
    : mBatch(aBatch)
    , mProfilerStart(*gProfilerStart)
  {
  }
private:
  ~VisualEventTracerLog();

protected:
  RecordBatch* mBatch;
  TimeStamp mProfilerStart;
};

NS_IMPL_ISUPPORTS(VisualEventTracerLog, nsIVisualEventTracerLog)

VisualEventTracerLog::~VisualEventTracerLog()
{
  RecordBatch::Delete(mBatch);
}

NS_IMETHODIMP
VisualEventTracerLog::GetJSONString(nsACString& aResult)
{
  nsCString buffer;
  buffer.AssignLiteral("{\n\"version\": 1,\n\"records\":[\n");

  RecordBatch* batch = mBatch;
  while (batch) {
    if (batch != mBatch) {
      // This is not the first batch we are writting, add comma
      buffer.AppendLiteral(",\n");
    }

    buffer.AppendLiteral("{\"thread\":\"");
    buffer.Append(batch->mThreadNameCopy);
    buffer.AppendLiteral("\",\"log\":[\n");

    static const int kBufferSize = 2048;
    char buf[kBufferSize];

    for (Record* record = batch->mRecordsHead;
         record < batch->mNextRecord;
         ++record) {

      // mType carries both type and flags, separate type
      // as lower 16 bits and flags as higher 16 bits.
      // The json format expects this separated.
      uint32_t type = record->mType & 0xffffUL;
      uint32_t flags = record->mType >> 16;
      PR_snprintf(buf, kBufferSize,
        "{\"e\":\"%c\",\"t\":%llu,\"f\":%d,\"i\":\"%p\",\"n\":\"%s%s\"}%s\n",
        kTypeChars[type],
        static_cast<uint64_t>((record->mTime - mProfilerStart).ToMilliseconds()),
        flags,
        record->mItem,
        record->mText,
        record->mText2 ? record->mText2 : "",
        (record == batch->mNextRecord - 1) ? "" : ",");

      buffer.Append(buf);
    }

    buffer.AppendLiteral("]}\n");

    RecordBatch* next = batch->mNextBatch;
    batch = next;
  }

  buffer.AppendLiteral("]}\n");
  aResult.Assign(buffer);

  return NS_OK;
}

nsresult
VisualEventTracerLog::WriteToProfilingFile()
{
  const char* filename = PR_GetEnv("MOZ_TRACE_FILE");
  if (!filename) {
    return NS_OK;
  }

  PRFileDesc* fd = PR_Open(filename, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
                           0644);
  if (!fd) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  nsCString json;
  GetJSONString(json);

  int32_t bytesWritten = PR_Write(fd, json.get(), json.Length());
  PR_Close(fd);

  if (bytesWritten < json.Length()) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(VisualEventTracer, nsIVisualEventTracer)

NS_IMETHODIMP
VisualEventTracer::Start(const uint32_t aMaxBacklogSeconds)
{
  if (!gInitialized) {
    return NS_ERROR_UNEXPECTED;
  }

  if (gCapture) {
    NS_WARNING("VisualEventTracer has already been started");
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  *gMaxBacklogTime = TimeDuration::FromMilliseconds(aMaxBacklogSeconds * 1000);

  *gProfilerStart = mozilla::TimeStamp::Now();
  {
    MonitorAutoLock mon(*gMonitor);
    RecordBatch::GCLog(*gProfilerStart);
  }
  gCapture = true;

  MOZ_EVENT_TRACER_MARK(this, "trace::start");

  return NS_OK;
}

NS_IMETHODIMP
VisualEventTracer::Stop()
{
  if (!gInitialized) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!gCapture) {
    NS_WARNING("VisualEventTracer is not runing");
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_EVENT_TRACER_MARK(this, "trace::stop");

  gCapture = false;

  nsresult rv = NS_OK;
  if (PR_GetEnv("MOZ_TRACE_FILE")) {
    nsCOMPtr<nsIVisualEventTracerLog> tracelog;
    rv = Snapshot(getter_AddRefs(tracelog));
    if (NS_SUCCEEDED(rv)) {
      rv = tracelog->WriteToProfilingFile();
    }
  }

  return rv;
}

NS_IMETHODIMP
VisualEventTracer::Snapshot(nsIVisualEventTracerLog** aResult)
{
  if (!gInitialized) {
    return NS_ERROR_UNEXPECTED;
  }

  RecordBatch* batch = RecordBatch::CloneLog();

  nsRefPtr<VisualEventTracerLog> log = new VisualEventTracerLog(batch);
  log.forget(aResult);

  return NS_OK;
}

#endif

} // eventtracer
} // mozilla
