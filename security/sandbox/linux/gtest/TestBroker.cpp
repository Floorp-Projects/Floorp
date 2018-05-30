/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "broker/SandboxBroker.h"
#include "broker/SandboxBrokerUtils.h"
#include "SandboxBrokerClient.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "mozilla/Atomics.h"
#include "mozilla/NullPtr.h"
#include "mozilla/PodOperations.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/FileDescriptor.h"

namespace mozilla {

static const int MAY_ACCESS = SandboxBroker::MAY_ACCESS;
static const int MAY_READ = SandboxBroker::MAY_READ;
static const int MAY_WRITE = SandboxBroker::MAY_WRITE;
static const int MAY_CREATE = SandboxBroker::MAY_CREATE;
static const auto AddAlways = SandboxBroker::Policy::AddAlways;

class SandboxBrokerTest : public ::testing::Test
{
  UniquePtr<SandboxBroker> mServer;
  UniquePtr<SandboxBrokerClient> mClient;

  UniquePtr<const SandboxBroker::Policy> GetPolicy() const;

  template<class C, void (C::* Main)()>
  static void* ThreadMain(void* arg) {
    (static_cast<C*>(arg)->*Main)();
    return nullptr;
  }

protected:
  int Open(const char* aPath, int aFlags) {
    return mClient->Open(aPath, aFlags);
  }
  int Access(const char* aPath, int aMode) {
    return mClient->Access(aPath, aMode);
  }
  int Stat(const char* aPath, statstruct* aStat) {
    return mClient->Stat(aPath, aStat);
  }
  int LStat(const char* aPath, statstruct* aStat) {
    return mClient->LStat(aPath, aStat);
  }
  int Chmod(const char* aPath, int aMode) {
    return mClient->Chmod(aPath, aMode);
  }
  int Link(const char* aPath, const char* bPath) {
    return mClient->Link(aPath, bPath);
  }
  int Mkdir(const char* aPath, int aMode) {
    return mClient->Mkdir(aPath, aMode);
  }
  int Symlink(const char* aPath, const char* bPath) {
    return mClient->Symlink(aPath, bPath);
  }
  int Rename(const char* aPath, const char* bPath) {
    return mClient->Rename(aPath, bPath);
  }
  int Rmdir(const char* aPath) {
    return mClient->Rmdir(aPath);
  }
  int Unlink(const char* aPath) {
    return mClient->Unlink(aPath);
  }
  ssize_t Readlink(const char* aPath, char* aBuff, size_t aSize) {
    return mClient->Readlink(aPath, aBuff, aSize);
  }

  void SetUp() override {
    ipc::FileDescriptor fd;

    mServer = SandboxBroker::Create(GetPolicy(), getpid(), fd);
    ASSERT_NE(mServer, nullptr);
    ASSERT_TRUE(fd.IsValid());
    auto rawFD = fd.ClonePlatformHandle();
    mClient.reset(new SandboxBrokerClient(rawFD.release()));
  }

  template<class C, void (C::* Main)()>
  void StartThread(pthread_t *aThread) {
    ASSERT_EQ(0, pthread_create(aThread, nullptr, ThreadMain<C, Main>,
                                static_cast<C*>(this)));
  }

  template<class C, void (C::* Main)()>
  void RunOnManyThreads() {
    static const int kNumThreads = 5;
    pthread_t threads[kNumThreads];
    for (pthread_t & thread : threads) {
      StartThread<C, Main>(&thread);
    }
    for (pthread_t thread : threads) {
      void* retval;
      ASSERT_EQ(pthread_join(thread, &retval), 0);
      ASSERT_EQ(retval, static_cast<void*>(nullptr));
    }
  }

public:
  void MultiThreadOpenWorker();
  void MultiThreadStatWorker();
};

UniquePtr<const SandboxBroker::Policy>
SandboxBrokerTest::GetPolicy() const
{
  UniquePtr<SandboxBroker::Policy> policy(new SandboxBroker::Policy());

  policy->AddPath(MAY_READ | MAY_WRITE, "/dev/null", AddAlways);
  policy->AddPath(MAY_READ, "/dev/zero", AddAlways);
  policy->AddPath(MAY_READ, "/var/empty/qwertyuiop", AddAlways);
  policy->AddPath(MAY_ACCESS, "/proc/self", AddAlways); // Warning: Linux-specific.
  policy->AddPath(MAY_READ | MAY_WRITE, "/tmp", AddAlways);
  policy->AddPath(MAY_READ | MAY_WRITE | MAY_CREATE, "/tmp/blublu", AddAlways);
  policy->AddPath(MAY_READ | MAY_WRITE | MAY_CREATE, "/tmp/blublublu", AddAlways);
  // This should be non-writable by the user running the test:
  policy->AddPath(MAY_READ | MAY_WRITE, "/etc", AddAlways);

  return std::move(policy);
}

TEST_F(SandboxBrokerTest, OpenForRead)
{
  int fd;

  fd = Open("/dev/null", O_RDONLY);
  ASSERT_GE(fd, 0) << "Opening /dev/null failed.";
  close(fd);
  fd = Open("/dev/zero", O_RDONLY);
  ASSERT_GE(fd, 0) << "Opening /dev/zero failed.";
  close(fd);
  fd = Open("/var/empty/qwertyuiop", O_RDONLY);
  EXPECT_EQ(-ENOENT, fd) << "Opening allowed but nonexistent file succeeded.";
  fd = Open("/proc/self", O_RDONLY);
  EXPECT_EQ(-EACCES, fd) << "Opening stat-only file for read succeeded.";
  fd = Open("/proc/self/stat", O_RDONLY);
  EXPECT_EQ(-EACCES, fd) << "Opening disallowed file succeeded.";
}

TEST_F(SandboxBrokerTest, OpenForWrite)
{
  int fd;

  fd = Open("/dev/null", O_WRONLY);
  ASSERT_GE(fd, 0) << "Opening /dev/null write-only failed.";
  close(fd);
  fd = Open("/dev/null", O_RDWR);
  ASSERT_GE(fd, 0) << "Opening /dev/null read/write failed.";
  close(fd);
  fd = Open("/dev/zero", O_WRONLY);
  ASSERT_EQ(-EACCES, fd) << "Opening read-only-by-policy file write-only succeeded.";
  fd = Open("/dev/zero", O_RDWR);
  ASSERT_EQ(-EACCES, fd) << "Opening read-only-by-policy file read/write succeeded.";
}

TEST_F(SandboxBrokerTest, SimpleRead)
{
  int fd;
  char c;

  fd = Open("/dev/null", O_RDONLY);
  ASSERT_GE(fd, 0);
  EXPECT_EQ(0, read(fd, &c, 1));
  close(fd);
  fd = Open("/dev/zero", O_RDONLY);
  ASSERT_GE(fd, 0);
  ASSERT_EQ(1, read(fd, &c, 1));
  EXPECT_EQ(c, '\0');
}

TEST_F(SandboxBrokerTest, Access)
{
  EXPECT_EQ(0, Access("/dev/null", F_OK));
  EXPECT_EQ(0, Access("/dev/null", R_OK));
  EXPECT_EQ(0, Access("/dev/null", W_OK));
  EXPECT_EQ(0, Access("/dev/null", R_OK|W_OK));
  EXPECT_EQ(-EACCES, Access("/dev/null", X_OK));
  EXPECT_EQ(-EACCES, Access("/dev/null", R_OK|X_OK));

  EXPECT_EQ(0, Access("/dev/zero", R_OK));
  EXPECT_EQ(-EACCES, Access("/dev/zero", W_OK));
  EXPECT_EQ(-EACCES, Access("/dev/zero", R_OK|W_OK));

  EXPECT_EQ(-ENOENT, Access("/var/empty/qwertyuiop", R_OK));
  EXPECT_EQ(-EACCES, Access("/var/empty/qwertyuiop", W_OK));

  EXPECT_EQ(0, Access("/proc/self", F_OK));
  EXPECT_EQ(-EACCES, Access("/proc/self", R_OK));

  EXPECT_EQ(-EACCES, Access("/proc/self/stat", F_OK));

  EXPECT_EQ(0, Access("/tmp", X_OK));
  EXPECT_EQ(0, Access("/tmp", R_OK|X_OK));
  EXPECT_EQ(0, Access("/tmp", R_OK|W_OK|X_OK));
  EXPECT_EQ(0, Access("/proc/self", X_OK));

  EXPECT_EQ(0, Access("/etc", R_OK|X_OK));
  EXPECT_EQ(-EACCES, Access("/etc", W_OK));
}

TEST_F(SandboxBrokerTest, Stat)
{
  statstruct realStat, brokeredStat;
  ASSERT_EQ(0, statsyscall("/dev/null", &realStat)) << "Shouldn't ever fail!";
  EXPECT_EQ(0, Stat("/dev/null", &brokeredStat));
  EXPECT_EQ(realStat.st_ino, brokeredStat.st_ino);
  EXPECT_EQ(realStat.st_rdev, brokeredStat.st_rdev);

  EXPECT_EQ(-ENOENT, Stat("/var/empty/qwertyuiop", &brokeredStat));
  EXPECT_EQ(-EACCES, Stat("/dev", &brokeredStat));

  EXPECT_EQ(0, Stat("/proc/self", &brokeredStat));
  EXPECT_TRUE(S_ISDIR(brokeredStat.st_mode));
}

TEST_F(SandboxBrokerTest, LStat)
{
  statstruct realStat, brokeredStat;
  ASSERT_EQ(0, lstatsyscall("/dev/null", &realStat));
  EXPECT_EQ(0, LStat("/dev/null", &brokeredStat));
  EXPECT_EQ(realStat.st_ino, brokeredStat.st_ino);
  EXPECT_EQ(realStat.st_rdev, brokeredStat.st_rdev);

  EXPECT_EQ(-ENOENT, LStat("/var/empty/qwertyuiop", &brokeredStat));
  EXPECT_EQ(-EACCES, LStat("/dev", &brokeredStat));

  EXPECT_EQ(0, LStat("/proc/self", &brokeredStat));
  EXPECT_TRUE(S_ISLNK(brokeredStat.st_mode));
}

static void PrePostTestCleanup(void)
{
  unlink("/tmp/blublu");
  rmdir("/tmp/blublu");
  unlink("/tmp/nope");
  rmdir("/tmp/nope");
  unlink("/tmp/blublublu");
  rmdir("/tmp/blublublu");
}

TEST_F(SandboxBrokerTest, Chmod)
{
  PrePostTestCleanup();

  int fd = Open("/tmp/blublu", O_WRONLY | O_CREAT);
  ASSERT_GE(fd, 0) << "Opening /tmp/blublu for writing failed.";
  close(fd);
  // Set read only. SandboxBroker enforces 0600 mode flags.
  ASSERT_EQ(0, Chmod("/tmp/blublu", S_IRUSR));
  EXPECT_EQ(-EACCES, Access("/tmp/blublu", W_OK));
  statstruct realStat;
  EXPECT_EQ(0, statsyscall("/tmp/blublu", &realStat));
  EXPECT_EQ((mode_t)S_IRUSR, realStat.st_mode & 0777);

  ASSERT_EQ(0, Chmod("/tmp/blublu", S_IRUSR | S_IWUSR));
  EXPECT_EQ(0, statsyscall("/tmp/blublu", &realStat));
  EXPECT_EQ((mode_t)(S_IRUSR | S_IWUSR), realStat.st_mode & 0777);
  EXPECT_EQ(0, unlink("/tmp/blublu"));

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, Link)
{
  PrePostTestCleanup();

  int fd = Open("/tmp/blublu", O_WRONLY | O_CREAT);
  ASSERT_GE(fd, 0) << "Opening /tmp/blublu for writing failed.";
  close(fd);
  ASSERT_EQ(0, Link("/tmp/blublu", "/tmp/blublublu"));
  EXPECT_EQ(0, Access("/tmp/blublublu", F_OK));
  // Not whitelisted target path
  EXPECT_EQ(-EACCES, Link("/tmp/blublu", "/tmp/nope"));
  EXPECT_EQ(0, unlink("/tmp/blublublu"));
  EXPECT_EQ(0, unlink("/tmp/blublu"));

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, Symlink)
{
  PrePostTestCleanup();

  int fd = Open("/tmp/blublu", O_WRONLY | O_CREAT);
  ASSERT_GE(fd, 0) << "Opening /tmp/blublu for writing failed.";
  close(fd);
  ASSERT_EQ(0, Symlink("/tmp/blublu", "/tmp/blublublu"));
  EXPECT_EQ(0, Access("/tmp/blublublu", F_OK));
  statstruct aStat;
  ASSERT_EQ(0, lstatsyscall("/tmp/blublublu", &aStat));
  EXPECT_EQ((mode_t)S_IFLNK, aStat.st_mode & S_IFMT);
  // Not whitelisted target path
  EXPECT_EQ(-EACCES, Symlink("/tmp/blublu", "/tmp/nope"));
  EXPECT_EQ(0, unlink("/tmp/blublublu"));
  EXPECT_EQ(0, unlink("/tmp/blublu"));

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, Mkdir)
{
  PrePostTestCleanup();

  ASSERT_EQ(0, mkdir("/tmp/blublu", 0600))
    << "Creating dir /tmp/blublu failed.";
  EXPECT_EQ(0, Access("/tmp/blublu", F_OK));
  // Not whitelisted target path
  EXPECT_EQ(-EACCES, Mkdir("/tmp/nope", 0600))
    << "Creating dir without MAY_CREATE succeed.";
  EXPECT_EQ(0, rmdir("/tmp/blublu"));
  EXPECT_EQ(-EEXIST, Mkdir("/proc/self", 0600))
    << "Creating uncreatable dir that already exists didn't fail correctly.";
  EXPECT_EQ(-EEXIST, Mkdir("/dev/zero", 0600))
    << "Creating uncreatable dir over preexisting file didn't fail correctly.";

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, Rename)
{
  PrePostTestCleanup();

  ASSERT_EQ(0, mkdir("/tmp/blublu", 0600))
    << "Creating dir /tmp/blublu failed.";
  EXPECT_EQ(0, Access("/tmp/blublu", F_OK));
  ASSERT_EQ(0, Rename("/tmp/blublu", "/tmp/blublublu"));
  EXPECT_EQ(0, Access("/tmp/blublublu", F_OK));
  EXPECT_EQ(-ENOENT , Access("/tmp/blublu", F_OK));
  // Not whitelisted target path
  EXPECT_EQ(-EACCES, Rename("/tmp/blublublu", "/tmp/nope"))
    << "Renaming dir without write access succeed.";
  EXPECT_EQ(0, rmdir("/tmp/blublublu"));

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, Rmdir)
{
  PrePostTestCleanup();

  ASSERT_EQ(0, mkdir("/tmp/blublu", 0600))
    << "Creating dir /tmp/blublu failed.";
  EXPECT_EQ(0, Access("/tmp/blublu", F_OK));
  ASSERT_EQ(0, Rmdir("/tmp/blublu"));
  EXPECT_EQ(-ENOENT, Access("/tmp/blublu", F_OK));
  // Bypass sandbox to create a non-deletable dir
  ASSERT_EQ(0, mkdir("/tmp/nope", 0600));
  EXPECT_EQ(-EACCES, Rmdir("/tmp/nope"));

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, Unlink)
{
  PrePostTestCleanup();

  int fd = Open("/tmp/blublu", O_WRONLY | O_CREAT);
  ASSERT_GE(fd, 0) << "Opening /tmp/blublu for writing failed.";
  close(fd);
  EXPECT_EQ(0, Access("/tmp/blublu", F_OK));
  EXPECT_EQ(0, Unlink("/tmp/blublu"));
  EXPECT_EQ(-ENOENT , Access("/tmp/blublu", F_OK));
  // Bypass sandbox to write a non-deletable file
  fd = open("/tmp/nope", O_WRONLY | O_CREAT, 0600);
  ASSERT_GE(fd, 0) << "Opening /tmp/nope for writing failed.";
  close(fd);
  EXPECT_EQ(-EACCES, Unlink("/tmp/nope"));

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, Readlink)
{
  PrePostTestCleanup();

  int fd = Open("/tmp/blublu", O_WRONLY | O_CREAT);
  ASSERT_GE(fd, 0) << "Opening /tmp/blublu for writing failed.";
  close(fd);
  ASSERT_EQ(0, Symlink("/tmp/blublu", "/tmp/blublublu"));
  EXPECT_EQ(0, Access("/tmp/blublublu", F_OK));
  char linkBuff[256];
  EXPECT_EQ(11, Readlink("/tmp/blublublu", linkBuff, sizeof(linkBuff)));
  linkBuff[11] = '\0';
  EXPECT_EQ(0, strcmp(linkBuff, "/tmp/blublu"));

  PrePostTestCleanup();
}

TEST_F(SandboxBrokerTest, MultiThreadOpen) {
  RunOnManyThreads<SandboxBrokerTest,
                   &SandboxBrokerTest::MultiThreadOpenWorker>();
}
void SandboxBrokerTest::MultiThreadOpenWorker() {
  static const int kNumLoops = 10000;

  for (int i = 1; i <= kNumLoops; ++i) {
    int nullfd = Open("/dev/null", O_RDONLY);
    int zerofd = Open("/dev/zero", O_RDONLY);
    ASSERT_GE(nullfd, 0) << "Loop " << i << "/" << kNumLoops;
    ASSERT_GE(zerofd, 0) << "Loop " << i << "/" << kNumLoops;
    char c;
    ASSERT_EQ(0, read(nullfd, &c, 1)) << "Loop " << i << "/" << kNumLoops;
    ASSERT_EQ(1, read(zerofd, &c, 1)) << "Loop " << i << "/" << kNumLoops;
    ASSERT_EQ('\0', c) << "Loop " << i << "/" << kNumLoops;
    close(nullfd);
    close(zerofd);
  }
}

TEST_F(SandboxBrokerTest, MultiThreadStat) {
  RunOnManyThreads<SandboxBrokerTest,
                   &SandboxBrokerTest::MultiThreadStatWorker>();
}
void SandboxBrokerTest::MultiThreadStatWorker() {
  static const int kNumLoops = 7500;
  statstruct nullStat, zeroStat, selfStat;
  dev_t realNullDev, realZeroDev;
  ino_t realSelfInode;

  ASSERT_EQ(0, statsyscall("/dev/null", &nullStat)) << "Shouldn't ever fail!";
  ASSERT_EQ(0, statsyscall("/dev/zero", &zeroStat)) << "Shouldn't ever fail!";
  ASSERT_EQ(0, lstatsyscall("/proc/self", &selfStat)) << "Shouldn't ever fail!";
  ASSERT_TRUE(S_ISLNK(selfStat.st_mode)) << "Shouldn't ever fail!";
  realNullDev = nullStat.st_rdev;
  realZeroDev = zeroStat.st_rdev;
  realSelfInode = selfStat.st_ino;
  for (int i = 1; i <= kNumLoops; ++i) {
    ASSERT_EQ(0, Stat("/dev/null", &nullStat))
      << "Loop " << i << "/" << kNumLoops;
    ASSERT_EQ(0, Stat("/dev/zero", &zeroStat))
      << "Loop " << i << "/" << kNumLoops;
    ASSERT_EQ(0, LStat("/proc/self", &selfStat))
      << "Loop " << i << "/" << kNumLoops;

    ASSERT_EQ(realNullDev, nullStat.st_rdev)
      << "Loop " << i << "/" << kNumLoops;
    ASSERT_EQ(realZeroDev, zeroStat.st_rdev)
      << "Loop " << i << "/" << kNumLoops;
    ASSERT_TRUE(S_ISLNK(selfStat.st_mode))
      << "Loop " << i << "/" << kNumLoops;
    ASSERT_EQ(realSelfInode, selfStat.st_ino)
      << "Loop " << i << "/" << kNumLoops;
  }
}

#if 0
class SandboxBrokerSigStress : public SandboxBrokerTest
{
  int mSigNum;
  struct sigaction mOldAction;
  Atomic<void*> mVoidPtr;

  static void SigHandler(int aSigNum, siginfo_t* aSigInfo, void *aCtx) {
    ASSERT_EQ(SI_QUEUE, aSigInfo->si_code);
    SandboxBrokerSigStress* that =
      static_cast<SandboxBrokerSigStress*>(aSigInfo->si_value.sival_ptr);
    ASSERT_EQ(that->mSigNum, aSigNum);
    that->DoSomething();
  }

protected:
  Atomic<int> mTestIter;
  sem_t mSemaphore;

  void SignalThread(pthread_t aThread) {
    union sigval sv;
    sv.sival_ptr = this;
    ASSERT_NE(0, mSigNum);
    ASSERT_EQ(0, pthread_sigqueue(aThread, mSigNum, sv));
  }

  virtual void SetUp() {
    ASSERT_EQ(0, sem_init(&mSemaphore, 0, 0));
    mVoidPtr = nullptr;
    mSigNum = 0;
    for (int sigNum = SIGRTMIN; sigNum < SIGRTMAX; ++sigNum) {
      ASSERT_EQ(0, sigaction(sigNum, nullptr, &mOldAction));
      if ((mOldAction.sa_flags & SA_SIGINFO) == 0 &&
          mOldAction.sa_handler == SIG_DFL) {
        struct sigaction newAction;
        PodZero(&newAction);
        newAction.sa_flags = SA_SIGINFO;
        newAction.sa_sigaction = SigHandler;
        ASSERT_EQ(0, sigaction(sigNum, &newAction, nullptr));
        mSigNum = sigNum;
        break;
      }
    }
    ASSERT_NE(mSigNum, 0);

    SandboxBrokerTest::SetUp();
  }

  virtual void TearDown() {
    ASSERT_EQ(0, sem_destroy(&mSemaphore));
    if (mSigNum != 0) {
      ASSERT_EQ(0, sigaction(mSigNum, &mOldAction, nullptr));
    }
    if (mVoidPtr) {
      free(mVoidPtr);
    }
  }

  void DoSomething();

public:
  void MallocWorker();
  void FreeWorker();
};

TEST_F(SandboxBrokerSigStress, StressTest)
{
  static const int kIters = 6250;
  static const int kNsecPerIterPerIter = 4;
  struct timespec delay = { 0, 0 };
  pthread_t threads[2];

  mTestIter = kIters;

  StartThread<SandboxBrokerSigStress,
              &SandboxBrokerSigStress::MallocWorker>(&threads[0]);
  StartThread<SandboxBrokerSigStress,
              &SandboxBrokerSigStress::FreeWorker>(&threads[1]);

  for (int i = kIters; i > 0; --i) {
    SignalThread(threads[i % 2]);
    while (sem_wait(&mSemaphore) == -1 && errno == EINTR)
      /* retry */;
    ASSERT_EQ(i - 1, mTestIter);
    delay.tv_nsec += kNsecPerIterPerIter;
    struct timespec req = delay, rem;
    while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
      req = rem;
    }
  }
  void *retval;
  ASSERT_EQ(0, pthread_join(threads[0], &retval));
  ASSERT_EQ(nullptr, retval);
  ASSERT_EQ(0, pthread_join(threads[1], &retval));
  ASSERT_EQ(nullptr, retval);
}

void
SandboxBrokerSigStress::MallocWorker()
{
  static const size_t kSize = 64;

  void* mem = malloc(kSize);
  while (mTestIter > 0) {
    ASSERT_NE(mem, mVoidPtr);
    mem = mVoidPtr.exchange(mem);
    if (mem) {
      sched_yield();
    } else {
      mem = malloc(kSize);
    }
  }
  if (mem) {
    free(mem);
  }
}

void
SandboxBrokerSigStress::FreeWorker()
{
  void *mem = nullptr;
  while (mTestIter > 0) {
    mem = mVoidPtr.exchange(mem);
    if (mem) {
      free(mem);
      mem = nullptr;
    } else {
      sched_yield();
    }
  }
}

void
SandboxBrokerSigStress::DoSomething()
{
  int fd;
  char c;
  struct stat st;

  //fprintf(stderr, "Don't try this at home: %d\n", static_cast<int>(mTestIter));
  switch (mTestIter % 5) {
  case 0:
    fd = Open("/dev/null", O_RDWR);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(0, read(fd, &c, 1));
    close(fd);
    break;
  case 1:
    fd = Open("/dev/zero", O_RDONLY);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(1, read(fd, &c, 1));
    ASSERT_EQ('\0', c);
    close(fd);
    break;
  case 2:
    ASSERT_EQ(0, Access("/dev/null", W_OK));
    break;
  case 3:
    ASSERT_EQ(0, Stat("/proc/self", &st));
    ASSERT_TRUE(S_ISDIR(st.st_mode));
    break;
  case 4:
    ASSERT_EQ(0, LStat("/proc/self", &st));
    ASSERT_TRUE(S_ISLNK(st.st_mode));
    break;
  }
  mTestIter--;
  sem_post(&mSemaphore);
}
#endif

} // namespace mozilla
