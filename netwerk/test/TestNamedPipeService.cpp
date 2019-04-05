/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "gtest/gtest.h"

#include <windows.h>

#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "nsNamedPipeService.h"
#include "nsNetCID.h"

#define PIPE_NAME "\\\\.\\pipe\\TestNPS"
#define TEST_STR "Hello World"

using namespace mozilla;

/**
 * Unlike a monitor, an event allows a thread to wait on another thread
 * completing an action without regard to ordering of the wait and the notify.
 */
class Event {
 public:
  explicit Event(const char* aName) : mMonitor(aName) {}

  ~Event() = default;

  void Set() {
    MonitorAutoLock lock(mMonitor);
    MOZ_ASSERT(!mSignaled);
    mSignaled = true;
    mMonitor.Notify();
  }
  void Wait() {
    MonitorAutoLock lock(mMonitor);
    while (!mSignaled) {
      lock.Wait();
    }
    mSignaled = false;
  }

 private:
  Monitor mMonitor;
  bool mSignaled = false;
};

class nsNamedPipeDataObserver final : public nsINamedPipeDataObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINAMEDPIPEDATAOBSERVER

  explicit nsNamedPipeDataObserver(HANDLE aPipe);

  int Read(void* aBuffer, uint32_t aSize);
  int Write(const void* aBuffer, uint32_t aSize);

  uint32_t Transferred() const { return mBytesTransferred; }

 private:
  ~nsNamedPipeDataObserver() = default;

  HANDLE mPipe;
  OVERLAPPED mOverlapped;
  Atomic<uint32_t> mBytesTransferred;
  Event mEvent;
};

NS_IMPL_ISUPPORTS(nsNamedPipeDataObserver, nsINamedPipeDataObserver)

nsNamedPipeDataObserver::nsNamedPipeDataObserver(HANDLE aPipe)
    : mPipe(aPipe), mOverlapped(), mBytesTransferred(0), mEvent("named-pipe") {
  mOverlapped.hEvent = CreateEventA(nullptr, TRUE, TRUE, "named-pipe");
}

int nsNamedPipeDataObserver::Read(void* aBuffer, uint32_t aSize) {
  DWORD bytesRead = 0;
  if (!ReadFile(mPipe, aBuffer, aSize, &bytesRead, &mOverlapped)) {
    switch (GetLastError()) {
      case ERROR_IO_PENDING: {
        mEvent.Wait();
      }
        if (!GetOverlappedResult(mPipe, &mOverlapped, &bytesRead, FALSE)) {
          ADD_FAILURE() << "GetOverlappedResult failed";
          return -1;
        }
        if (mBytesTransferred != bytesRead) {
          ADD_FAILURE() << "GetOverlappedResult mismatch";
          return -1;
        }

        break;
      default:
        ADD_FAILURE() << "ReadFile error " << GetLastError();
        return -1;
    }
  } else {
    mEvent.Wait();

    if (mBytesTransferred != bytesRead) {
      ADD_FAILURE() << "GetOverlappedResult mismatch";
      return -1;
    }
  }

  mBytesTransferred = 0;
  return bytesRead;
}

int nsNamedPipeDataObserver::Write(const void* aBuffer, uint32_t aSize) {
  DWORD bytesWritten = 0;
  if (!WriteFile(mPipe, aBuffer, aSize, &bytesWritten, &mOverlapped)) {
    switch (GetLastError()) {
      case ERROR_IO_PENDING: {
        mEvent.Wait();
      }
        if (!GetOverlappedResult(mPipe, &mOverlapped, &bytesWritten, FALSE)) {
          ADD_FAILURE() << "GetOverlappedResult failed";
          return -1;
        }
        if (mBytesTransferred != bytesWritten) {
          ADD_FAILURE() << "GetOverlappedResult mismatch";
          return -1;
        }

        break;
      default:
        ADD_FAILURE() << "WriteFile error " << GetLastError();
        return -1;
    }
  } else {
    mEvent.Wait();

    if (mBytesTransferred != bytesWritten) {
      ADD_FAILURE() << "GetOverlappedResult mismatch";
      return -1;
    }
  }

  mBytesTransferred = 0;
  return bytesWritten;
}

NS_IMETHODIMP
nsNamedPipeDataObserver::OnDataAvailable(uint32_t aBytesTransferred,
                                         void* aOverlapped) {
  if (aOverlapped != &mOverlapped) {
    ADD_FAILURE() << "invalid overlapped object";
    return NS_ERROR_FAILURE;
  }

  DWORD bytesTransferred = 0;
  BOOL ret =
      GetOverlappedResult(mPipe, reinterpret_cast<LPOVERLAPPED>(aOverlapped),
                          &bytesTransferred, FALSE);

  if (!ret) {
    ADD_FAILURE() << "GetOverlappedResult failed";
    return NS_ERROR_FAILURE;
  }

  if (bytesTransferred != aBytesTransferred) {
    ADD_FAILURE() << "GetOverlappedResult mismatch";
    return NS_ERROR_FAILURE;
  }

  mBytesTransferred += aBytesTransferred;
  mEvent.Set();

  return NS_OK;
}

NS_IMETHODIMP
nsNamedPipeDataObserver::OnError(uint32_t aError, void* aOverlapped) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

BOOL CreateAndConnectInstance(LPOVERLAPPED aOverlapped, LPHANDLE aPipe);
BOOL ConnectToNewClient(HANDLE aPipe, LPOVERLAPPED aOverlapped);

BOOL CreateAndConnectInstance(LPOVERLAPPED aOverlapped, LPHANDLE aPipe) {
  // FIXME: adjust parameters
  *aPipe =
      CreateNamedPipeA(PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                       PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
                       65536, 65536, 3000, NULL);

  if (*aPipe == INVALID_HANDLE_VALUE) {
    ADD_FAILURE() << "CreateNamedPipe failed " << GetLastError();
    return FALSE;
  }

  return ConnectToNewClient(*aPipe, aOverlapped);
}

BOOL ConnectToNewClient(HANDLE aPipe, LPOVERLAPPED aOverlapped) {
  if (ConnectNamedPipe(aPipe, aOverlapped)) {
    ADD_FAILURE()
        << "Unexpected, overlapped ConnectNamedPipe() always returns 0.";
    return FALSE;
  }

  switch (GetLastError()) {
    case ERROR_IO_PENDING:
      return TRUE;

    case ERROR_PIPE_CONNECTED:
      if (SetEvent(aOverlapped->hEvent)) break;

    default:  // error
      ADD_FAILURE() << "ConnectNamedPipe failed " << GetLastError();
      break;
  }

  return FALSE;
}

static nsresult CreateNamedPipe(LPHANDLE aServer, LPHANDLE aClient) {
  OVERLAPPED overlapped;
  overlapped.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
  BOOL ret;

  ret = CreateAndConnectInstance(&overlapped, aServer);
  if (!ret) {
    ADD_FAILURE() << "pipe server should be pending";
    return NS_ERROR_FAILURE;
  }

  *aClient = CreateFileA(PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

  if (*aClient == INVALID_HANDLE_VALUE) {
    ADD_FAILURE() << "Unable to create pipe client";
    CloseHandle(*aServer);
    return NS_ERROR_FAILURE;
  }

  DWORD pipeMode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(*aClient, &pipeMode, nullptr, nullptr)) {
    ADD_FAILURE() << "SetNamedPipeHandleState error " << GetLastError();
    CloseHandle(*aServer);
    CloseHandle(*aClient);
    return NS_ERROR_FAILURE;
  }

  WaitForSingleObjectEx(overlapped.hEvent, INFINITE, TRUE);

  return NS_OK;
}

TEST(TestNamedPipeService, Test)
{
  nsCOMPtr<nsINamedPipeService> svc = net::NamedPipeService::GetOrCreate();

  HANDLE readPipe, writePipe;
  nsresult rv = CreateNamedPipe(&readPipe, &writePipe);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<nsNamedPipeDataObserver> readObserver =
      new nsNamedPipeDataObserver(readPipe);
  RefPtr<nsNamedPipeDataObserver> writeObserver =
      new nsNamedPipeDataObserver(writePipe);

  ASSERT_TRUE(NS_SUCCEEDED(svc->AddDataObserver(readPipe, readObserver)));
  ASSERT_TRUE(NS_SUCCEEDED(svc->AddDataObserver(writePipe, writeObserver)));
  ASSERT_EQ(std::size_t(writeObserver->Write(TEST_STR, sizeof(TEST_STR))),
            sizeof(TEST_STR));

  char buffer[sizeof(TEST_STR)];
  ASSERT_EQ(std::size_t(readObserver->Read(buffer, sizeof(buffer))),
            sizeof(TEST_STR));
  ASSERT_STREQ(buffer, TEST_STR) << "I/O mismatch";

  ASSERT_TRUE(NS_SUCCEEDED(svc->RemoveDataObserver(readPipe, readObserver)));
  ASSERT_TRUE(NS_SUCCEEDED(svc->RemoveDataObserver(writePipe, writeObserver)));
}
