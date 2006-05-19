/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Storage
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *   Ben Turner <mozilla@songbirdnest.com>
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

/**
 * This code is partially based on test_async.c distributed with sqlite
 *
 * Error handling:
 *
 * When there is a write error, we don't know what to do, since the program
 * has continued and has assumed that the operation has succeeded. Instead,
 * we set the flag AsyncWriteError. When set, every file operation will fail,
 * so the errors will get passed to the program. Since the error occured
 * asynchronously, there is no easy way to figure out what was happening,
 * undo it, and recover.
 *
 * COMMENTS FROM SQLITE DEMO FILE
 * ==============================
 *
 * This file contains an example implementation of an asynchronous IO 
 * backend for SQLite.
 *
 * WHAT IS ASYNCHRONOUS I/O?
 *
 * With asynchronous I/O, write requests are handled by a separate thread
 * running in the background.  This means that the thread that initiates
 * a database write does not have to wait for (sometimes slow) disk I/O
 * to occur.  The write seems to happen very quickly, though in reality
 * it is happening at its usual slow pace in the background.
 *
 * Asynchronous I/O appears to give better responsiveness, but at a price.
 * You lose the Durable property.  With the default I/O backend of SQLite,
 * once a write completes, you know that the information you wrote is
 * safely on disk.  With the asynchronous I/O, this is no the case.  If
 * your program crashes or if you take a power lose after the database
 * write but before the asynchronous write thread has completed, then the
 * database change might never make it to disk and the next user of the
 * database might not see your change.
 *
 * You lose Durability with asynchronous I/O, but you still retain the
 * other parts of ACID:  Atomic,  Consistent, and Isolated.  Many
 * appliations get along fine without the Durablity.
 *
 * HOW IT WORKS
 *
 * Asynchronous I/O works by overloading the OS-layer disk I/O routines
 * with modified versions that store the data to be written in queue of
 * pending write operations.  Look at the asyncEnable() subroutine to see
 * how overloading works.  Six os-layer routines are overloaded:
 *
 *     sqlite3OsOpenReadWrite;
 *     sqlite3OsOpenReadOnly;
 *     sqlite3OsOpenExclusive;
 *     sqlite3OsDelete;
 *     sqlite3OsFileExists;
 *     sqlite3OsSyncDirectory;
 *
 * The original implementations of these routines are saved and are
 * used by the writer thread to do the real I/O.  The substitute
 * implementations typically put the I/O operation on a queue
 * to be handled later by the writer thread, though read operations
 * must be handled right away, obviously.
 *
 * Asynchronous I/O is disabled by setting the os-layer interface routines
 * back to their original values.
 *
 * LIMITATIONS
 *
 * This demonstration code is deliberately kept simple in order to keep
 * the main ideas clear and easy to understand.  Real applications that
 * want to do asynchronous I/O might want to add additional capabilities.
 * For example, in this demonstration if writes are happening at a steady
 * stream that exceeds the I/O capability of the background writer thread,
 * the queue of pending write operations will grow without bound until we
 * run out of memory.  Users of this technique may want to keep track of
 * the quantity of pending writes and stop accepting new write requests
 * when the buffer gets to be too big.
 *
 * THREAD SAFETY NOTES
 *
 * Basic rules:
 *
 *     * Both read and write access to the global write-op queue must be 
 *       protected by the async.queueMutex.
 *
 *     * The file handles from the underlying system are assumed not to 
 *       be thread safe.
 *
 *     * See the last two paragraphs under "The Writer Thread" for
 *       an assumption to do with file-handle synchronization by the Os.
 *
 * File system operations (invoked by SQLite thread):
 *
 *     xOpenXXX (three versions)
 *     xDelete
 *     xFileExists
 *     xSyncDirectory
 *
 * File handle operations (invoked by SQLite thread):
 *
 *         asyncWrite, asyncClose, asyncTruncate, asyncSync,
 *         asyncSetFullSync, asyncOpenDirectory.
 *
 *     The operations above add an entry to the global write-op list. They
 *     prepare the entry, acquire the async.queueMutex momentarily while
 *     list pointers are  manipulated to insert the new entry, then release
 *     the mutex and signal the writer thread to wake up in case it happens
 *     to be asleep.
 *
 *         asyncRead, asyncFileSize.
 *
 *     Read operations. Both of these read from both the underlying file
 *     first then adjust their result based on pending writes in the
 *     write-op queue.   So async.queueMutex is held for the duration
 *     of these operations to prevent other threads from changing the
 *     queue in mid operation.
 *
 *         asyncLock, asyncUnlock, asyncLockState, asyncCheckReservedLock
 *
 *     These locking primitives become no-ops. Files are always opened for
 *     exclusive access when using this IO backend.
 *
 *         asyncFileHandle.
 *
 *     The sqlite3OsFileHandle() function is currently only used when
 *     debugging the pager module. Unless sqlite3OsClose() is called on the
 *     file (shouldn't be possible for other reasons), the underlying
 *     implementations are safe to call without grabbing any mutex. So we just
 *     go ahead and call it no matter what any other threads are doing.
 *
 *         asyncSeek.
 *
 *     Calling this method just manipulates the AsyncFile.iOffset variable.
 *     Since this variable is never accessed by writer thread, this
 *     function does not require the mutex.  Actual calls to OsSeek() take
 *     place just before OsWrite() or OsRead(), which are always protected by
 *     the mutex.
 *
 * The writer thread:
 *
 *     The async.writerMutex is used to make sure only there is only
 *     a single writer thread running at a time.
 *
 *     Inside the writer thread is a loop that works like this:
 *
 *         WHILE (write-op list is not empty)
 *             Do IO operation at head of write-op list
 *             Remove entry from head of write-op list
 *         END WHILE
 *
 *     The async.queueMutex is always held during the <write-op list is
 *     not empty> test, and when the entry is removed from the head
 *     of the write-op list. Sometimes it is held for the interim
 *     period (while the IO is performed), and sometimes it is
 *     relinquished. It is relinquished if (a) the IO op is an
 *     ASYNC_CLOSE or (b) when the file handle was opened, two of
 *     the underlying systems handles were opened on the same
 *     file-system entry.
 *
 *     If condition (b) above is true, then one file-handle
 *     (AsyncFile.pBaseRead) is used exclusively by sqlite threads to read the
 *     file, the other (AsyncFile.pBaseWrite) by sqlite3_async_flush()
 *     threads to perform write() operations. This means that read
 *     operations are not blocked by asynchronous writes (although
 *     asynchronous writes may still be blocked by reads).
 *
 *     This assumes that the OS keeps two handles open on the same file
 *     properly in sync. That is, any read operation that starts after a
 *     write operation on the same file system entry has completed returns
 *     data consistent with the write. We also assume that if one thread
 *     reads a file while another is writing it all bytes other than the
 *     ones actually being written contain valid data.
 *
 *     If the above assumptions are not true, set the preprocessor symbol
 *     SQLITE_ASYNC_TWO_FILEHANDLES to 0.
 *
 * ----------------------------------------------------------------------------
 * VERSIONING
 *
 *     This file was originally based on version 1.5 of test_async.c in sqlite,
 *     see http://www.sqlite.org/cvstrac/rlog?f=sqlite/src/test_async.c
 *     Versions 1.6 and 1.7 were based on errors found and fixed here.
 *
 *     We've incorportated the patches for versions 1.12, 1.13, 1.15, 1.17
 *     (which backs out some of the changes in 1.13)
 *
 *     FIXME: It would be nice to have "fake" in-process file locking as in
 *     versions 1.11.
 *
 *     Our error handling is a little more coarse than the ones implemented
 *     in versions 1.8 and 1.14. Those ones count the number of files and
 *     reset the error value when all files are closed. This allows one to
 *     potentially recover by closing all the connections and reopening them.
 *     We don't handle this from the calling code, so there's no point in
 *     implementing that here. Instead we just fail all operations on error.
 */

#include "mozStorageService.h"
#include "nsAutoLock.h"
#include "nsThreadUtils.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsMemory.h"
#include "nsProxyRelease.h"
#include "plstr.h"
#include "prlock.h"
#include "prcvar.h"
#include "prtypes.h"

#include "sqlite3.h"
#include "sqlite3file.h"

// See below for some discussion on this. This will let us use a reader
// filehandle and a writer filehandle so that read operations are not blocked
// by asychronous writes.
#define SQLITE_ASYNC_TWO_FILEHANDLES 1

//#define SINGLE_THREADED

// define this to wait this many ms after every IO operation. Good for
// emulating slow disks and for verification: Set this to some value like
// 200-1000ms and do a bunch of stuff. This will make writes fall behind reads
// and will test that everything stays in sync. Also try doing stuff and then
// exiting to be sure that everything gets flushed on exit.
//
// Undefine for not waiting.
//#define IO_DELAY_INTERVAL_MS 30

// AsyncOsFile
//
//    This is a wrapper around the sqlite interal OsFile.

struct AsyncOsFile : public OsFile
{
  // This keeps track of the current file offset. Seek operations change this
  // offset instead of actually changing the file because we will do stuff to
  // the file in the background. We store this offset for each operation such
  // as reading and writing so that when it occurs later we know where it
  // actually was.
  sqlite_int64 mOffset;

  // Set to true normally, false when the file is closed. This way we know not
  // to accept any more operations for a closed file (even if the close is
  // pending).
  PRBool mOpen;

  OsFile* mBaseRead;
  OsFile* mBaseWrite;
};

// AsyncMessage
//
//    Entries on the write-op queue are instances of the AsyncMessage
//    structure, defined here.
//
//    The interpretation of the iOffset and mBytes variables varies depending 
//    on the value of AsyncMessage.mOp:
//
//    ASYNC_WRITE:
//        mOffset -> Offset in file to write to.
//        mBytes  -> Number of bytes of data to write (pointed to by zBuf).
//
//    ASYNC_SYNC:
//        mOffset -> Unused.
//        mBytes  -> Value of "fullsync" flag to pass to sqlite3OsSync().
//
//    ASYNC_TRUNCATE:
//        mOffset -> Size to truncate file to.
//        mBytes  -> Unused.
//
//    ASYNC_CLOSE:
//        mOffset -> Unused.
//        mBytes  -> Unused.
//
//    ASYNC_OPENDIRECTORY:
//        mOffset -> Unused.
//        mBytes  -> Number of bytes of zBuf points to (directory name).
//
//    ASYNC_SETFULLSYNC:
//        mOffset -> Unused.
//        mBytes  -> New value for the full-sync flag.
//
//    ASYNC_DELETE:
//        mOffset -> Unused.
//        mBytes  -> Number of bytes of zBuf points to (file name).
//
//    ASYNC_OPENEXCLUSIVE:
//        mOffset -> Value of "delflag".
//        mBytes  -> Number of bytes of zBuf points to (file name).
//
//    For an ASYNC_WRITE operation, zBuf points to the data to write to the file. 
//    This space is sqliteMalloc()d along with the AsyncMessage structure in a
//    single blob, so is deleted when sqliteFree() is called on the parent 
//    structure.

struct AsyncMessage
{
  // File to write data or to sync
  AsyncOsFile* mFile;

  // One of ASYNC_xxx etc.
  PRUint32 mOp;            // (was op)
  sqlite_int64 mOffset;    // See above (was iOffset)
  PRInt32 mBytes;          // See above (was nByte)

  // Data to write to file (or NULL if op != ASYNC_WRITE)
  // YOU DO NOT NEED TO FREE THIS SEPARATELY. AppendNewAsyncMessage allocates
  // a single buffer with mBuf pointing to the memory immediately following
  // this structure. Freeing this structure will also free mBuf.
  char *mBuf;

  // Next write operation (to any file) in the linked list
  AsyncMessage* mNext;
};

// Possible values of AsyncMessage.mOp
#define ASYNC_WRITE         1
#define ASYNC_SYNC          2
#define ASYNC_TRUNCATE      3
#define ASYNC_CLOSE         4
#define ASYNC_OPENDIRECTORY 5
#define ASYNC_SETFULLSYNC   6
#define ASYNC_DELETE        7
#define ASYNC_OPENEXCLUSIVE 8
#define ASYNC_SYNCDIRECTORY 9

// replacements for the sqlite OS routines
static int AsyncOpenReadWrite(const char *aName, OsFile **aFile, int *aReadOnly);
static int AsyncOpenExclusive(const char *aName, OsFile **aFile, int aDelFlag);
static int AsyncOpenReadOnly(const char *aName, OsFile **aFile);
static int AsyncDelete(const char* aName);
static int AsyncSyncDirectory(const char* aName);
static int AsyncFileExists(const char *aName);
static int AsyncClose(OsFile** aFile);
static int AsyncWrite(OsFile* aFile, const void* aBuf, int aCount);
static int AsyncTruncate(OsFile* aFile, sqlite_int64 aNumBytes);
static int AsyncOpenDirectory(OsFile* aFile, const char* aName);
static int AsyncSync(OsFile* aFile, int aFullsync);
static void AsyncSetFullSync(OsFile* aFile, int aValue);
static int AsyncRead(OsFile* aFile, void *aBuffer, int aCount);
static int AsyncSeek(OsFile* aFile, sqlite_int64 aOffset);
static int AsyncFileSize(OsFile* aFile, sqlite_int64* aSize);
static int AsyncFileHandle(OsFile* aFile);
static int AsyncLock(OsFile* aFile, int aLockType);
static int AsyncUnlock(OsFile* aFile, int aLockType);
static int AsyncCheckReservedLock(OsFile* aFile);
static int AsyncLockState(OsFile* aFile);

// backend for all the open functions
static int AsyncOpenFile(const char *aName, AsyncOsFile **aFile,
                     OsFile *aBaseRead, PRBool aOpenForWriting);

// message queue
static AsyncMessage* AsyncQueueFirst = nsnull;
static AsyncMessage* AsyncQueueLast = nsnull;
#ifdef SINGLE_THREADED
  // this causes the processing function to return as soon as the messages
  // have been processed
  static PRBool AsyncWriterHaltWhenIdle = PR_TRUE;
#else
  static PRBool AsyncWriterHaltWhenIdle = PR_FALSE;
#endif
static void ProcessAsyncMessages();
static int ProcessOneMessage(AsyncMessage* aMessage);
static void AppendAsyncMessage(AsyncMessage* aMessage);
static int AppendNewAsyncMessage(AsyncOsFile* aFile, PRUint32 aOp,
                                 sqlite_int64 aOffset, PRInt32 aDataSize,
                                 const char *aData);
static int AsyncWriteError = SQLITE_OK; // set on write error

// threading
// serializes access to the queue, AsyncWriteThreadInstance = nsnull means
// single-threaded mode
static nsIThread* AsyncWriteThreadInstance = nsnull;
static PRLock* AsyncQueueLock = nsnull;
static PRCondVar* AsyncQueueCondition = nsnull; // set when queue has something in it

// pointers to the original sqlite file I/O routines
static int (*sqliteOrigOpenReadWrite)(const char*, OsFile**, int*) = nsnull;
static int (*sqliteOrigOpenExclusive)(const char*, OsFile**, int) = nsnull;
static int (*sqliteOrigOpenReadOnly)(const char*, OsFile**) = nsnull;
static int (*sqliteOrigDelete)(const char*) = nsnull;
static int (*sqliteOrigFileExists)(const char*) = nsnull;
static int (*sqliteOrigSyncDirectory)(const char*) = nsnull;

// pointers to the original file I/O routines associated with an open file
// these are populated the first time we open a file
static int (*sqliteOrigClose)(OsFile**) = nsnull;
static int (*sqliteOrigRead)(OsFile*, void*, int amt) = nsnull;
static int (*sqliteOrigWrite)(OsFile*, const void*, int amt) = nsnull;
static int (*sqliteOrigFileSize)(OsFile*, sqlite_int64 *pSize) = nsnull;
static int (*sqliteOrigSeek)(OsFile*, sqlite_int64 offset) = nsnull;
static int (*sqliteOrigSync)(OsFile*, int) = nsnull;
static int (*sqliteOrigTruncate)(OsFile*, sqlite_int64 size) = nsnull;
static int (*sqliteOrigOpenDirectory)(OsFile*, const char*);
static void (*sqliteOrigSetFullSync)(OsFile*, int setting);


#ifndef SINGLE_THREADED
class AsyncWriteThread : public nsIRunnable
{
public:
  AsyncWriteThread(mozIStorageService* aStorageService) :
    mStorageService(aStorageService) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Run()
  {
    NS_ASSERTION(! AsyncWriterHaltWhenIdle, "You don't want halt on idle when starting up!");
    ProcessAsyncMessages();

    // this will delay processing the release of the storage service until we
    // get to the main thread.
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_SUCCEEDED(rv)) {
      mozIStorageService* service = nsnull;
      mStorageService.swap(service);
      NS_ProxyRelease(mainThread, service);
    } else {
      NS_NOTREACHED("No event queue");
    }
    return NS_OK;
  }

protected:
  // The thread must keep a reference to the storage service to make sure the
  // thread is destroyed before the storage service is. When the storage service
  // is done, it will release all the locks that this thread is using. This
  // makes sure our locks don't get deleted until we're done using them.
  nsCOMPtr<mozIStorageService> mStorageService;
};
NS_IMPL_THREADSAFE_ISUPPORTS1(AsyncWriteThread, nsIRunnable)
#endif // ! SINGLE_THREADED


// mozStorageService::InitStorageAsyncIO
//
//    This function must be called before any data base connections have been
//    opened.

nsresult
mozStorageService::InitStorageAsyncIO()
{
  sqlite3OsVtbl* vtable = sqlite3_os_switch();

  sqliteOrigOpenReadWrite = vtable->xOpenReadWrite;
  sqliteOrigOpenReadOnly = vtable->xOpenReadOnly;
  sqliteOrigOpenExclusive = vtable->xOpenExclusive;
  sqliteOrigDelete = vtable->xDelete;
  sqliteOrigFileExists = vtable->xFileExists;
  sqliteOrigSyncDirectory = vtable->xSyncDirectory;

  vtable->xOpenReadWrite = AsyncOpenReadWrite;
  vtable->xOpenReadOnly = AsyncOpenReadOnly;
  vtable->xOpenExclusive = AsyncOpenExclusive;
  vtable->xDelete = AsyncDelete;
  vtable->xFileExists = AsyncFileExists;
  vtable->xSyncDirectory = AsyncSyncDirectory;

  // AsyncQueueLock
  AsyncQueueLock = PR_NewLock();
  if (! AsyncQueueLock) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // AsyncQueueCondition
  AsyncQueueCondition = PR_NewCondVar(AsyncQueueLock);
  if (! AsyncQueueCondition)
    return NS_ERROR_OUT_OF_MEMORY;

#ifndef SINGLE_THREADED
  // start the writer thread
  nsCOMPtr<nsIRunnable> thread = new AsyncWriteThread(this);
  if (! thread)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = NS_NewThread(&AsyncWriteThreadInstance, thread);
  if (NS_FAILED(rv)) {
    AsyncWriteThreadInstance = nsnull;
    return rv;
  }
#endif

  return NS_OK;
}


// mozStorageService::FinishAsyncIO
//
//    Call this function on shutdown to ensure that all buffered writes have
//    been comitted to disk. This then puts us into sychronous write mode. Any
//    subsequent database operations will be blocking. This way, we don't care
//    about the shutdown order of components. Other components can still
//    continue to use the database as we shut down, it just won't be buffered.
//    (Which is usually fine since we have to wait for these to be flushed
//    before we can exit anyway.)

nsresult
mozStorageService::FinishAsyncIO()
{
  {
    nsAutoLock lock(AsyncQueueLock);

    if (!AsyncWriteThreadInstance)
      return NS_OK; // single-threaded mode, nothing to do

    // this will tell the writer to exit when the message queue is empty.
    AsyncWriterHaltWhenIdle = PR_TRUE;

    // this will wake up the writer thread when we release the lock
    PR_NotifyAllCondVar(AsyncQueueCondition);
  }

  // now we join with the writer thread
  AsyncWriteThreadInstance->Shutdown();

  // release the thread and enter single-threaded mode
  NS_RELEASE(AsyncWriteThreadInstance);
  AsyncWriteThreadInstance = nsnull;

  return NS_OK;
}


// mozStorageService::FreeLocks
//
//    The locks must be associated with the service so that the destruction
//    is cleanly handled ay service shutdown without the tread trying to
//    unlock a destroyed lock.

void
mozStorageService::FreeLocks()
{
  // Destroy the condition variables
  if (AsyncQueueCondition) {
    PR_DestroyCondVar(AsyncQueueCondition);
    AsyncQueueCondition = nsnull;
  }

  if (AsyncQueueLock) {
    PR_DestroyLock(AsyncQueueLock);
    AsyncQueueLock = nsnull;
  }
}


// AsyncOpenFile
//
//    This routine does most of the work of opening a file and building the
//    OsFile structure. On error, it will close the input file aBaseRead.
//
//    @param aName            The name of the file to be opened
//    @paran aFile            Put the OsFile structure here
//    @param aBaseRead        The real OsFile from the real I/O routine
//    @param aOpenForWriting  Open a second file handle for writing if true

int
AsyncOpenFile(const char* aName, AsyncOsFile** aFile,
                                 OsFile* aBaseRead, PRBool aOpenForWriting)
{
  int rc;
  OsFile *baseWrite = nsnull;

  if (! sqliteOrigClose) {
    sqliteOrigClose = aBaseRead->pMethod->xClose;
    sqliteOrigRead = aBaseRead->pMethod->xRead;
    sqliteOrigWrite = aBaseRead->pMethod->xWrite;
    sqliteOrigFileSize = aBaseRead->pMethod->xFileSize;
    sqliteOrigSeek = aBaseRead->pMethod->xSeek;
    sqliteOrigSync = aBaseRead->pMethod->xSync;
    sqliteOrigTruncate = aBaseRead->pMethod->xTruncate;
    sqliteOrigOpenDirectory = aBaseRead->pMethod->xOpenDirectory;
    sqliteOrigSetFullSync = aBaseRead->pMethod->xSetFullSync;
  }

  static IoMethod iomethod = {
    AsyncClose,
    AsyncOpenDirectory,
    AsyncRead,
    AsyncWrite,
    AsyncSeek,
    AsyncTruncate,
    AsyncSync,
    AsyncSetFullSync,
    AsyncFileHandle,
    AsyncFileSize,
    AsyncLock,
    AsyncUnlock,
    AsyncLockState,
    AsyncCheckReservedLock
  };

  if (aOpenForWriting && SQLITE_ASYNC_TWO_FILEHANDLES) {
    int dummy;
    rc = sqliteOrigOpenReadWrite(aName, &baseWrite, &dummy);
    if (rc != SQLITE_OK)
      goto error_out;
  }

  *aFile = NS_STATIC_CAST(AsyncOsFile*, nsMemory::Alloc(sizeof(AsyncOsFile)));
  if (! *aFile) {
    rc = SQLITE_NOMEM;
    goto error_out;
  }
  memset(*aFile, 0, sizeof(AsyncOsFile));

  (*aFile)->pMethod = &iomethod;
  (*aFile)->mOpen = PR_TRUE;
  (*aFile)->mBaseRead = aBaseRead;
  (*aFile)->mBaseWrite = baseWrite;

  return SQLITE_OK;

error_out:
  NS_ASSERTION(!*aFile, "File not cleared on error");
  sqliteOrigClose(&aBaseRead);
  sqliteOrigClose(&baseWrite);
  return rc;
}


// AppendAsyncMessage
//
//    Add an entry to the end of the global write-op list. pWrite should point
//    to an AsyncMessage structure allocated using nsMemory::Alloc().  The
//    writer thread will call nsMemory::Free() to free the structure after the
//    specified operation has been completed.
//
//    Once an AsyncMessage structure has been added to the list, it becomes the
//    property of the writer thread and must not be read or modified by the
//    caller.

void
AppendAsyncMessage(AsyncMessage* aMessage)
{
  // We must hold the queue mutex in order to modify the queue pointers
  PR_Lock(AsyncQueueLock);

  // Add the record to the end of the write-op queue
  NS_ASSERTION(! aMessage->mNext, "New messages should not have next pointers");
  if (AsyncQueueLast) {
    NS_ASSERTION(AsyncQueueFirst, "If we have a last item, we need to have a first one");
    AsyncQueueLast->mNext = aMessage;
  } else {
    AsyncQueueFirst = aMessage;
  }
  AsyncQueueLast = aMessage;

  // The writer thread might have been idle because there was nothing on the
  // write-op queue for it to do. So wake it up.
  if (AsyncWriteThreadInstance) {
    PR_NotifyCondVar(AsyncQueueCondition);
    PR_Unlock(AsyncQueueLock);
  } else {
    // single threaded mode: call the writer to process this message
    NS_ASSERTION(AsyncWriterHaltWhenIdle, "In single-threaded mode, the writer thread should always halt when idle");
    PR_Unlock(AsyncQueueLock);
    ProcessAsyncMessages();
  }
}


// AppendNewAsyncMessage
//
//    This is a utility function to allocate and populate a new AsyncWrite
//    structure and insert it (via addAsyncWrite() ) into the global list.
//
//    Note that for some messages data size has a different meaning, and
//    the data pointer is NULL, so we always have to check 'aData' for NULL.

int // static
AppendNewAsyncMessage(AsyncOsFile* aFile, PRUint32 aOp,
                                         sqlite_int64 aOffset, PRInt32 aDataSize,
                                         const char *aData)
{
  // allocate one buffer, we will put the buffer immediately after our struct
  AsyncMessage* p = NS_STATIC_CAST(AsyncMessage*,
      nsMemory::Alloc(sizeof(AsyncMessage) + (aData ? aDataSize : 0)));
  if (! p)
    return SQLITE_NOMEM;

  p->mOp = aOp;
  p->mOffset = aOffset;
  p->mBytes = aDataSize;
  p->mFile = aFile;
  p->mNext = nsnull;
  if (aData) {
    // this gets the address of the data immediately following our structure
    p->mBuf = (char*)&p[1];
    memcpy(p->mBuf, aData, aDataSize);
  } else {
    p->mBuf = nsnull;
  }
  AppendAsyncMessage(p);
  return SQLITE_OK;
}


// AsyncOpenExclusive
//
//    The async-IO backends implementation of the three functions used to open
//    a file (mOpenExclusive, mOpenReadWrite and mOpenReadOnly). Most of the
//    work is done in function AsyncOpenFile() - see above.
//
//    An OpenExclusive is only valid when the file does not exist.
//
//    OpenExclusive creates a new file structure with no reader and no writer.
//    It posts a message onto the thread for exclusive opening. When this
//    message is processed by the thread, it will create a mBaseReader opened
//    exclusively. Writing will still be OK because the thread will try to
//    use the reader structure for writing if no writer exists.
//
//    Until the file is actually opened, you can actually write to it because
//    the writes will be added to the queue. Reads will work because it will
//    skip reading from the file (no reader structure has been created) and
//    read from the write queue. Because OpenExclusive is not valid for
//    previously-existing files, we know anything in the file is in our
//    write queue until it is actually opened.

int // static
AsyncOpenExclusive(const char* aName, OsFile** aFile,
                                      int aDelFlag)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  // Create a new async file with no base reader that is not writable. Nothing
  // will be able to be done with this until the message is processed.
  AsyncOsFile* osfile;
  int rc = AsyncOpenFile(aName, &osfile, nsnull, PR_FALSE);
  if (rc != SQLITE_OK)
    return rc;

  rc = AppendNewAsyncMessage(osfile, ASYNC_OPENEXCLUSIVE, aDelFlag,
                             PL_strlen(aName) + 1, aName);
  if (rc != SQLITE_OK) {
    nsMemory::Free(osfile);
    osfile = nsnull;
  }
  *aFile = osfile;
  return rc;
}


// AsyncOpenReadOnly

int // static
AsyncOpenReadOnly(const char* aName, OsFile** aFile)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  OsFile* base = nsnull;
  int rc = sqliteOrigOpenReadOnly(aName, &base);
  if (rc == SQLITE_OK) {
    AsyncOsFile* asyncfile;
    rc = AsyncOpenFile(aName, &asyncfile, base, PR_FALSE);
    if (rc == SQLITE_OK)
      *aFile = asyncfile;
    else
      *aFile = nsnull;
  }
  return rc;
}


// AsyncOpenReadWrite

int // static
AsyncOpenReadWrite(const char *aName, OsFile** aFile,
                                      int* aReadOnly)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  OsFile* base = nsnull;
  int rc = sqliteOrigOpenReadWrite(aName, &base, aReadOnly);
  if (rc == SQLITE_OK) {
    AsyncOsFile* asyncfile;
    rc = AsyncOpenFile(aName, &asyncfile, base, !(*aReadOnly));
    if (rc == SQLITE_OK)
      *aFile = asyncfile;
    else
      *aFile = nsnull;
  }
  return rc;
}


// AsyncDelete
//
//    Implementation of sqlite3OsDelete. Add an entry to the end of the
//    write-op queue to perform the delete.

int // static
AsyncDelete(const char* aName)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  return AppendNewAsyncMessage(0, ASYNC_DELETE, 0, PL_strlen(aName) + 1, aName);
}


// AsyncSyncDirectory
//
//    Implementation of sqlite3OsSyncDirectory. Add an entry to the end of the
//    write-op queue to perform the directory sync.

int // static
AsyncSyncDirectory(const char* aName)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  return AppendNewAsyncMessage(0, ASYNC_SYNCDIRECTORY, 0, strlen(aName) + 1, aName);
}


// AsyncFileExists
//
//    Implementation of sqlite3OsFileExists. Return true if the file exists in
//    the file system.
//
//    This method is more complicated because the file may have been requested
//    to be deleted, then created, etc. This has to calculate the status of
//    the file at the end of the queue.
//
//    This method holds the mutex from start to finish because it has to check
//    the whole queue to see if the file has been created or deleted.

int // static
AsyncFileExists(const char *aName)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  nsAutoLock lock(AsyncQueueLock);

  // See if the real file system contains the specified file.
  int ret = sqliteOrigFileExists(aName);

  for (AsyncMessage* p = AsyncQueueFirst; p != nsnull; p = p->mNext) {
    if (p->mOp == ASYNC_DELETE && 0 == strcmp(p->mBuf, aName)) {
      ret = 0;
    } else if (p->mOp == ASYNC_OPENEXCLUSIVE && 0 == strcmp(p->mBuf, aName)) {
      ret = 1;
    }
  }
  return ret;
}


// AsyncClose
//
//    Close the file. This just adds an entry to the write-op list, the file is
//    not actually closed. We also note that the file has been closed by NULLing
//    out the file pointers on the file structure. Other functions will check
//    these to verify that the file hasn't been closed before they accept new
//    operations.

int // static
AsyncClose(OsFile** aFile)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, *aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }
  asyncfile->mOpen = PR_FALSE;
  return AppendNewAsyncMessage(asyncfile, ASYNC_CLOSE, 0, 0, 0);
}


// AsyncWrite
//
//    Implementation of sqlite3OsWrite() for asynchronous files. Instead of
//    writing to the underlying file, this function adds an entry to the end of
//    the global AsyncWrite list. Either SQLITE_OK or SQLITE_NOMEM may be
//    returned.

int // static
AsyncWrite(OsFile* aFile, const void* aBuf, int aCount)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }
  int rc = AppendNewAsyncMessage(asyncfile, ASYNC_WRITE, asyncfile->mOffset,
                                 aCount, NS_STATIC_CAST(const char*, aBuf));
  asyncfile->mOffset += aCount;
  return rc;
}


// AsyncTruncate
//
//    Truncate the file to nByte bytes in length. This just adds an entry to
//    the write-op list, no IO actually takes place.

int // static
AsyncTruncate(OsFile* aFile, sqlite_int64 aNumBytes)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }
  return AppendNewAsyncMessage(asyncfile, ASYNC_TRUNCATE, aNumBytes, 0, 0);
}


// AsyncOpenDirectory
//
//    Open the directory identified by zName and associate it with the
//    specified file. This just adds an entry to the write-op list, the
//    directory is opened later by sqlite3_async_flush().

int // static
AsyncOpenDirectory(OsFile* aFile, const char* aName)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }
  return AppendNewAsyncMessage(asyncfile, ASYNC_OPENDIRECTORY, 0,
                          strlen(aName) + 1, aName);
}


// AsyncSync
//
//    Sync the file. This just adds an entry to the write-op list, the sync()
//    is done later by sqlite3_async_flush().

int // static
AsyncSync(OsFile* aFile, int aFullsync)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }
  return AppendNewAsyncMessage(asyncfile, ASYNC_SYNC, 0, aFullsync, 0);
}


// AsyncSetFullSync
//
//    Set (or clear) the full-sync flag on the underlying file. This operation
//    is queued and performed later by sqlite3_async_flush().

void // static
AsyncSetFullSync(OsFile* aFile, int aValue)
{
  if (AsyncWriteError != SQLITE_OK)
    return;
  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return;
  }
  AppendNewAsyncMessage(asyncfile, ASYNC_SETFULLSYNC, 0, aValue, 0);
}


// AsyncRead
//
//    Read data from the file. First we read from the filesystem, then adjust
//    the contents of the buffer based on ASYNC_WRITE operations in the
//    write-op queue.
//
//    This method holds the mutex from start to finish because it has to
//    go through the whole queue and apply any changes to the file.

int // static
AsyncRead(OsFile* aFile, void *aBuffer, int aCount)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  int rc = SQLITE_OK;

  // Grab the write queue mutex for the duration of the call. We don't want
  // the writer thread going and writing stuff to the file or processing
  // any messages while we do this. Open exclusive may also change mBaseRead.
  nsAutoLock lock(AsyncQueueLock);

  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }

  OsFile* pBase = asyncfile->mBaseRead;
  if (pBase) {
    // Only do any actual file reading if there is a reader structure. For
    // pending OpenExclusives, there will not be any so we don't want to do
    // file reading. Reading while an OpenExclusive is pending will read
    // entirely from the write queue. Since OpenExclusive can not work for
    // prevously existing files, we know anything in the file is in our write
    // queue.
    sqlite_int64 filesize;
    NS_ASSERTION(sqliteOrigFileSize, "Original file size pointer uninitialized!");
    rc = sqliteOrigFileSize(pBase, &filesize);
    if (rc != SQLITE_OK)
      goto asyncread_out;

    // This may seek beyond EOF if there is appended data waiting in the write
    // buffer. The OS should be OK with this. We will only try reading if there
    // is stuff for us to read there.
    NS_ASSERTION(sqliteOrigSeek, "Original seek pointer uninitialized!");
    rc = sqliteOrigSeek(pBase, asyncfile->mOffset);
    if (rc != SQLITE_OK)
      goto asyncread_out;

    // Here, we try to read as much data as we want up to EOF.
    int numread = PR_MIN(filesize - asyncfile->mOffset, aCount);
    if (numread > 0) {
      NS_ASSERTION(pBase, "Original read pointer uninitialized!");
      rc = sqliteOrigRead(pBase, aBuffer, numread);
    }
  }

  if (rc == SQLITE_OK) {
    sqlite_int64 blockOffset = asyncfile->mOffset; // Current seek offset

    // Now we need to bring our data up-do-date with any pending writes.
    for (AsyncMessage* p = AsyncQueueFirst; p != nsnull; p = p->mNext) {
      if (p->mFile == asyncfile && p->mOp == ASYNC_WRITE) {

        // What we're reading:
        //
        //      [==================================================]
        //      ^- aFile.mOffset = blockOffset
        //      <--------------------aCount------------------------>
        //
        // Possibly pending writes:
        //
        // [==============]
        // ^- p.mOffset
        //      <---------> copycount
        //
        //               [================]
        //               ^- p.mOffset
        //               <----------------> copycount
        //
        //                                              [=============]
        //                                              ^- p.mOffset
        //                                              <----------> copycount
        PRInt32 beginIn = PR_MAX(0, blockOffset - p->mOffset);
        PRInt32 beginOut = PR_MAX(0, p->mOffset - blockOffset);
        PRInt32 copycount = PR_MIN(p->mBytes - beginIn, aCount - beginOut);

        if (copycount > 0) {
          memcpy(&NS_STATIC_CAST(char*, aBuffer)[beginOut],
                 &p->mBuf[beginIn], copycount);
        }
      }
    }

    // successful read, update virtual current seek offset
    asyncfile->mOffset += aCount;
  }

asyncread_out:
  return rc;
}


// AsyncSeek
//
//    Seek to the specified offset. This just adjusts the AsyncFile.iOffset
//    variable - calling seek() on the underlying file is defered until the
//    next read() or write() operation.

int // static
AsyncSeek(OsFile* aFile, sqlite_int64 aOffset)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }
  asyncfile->mOffset = aOffset;
  return SQLITE_OK;
}


// AsyncFileSize
//
//    Read the size of the file. First we read the size of the file system
//    entry, then adjust for any ASYNC_WRITE or ASYNC_TRUNCATE operations
//    currently in the write-op list.
//
//    This method holds the mutex from start to finish because it has to
//    grub through the whole queue.

int // static
AsyncFileSize(OsFile* aFile, sqlite_int64* aSize)
{
  nsAutoLock lock(AsyncQueueLock);

  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;

  AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  if (! asyncfile->mOpen) {
    NS_NOTREACHED("Attempting to write to a file with a close pending!");
    return SQLITE_INTERNAL;
  }
  int rc = SQLITE_OK;
  sqlite_int64 size = 0;

  // Read the filesystem size from the base file. If pBaseRead is NULL, this
  // means the file hasn't been opened yet. In this case all relevant data must
  // be in the write-op queue anyway, so we can omit reading from the
  // file-system.
  OsFile* pBase = asyncfile->mBaseRead;
  if (pBase) {
    NS_ASSERTION(sqliteOrigFileSize, "Original file size pointer uninitialized!");
    rc = sqliteOrigFileSize(pBase, &size);
  }

  if (rc == SQLITE_OK) {
    for (AsyncMessage* p = AsyncQueueFirst; p != nsnull; p = p->mNext) {
      if (p->mFile == asyncfile) {
        switch (p->mOp) {
          case ASYNC_WRITE:
            size = PR_MAX(p->mOffset + p->mBytes, size);
            break;
          case ASYNC_TRUNCATE:
            size = PR_MIN(size, p->mOffset);
            break;
        }
      }
    }
    *aSize = size;
  }
  return rc;
}


// AsyncFileHandle
//
//    Return the operating system file handle. This is only used for debugging
//    at the moment anyway. Using this filesystem handle outside of the async
//    service will make bad things happen!

int // static
AsyncFileHandle(OsFile* aFile)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  NS_NOTREACHED("Don't call FileHandle in async mode");
  return SQLITE_OK;

  // If you actually wanted the file handle you would do this:
  //AsyncOsFile* asyncfile = NS_STATIC_CAST(AsyncOsFile*, aFile);
  //return sqlite3OsFileHandle(asyncfile->mBaseRead);
}


// AsyncLock
//
//    No file locking occurs with this version of the asynchronous backend.
//    So the locking routines are no-ops.

int // static
AsyncLock(OsFile* aFile, int aLockType)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  return SQLITE_OK;
}


// AsnycUnlock

int // static
AsyncUnlock(OsFile* aFile, int aLockType)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  return SQLITE_OK;
}


// AsyncCheckReservedLock
//
//    This function is called when the pager layer first opens a database file
//    and is checking for a hot-journal.

int // static
AsyncCheckReservedLock(OsFile* aFile)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  return SQLITE_OK;
}


// AsyncLockState
//
//    This is broken in this async wrapper. But sqlite3OsLockState() is only
//    used for testing anyway.

int // static
AsyncLockState(OsFile* aFile)
{
  if (AsyncWriteError != SQLITE_OK)
    return AsyncWriteError;
  NS_NOTREACHED("Don't call LockState in async mode");
  return SQLITE_OK;
}


// ProcessOneMessage
//
//    When called, this thread is holding the mutex on the write-op queue.  In
//    the general case, we hold on to the mutex for the entire processing of
//    the message.
//
//    However in the potentially slower cases enumerated below, we relinquish
//    the mutex, perform the IO, and then re-request the mutex of the write-op
//    queue. The idea is to increase concurrency with sqlite threads.
//
//     * An ASYNC_CLOSE operation.
//     * An ASYNC_OPENEXCLUSIVE operation. For this one, we relinquish the
//       mutex, call the underlying xOpenExclusive() function, then
//       re-aquire the mutex before seting the AsyncFile.pBaseRead
//       variable.
//     * ASYNC_SYNC and ASYNC_WRITE operations, if
//       SQLITE_ASYNC_TWO_FILEHANDLES was set at compile time and two
//       file-handles are open for the particular file being "synced".

int // static
ProcessOneMessage(AsyncMessage* aMessage)
{
  PRBool regainMutex = PR_FALSE;
  OsFile* pBase = nsnull;

  if (aMessage->mFile) {
    pBase = aMessage->mFile->mBaseWrite;
    if (aMessage->mOp == ASYNC_CLOSE || 
        aMessage->mOp == ASYNC_OPENEXCLUSIVE ||
        (pBase && (aMessage->mOp == ASYNC_SYNC ||
                   aMessage->mOp == ASYNC_WRITE))) {
      regainMutex = PR_TRUE;
      PR_Unlock(AsyncQueueLock);
    }
    if (! pBase)
      pBase = aMessage->mFile->mBaseRead;
  }

  int rc = SQLITE_OK;
  switch (aMessage->mOp) {
    case ASYNC_WRITE:
      NS_ASSERTION(pBase, "Must have base writer for writing");
      rc = sqliteOrigSeek(pBase, aMessage->mOffset);
      if (rc == SQLITE_OK)
        rc = sqliteOrigWrite(pBase, (const void *)(aMessage->mBuf), aMessage->mBytes);
      break;

    case ASYNC_SYNC:
      NS_ASSERTION(pBase, "Must have base writer for writing");
      rc = sqliteOrigSync(pBase, aMessage->mBytes);
      break;

    case ASYNC_TRUNCATE:
      NS_ASSERTION(pBase, "Must have base writer for writing");
      NS_ASSERTION(sqliteOrigTruncate, "No truncate pointer");
      rc = sqliteOrigTruncate(pBase, aMessage->mOffset);
      break;

    case ASYNC_CLOSE:
      // note that the sqlite close function accepts NULL pointers here and
      // will return success if given one (I think the order we close these
      // two handles matters here)
      sqliteOrigClose(&aMessage->mFile->mBaseWrite);
      sqliteOrigClose(&aMessage->mFile->mBaseRead);
      nsMemory::Free(aMessage->mFile);
      break;

    case ASYNC_OPENDIRECTORY:
      NS_ASSERTION(pBase, "Must have base writer for writing");
      NS_ASSERTION(sqliteOrigOpenDirectory, "No open directory pointer");
      sqliteOrigOpenDirectory(pBase, aMessage->mBuf);
      break;

    case ASYNC_SETFULLSYNC:
      NS_ASSERTION(pBase, "Must have base writer for writing");
      sqliteOrigSetFullSync(pBase, aMessage->mBytes);
      break;

    case ASYNC_DELETE:
      NS_ASSERTION(sqliteOrigDelete, "No delete pointer");
      rc = sqliteOrigDelete(aMessage->mBuf);
      break;

    case ASYNC_SYNCDIRECTORY:
      NS_ASSERTION(sqliteOrigSyncDirectory, "No sync directory pointer");
      rc = sqliteOrigSyncDirectory(aMessage->mBuf);
      break;

    case ASYNC_OPENEXCLUSIVE: {
      AsyncOsFile *pFile = aMessage->mFile;
      int delFlag = ((aMessage->mOffset) ? 1 : 0);
      OsFile* pBase = nsnull;
      NS_ASSERTION(! pFile->mBaseRead && ! pFile->mBaseWrite,
                   "OpenExclusive expects no file pointers");
      rc = sqliteOrigOpenExclusive(aMessage->mBuf, &pBase, delFlag);

      // exclusive opens actually go and write to the OsFile structure to set
      // the file object. We therefore need to be locked so the main thread
      // doesn't try to use it to do synchronous reading.
      PR_Lock(AsyncQueueLock);
      regainMutex = PR_FALSE;
      if (rc == SQLITE_OK)
        pFile->mBaseRead = pBase;
      break;
    }

    default:
      NS_NOTREACHED("Illegal value for AsyncMessage.mOp");
  }

  if (regainMutex) {
    PR_Lock(AsyncQueueLock);
  }
  return rc;
}


// ProcessAsyncMessages
//
//    This procedure runs in a separate thread, reading messages off of the
//    write queue and processing them one by one.
//
//    If async.writerHaltNow is true, then this procedure exits after
//    processing a single message.
//
//    If async.writerHaltWhenIdle is true, then this procedure exits when the
//    write queue is empty.
//
//    If both of the above variables are false, this procedure runs
//    indefinitely, waiting for operations to be added to the write queue and
//    processing them in the order in which they arrive.
//
//    An artifical delay of async.ioDelay milliseconds is inserted before each
//    write operation in order to simulate the effect of a slow disk.
//
//    Only one instance of this procedure may be running at a time.

void // static
ProcessAsyncMessages()
{
  AsyncMessage *message = 0;
  int rc = SQLITE_OK;

  while (PR_TRUE) {
    {
      // wait for a message to come in
      nsAutoLock lock(AsyncQueueLock);
      while ((message = AsyncQueueFirst) == 0) {
        if (AsyncWriterHaltWhenIdle) {
          // We've been asked to stop, so exit the thread
          return;
        } else {
          // This will unlock AsyncQueueLock and wait for the condition to
          // be true. This condition is set when somebody adds an item to our
          // queue.
          NS_ASSERTION(AsyncQueueLock, "We need to be in multi threaded mode if we're going to wait");
          PR_WaitCondVar(AsyncQueueCondition, PR_INTERVAL_NO_TIMEOUT);
        }
      }

      // this function may release the lock in the middle, but should always
      // put it back when it's done
      rc = ProcessOneMessage(message);

      if (rc != SQLITE_OK) {
        AsyncWriteError = rc;
        NS_NOTREACHED("FILE ERROR");
        return;
      }

      // remove the message from the end of the message queue and release it
      if (message == AsyncQueueLast)
        AsyncQueueLast = nsnull;
      AsyncQueueFirst = message->mNext;
      nsMemory::Free(message);

      // free any out-of-memory flags in the library
      sqlite3ApiExit(nsnull, 0);
    }
    // Drop the queue mutex before continuing to the next write operation
    // in order to give other threads a chance to work with the write queue
    // (that should have been done by the autolock in exiting the scope that
    // just closed). We want writers to the queue to generally have priority.
    #ifdef IO_DELAY_INTERVAL_MS
      // this simulates slow disk
      PR_Sleep(PR_MillisecondsToInterval(IO_DELAY_INTERVAL_MS));
    #else
      // yield so the UI thread is more responsive
      PR_Sleep(PR_INTERVAL_NO_WAIT);
    #endif
  }
}
