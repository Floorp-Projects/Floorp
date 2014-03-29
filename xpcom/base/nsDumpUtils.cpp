/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDumpUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "prenv.h"
#include <errno.h>
#include "mozilla/Services.h"
#include "nsIObserverService.h"
 #include "mozilla/ClearOnShutdown.h"

#if defined(XP_LINUX) || defined(__FreeBSD__) // {
#include "mozilla/Preferences.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * The following code supports triggering a registered callback upon
 * receiving a specific signal.
 *
 * Take about:memory for example, we register
 * 1. doGCCCDump for doMemoryReport
 * 2. doMemoryReport for sDumpAboutMemorySignum(SIGRTMIN)
 *                       and sDumpAboutMemoryAfterMMUSignum(SIGRTMIN+1).
 *
 * When we receive one of these signals, we write the signal number to a pipe.
 * The IO thread then notices that the pipe has been written to, and kicks off
 * the appropriate task on the main thread.
 *
 * This scheme is similar to using signalfd(), except it's portable and it
 * doesn't require the use of sigprocmask, which is problematic because it
 * masks signals received by child processes.
 *
 * In theory, we could use Chromium's MessageLoopForIO::CatchSignal() for this.
 * But that uses libevent, which does not handle the realtime signals (bug
 * 794074).
 */

// This is the write-end of a pipe that we use to notice when a
// specific signal occurs.
static Atomic<int> sDumpPipeWriteFd(-1);

static void
DumpSignalHandler(int aSignum)
{
  // This is a signal handler, so everything in here needs to be
  // async-signal-safe.  Be careful!

  if (sDumpPipeWriteFd != -1) {
    uint8_t signum = static_cast<int>(aSignum);
    write(sDumpPipeWriteFd, &signum, sizeof(signum));
  }
}

NS_IMPL_ISUPPORTS1(FdWatcher, nsIObserver);

void FdWatcher::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  os->AddObserver(this, "xpcom-shutdown", /* ownsWeak = */ false);

  XRE_GetIOMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &FdWatcher::StartWatching));
}

// Implementations may call this function multiple times if they ensure that
// it's safe to call OpenFd() multiple times and they call StopWatching()
// first.
void FdWatcher::StartWatching()
{
  MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());
  MOZ_ASSERT(mFd == -1);

  mFd = OpenFd();
  if (mFd == -1) {
    LOG("FdWatcher: OpenFd failed.");
    return;
  }

  MessageLoopForIO::current()->WatchFileDescriptor(
    mFd, /* persistent = */ true,
    MessageLoopForIO::WATCH_READ,
    &mReadWatcher, this);
}

// Since implementations can call StartWatching() multiple times, they can of
// course call StopWatching() multiple times.
void FdWatcher::StopWatching()
{
  MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

  mReadWatcher.StopWatchingFileDescriptor();
  if (mFd != -1) {
    close(mFd);
    mFd = -1;
  }
}

StaticRefPtr<SignalPipeWatcher> SignalPipeWatcher::sSingleton;

/* static */ SignalPipeWatcher*
SignalPipeWatcher::GetSingleton()
{
  if (!sSingleton) {
    sSingleton = new SignalPipeWatcher();
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }
  return sSingleton;
}

/* static */ void
SignalPipeWatcher::RegisterCallback(const uint8_t aSignal,
                                    PipeCallback aCallback)
{
  for (SignalInfoArray::index_type i = 0; 
       i < SignalPipeWatcher::mSignalInfo.Length(); i++)
  {
    if (SignalPipeWatcher::mSignalInfo[i].mSignal == aSignal) {
      LOG("Register Signal(%d) callback failed! (DUPLICATE)", aSignal);
      return;
    }
  }
  SignalInfo aSignalInfo = { aSignal, aCallback };
  SignalPipeWatcher::mSignalInfo.AppendElement(aSignalInfo);
  SignalPipeWatcher::RegisterSignalHandler(aSignalInfo.mSignal);
}

/* static */ void
SignalPipeWatcher::RegisterSignalHandler(const uint8_t aSignal)
{
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = DumpSignalHandler;

  if (aSignal) {
    if (sigaction(aSignal, &action, nullptr)) {
      LOG("SignalPipeWatcher failed to register sig %d.", aSignal);
    }
  } else {
    for (SignalInfoArray::index_type i = 0; i < SignalPipeWatcher::mSignalInfo.Length(); i++) {
      if (sigaction(SignalPipeWatcher::mSignalInfo[i].mSignal, &action, nullptr)) {
        LOG("SignalPipeWatcher failed to register signal(%d) "
            "dump signal handler.",SignalPipeWatcher::mSignalInfo[i].mSignal);
      }
    }
  }
}

SignalPipeWatcher::~SignalPipeWatcher()
{
  if (sDumpPipeWriteFd != -1)
    SignalPipeWatcher::StopWatching();
}

int SignalPipeWatcher::OpenFd()
{
  MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

  // Create a pipe.  When we receive a signal in our signal handler, we'll
  // write the signum to the write-end of this pipe.
  int pipeFds[2];
  if (pipe(pipeFds)) {
    LOG("SignalPipeWatcher failed to create pipe.");
    return -1;
  }

  // Close this pipe on calls to exec().
  fcntl(pipeFds[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipeFds[1], F_SETFD, FD_CLOEXEC);

  int readFd = pipeFds[0];
  sDumpPipeWriteFd = pipeFds[1];

  SignalPipeWatcher::RegisterSignalHandler();
  return readFd;
}

void SignalPipeWatcher::StopWatching()
{
  MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

  // Close sDumpPipeWriteFd /after/ setting the fd to -1.
  // Otherwise we have the (admittedly far-fetched) race where we
  //
  //  1) close sDumpPipeWriteFd
  //  2) open a new fd with the same number as sDumpPipeWriteFd
  //     had.
  //  3) receive a signal, then write to the fd.
  int pipeWriteFd = sDumpPipeWriteFd.exchange(-1);
  close(pipeWriteFd);

  FdWatcher::StopWatching();
}

void SignalPipeWatcher::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

  uint8_t signum;
  ssize_t numReceived = read(aFd, &signum, sizeof(signum));
  if (numReceived != sizeof(signum)) {
    LOG("Error reading from buffer in "
        "SignalPipeWatcher::OnFileCanReadWithoutBlocking.");
    return;
  }

  for (SignalInfoArray::index_type i = 0; i < SignalPipeWatcher::mSignalInfo.Length(); i++) {
    if(signum == SignalPipeWatcher::mSignalInfo[i].mSignal) {
      SignalPipeWatcher::mSignalInfo[i].mCallback(signum);
      return;
    }
  }
  LOG("SignalPipeWatcher got unexpected signum.");
}

StaticRefPtr<FifoWatcher> FifoWatcher::sSingleton;

/* static */ FifoWatcher*
FifoWatcher::GetSingleton()
{
  if (!sSingleton) {
    nsAutoCString dirPath;
    Preferences::GetCString(
      "memory_info_dumper.watch_fifo.directory", &dirPath);
    sSingleton = new FifoWatcher(dirPath);
    sSingleton->Init();
    ClearOnShutdown(&sSingleton);
  }
  return sSingleton;
}

/* static */ bool
FifoWatcher::MaybeCreate()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    // We want this to be main-process only, since two processes can't listen
    // to the same fifo.
    return false;
  }

  if (!Preferences::GetBool("memory_info_dumper.watch_fifo.enabled", false)) {
    LOG("Fifo watcher disabled via pref.");
    return false;
  }

  // The FifoWatcher is held alive by the observer service.
  if (!FifoWatcher::sSingleton) {
    FifoWatcher::GetSingleton();
  }
  return true;
}

void
FifoWatcher::RegisterCallback(const nsCString& aCommand,FifoCallback aCallback)
{
  for (FifoInfoArray::index_type i = 0;
       i < FifoWatcher::mFifoInfo.Length(); i++)
  {
    if (FifoWatcher::mFifoInfo[i].mCommand.Equals(aCommand)) {
      LOG("Register command(%s) callback failed! (DUPLICATE)", aCommand.get());
      return;
    }
  }
  FifoInfo aFifoInfo = { aCommand, aCallback };
  FifoWatcher::mFifoInfo.AppendElement(aFifoInfo);
}

FifoWatcher::~FifoWatcher()
{
}

int FifoWatcher::OpenFd()
{
  // If the memory_info_dumper.directory pref is specified, put the fifo
  // there.  Otherwise, put it into the system's tmp directory.

  nsCOMPtr<nsIFile> file;

  nsresult rv;
  if (mDirPath.Length() > 0) {
    rv = XRE_GetFileFromPath(mDirPath.get(), getter_AddRefs(file));
    if (NS_FAILED(rv)) {
      LOG("FifoWatcher failed to open file \"%s\"", mDirPath.get());
      return -1;
    }
  } else {
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(file));
    if (NS_WARN_IF(NS_FAILED(rv)))
      return -1;
  }

  rv = file->AppendNative(NS_LITERAL_CSTRING("debug_info_trigger"));
  if (NS_WARN_IF(NS_FAILED(rv)))
    return -1;

  nsAutoCString path;
  rv = file->GetNativePath(path);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return -1;

  // unlink might fail because the file doesn't exist, or for other reasons.
  // But we don't care it fails; any problems will be detected later, when we
  // try to mkfifo or open the file.
  if (unlink(path.get())) {
    LOG("FifoWatcher::OpenFifo unlink failed; errno=%d.  "
        "Continuing despite error.", errno);
  }

  if (mkfifo(path.get(), 0766)) {
    LOG("FifoWatcher::OpenFifo mkfifo failed; errno=%d", errno);
    return -1;
  }

#ifdef ANDROID
    // Android runs with a umask, so we need to chmod our fifo to make it
    // world-writable.
    chmod(path.get(), 0666);
#endif

  int fd;
  do {
    // The fifo will block until someone else has written to it.  In
    // particular, open() will block until someone else has opened it for
    // writing!  We want open() to succeed and read() to block, so we open
    // with NONBLOCK and then fcntl that away.
    fd = open(path.get(), O_RDONLY | O_NONBLOCK);
  } while (fd == -1 && errno == EINTR);

  if (fd == -1) {
    LOG("FifoWatcher::OpenFifo open failed; errno=%d", errno);
    return -1;
  }

  // Make fd blocking now that we've opened it.
  if (fcntl(fd, F_SETFL, 0)) {
    close(fd);
    return -1;
  }

  return fd;
}

void FifoWatcher::OnFileCanReadWithoutBlocking(int aFd)
{
  MOZ_ASSERT(XRE_GetIOMessageLoop() == MessageLoopForIO::current());

  char buf[1024];
  int nread;
  do {
    // sizeof(buf) - 1 to leave space for the null-terminator.
    nread = read(aFd, buf, sizeof(buf));
  } while(nread == -1 && errno == EINTR);

  if (nread == -1) {
    // We want to avoid getting into a situation where
    // OnFileCanReadWithoutBlocking is called in an infinite loop, so when
    // something goes wrong, stop watching the fifo altogether.
    LOG("FifoWatcher hit an error (%d) and is quitting.", errno);
    StopWatching();
    return;
  }

  if (nread == 0) {
    // If we get EOF, that means that the other side closed the fifo.  We need
    // to close and re-open the fifo; if we don't,
    // OnFileCanWriteWithoutBlocking will be called in an infinite loop.

    LOG("FifoWatcher closing and re-opening fifo.");
    StopWatching();
    StartWatching();
    return;
  }

  nsAutoCString inputStr;
  inputStr.Append(buf, nread);

  // Trimming whitespace is important because if you do
  //   |echo "foo" >> debug_info_trigger|,
  // it'll actually write "foo\n" to the fifo.
  inputStr.Trim("\b\t\r\n");

  for (FifoInfoArray::index_type i = 0; i < FifoWatcher::mFifoInfo.Length(); i++) {
    const nsCString commandStr = FifoWatcher::mFifoInfo[i].mCommand;
    if(inputStr == commandStr.get()) {
      FifoWatcher::mFifoInfo[i].mCallback(inputStr);
      return;
    }
  }
  LOG("Got unexpected value from fifo; ignoring it.");
}

#endif // XP_LINUX }

// In Android case, this function will open a file named aFilename under
// /data/local/tmp/"aFoldername".
// Otherwise, it will open a file named aFilename under "NS_OS_TEMP_DIR".
/* static */ nsresult
nsDumpUtils::OpenTempFile(const nsACString& aFilename, nsIFile** aFile,
                          const nsACString& aFoldername)
{
#ifdef ANDROID
  // For Android, first try the downloads directory which is world-readable
  // rather than the temp directory which is not.
  if (!*aFile) {
    char *env = PR_GetEnv("DOWNLOADS_DIRECTORY");
    if (env) {
      NS_NewNativeLocalFile(nsCString(env), /* followLinks = */ true, aFile);
    }
  }
#endif
  nsresult rv;
  if (!*aFile) {
    rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, aFile);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;
  }

#ifdef ANDROID
  // /data/local/tmp is a true tmp directory; anyone can create a file there,
  // but only the user which created the file can remove it.  We want non-root
  // users to be able to remove these files, so we write them into a
  // subdirectory of the temp directory and chmod 777 that directory.
  if (aFoldername != EmptyCString()) {
    rv = (*aFile)->AppendNative(aFoldername);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    // It's OK if this fails; that probably just means that the directory already
    // exists.
    (*aFile)->Create(nsIFile::DIRECTORY_TYPE, 0777);

    nsAutoCString dirPath;
    rv = (*aFile)->GetNativePath(dirPath);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    while (chmod(dirPath.get(), 0777) == -1 && errno == EINTR) {}
  }
#endif

  nsCOMPtr<nsIFile> file(*aFile);

  rv = file->AppendNative(aFilename);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

  rv = file->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0666);
  if (NS_WARN_IF(NS_FAILED(rv)))
    return rv;

#ifdef ANDROID
    // Make this file world-read/writable; the permissions passed to the
    // CreateUnique call above are not sufficient on Android, which runs with a
    // umask.
    nsAutoCString path;
    rv = file->GetNativePath(path);
    if (NS_WARN_IF(NS_FAILED(rv)))
      return rv;

    while (chmod(path.get(), 0666) == -1 && errno == EINTR) {}
#endif

  return NS_OK;
}
