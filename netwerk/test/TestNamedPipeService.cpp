/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestCommon.h"
#include "TestHarness.h"

#include <Windows.h>

#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "nsINamedPipeService.h"
#include "nsNetCID.h"

#define PIPE_NAME "\\\\.\\pipe\\TestNPS"
#define TEST_STR "Hello World"

using namespace mozilla;

class nsNamedPipeDataObserver : public nsINamedPipeDataObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINAMEDPIPEDATAOBSERVER

  explicit nsNamedPipeDataObserver(HANDLE aPipe);

  int Read(void* aBuffer, uint32_t aSize);
  int Write(void* aBuffer, uint32_t aSize);

  uint32_t Transferred() const { return mBytesTransferred; }

private:
  ~nsNamedPipeDataObserver() = default;

  HANDLE mPipe;
  OVERLAPPED mOverlapped;
  Atomic<uint32_t> mBytesTransferred;
  Monitor mMonitor;
};

NS_IMPL_ISUPPORTS(nsNamedPipeDataObserver, nsINamedPipeDataObserver)

nsNamedPipeDataObserver::nsNamedPipeDataObserver(HANDLE aPipe)
  : mPipe(aPipe)
  , mOverlapped()
  , mBytesTransferred(0)
  , mMonitor("named-pipe")
{
  mOverlapped.hEvent = CreateEventA(nullptr, TRUE, TRUE, "named-pipe");
}

int
nsNamedPipeDataObserver::Read(void* aBuffer, uint32_t aSize)
{
  DWORD bytesRead = 0;
  if (!ReadFile(mPipe, aBuffer, aSize, &bytesRead, &mOverlapped)) {
    switch(GetLastError()) {
      case ERROR_IO_PENDING:
        {
          MonitorAutoLock lock(mMonitor);
          mMonitor.Wait();
        }
        if (!GetOverlappedResult(mPipe, &mOverlapped, &bytesRead, FALSE)) {
          fail("GetOverlappedResult failed");
          return -1;
        }
        if (mBytesTransferred != bytesRead) {
          fail("GetOverlappedResult mismatch");
          return -1;
        }

        break;
      default:
        fail("ReadFile error %d", GetLastError());
        return -1;
    }
  } else {
    MonitorAutoLock lock(mMonitor);
    mMonitor.Wait();

    if (mBytesTransferred != bytesRead) {
      fail("GetOverlappedResult mismatch");
      return -1;
    }
  }

  mBytesTransferred = 0;
  passed("[read] match");
  return bytesRead;
}

int
nsNamedPipeDataObserver::Write(void* aBuffer, uint32_t aSize)
{
  DWORD bytesWritten = 0;
  if (!WriteFile(mPipe, aBuffer, aSize, &bytesWritten, &mOverlapped)) {
    switch(GetLastError()) {
      case ERROR_IO_PENDING:
        {
          MonitorAutoLock lock(mMonitor);
          mMonitor.Wait();
        }
        if (!GetOverlappedResult(mPipe, &mOverlapped, &bytesWritten, FALSE)) {
          fail("GetOverlappedResult failed");
          return -1;
        }
        if (mBytesTransferred != bytesWritten) {
          fail("GetOverlappedResult mismatch");
          return -1;
        }

        break;
      default:
        fail("WriteFile error %d", GetLastError());
        return -1;
    }
  } else {
    MonitorAutoLock lock(mMonitor);
    mMonitor.Wait();

    if (mBytesTransferred != bytesWritten) {
      fail("GetOverlappedResult mismatch");
      return -1;
    }
  }

  mBytesTransferred = 0;
  passed("[write] match");
  return bytesWritten;
}

NS_IMETHODIMP
nsNamedPipeDataObserver::OnDataAvailable(uint32_t aBytesTransferred,
                                         void *aOverlapped)
{
  if (aOverlapped != &mOverlapped) {
    fail("invalid overlapped object");
    return NS_ERROR_FAILURE;
  }

  DWORD bytesTransferred = 0;
  BOOL ret = GetOverlappedResult(mPipe,
                                 reinterpret_cast<LPOVERLAPPED>(aOverlapped),
                                 &bytesTransferred,
                                 FALSE);

  if (!ret) {
    fail("GetOverlappedResult failed");
    return NS_ERROR_FAILURE;
  }

  if (bytesTransferred != aBytesTransferred) {
    fail("GetOverlappedResult mismatch");
    return NS_ERROR_FAILURE;
  }

  mBytesTransferred += aBytesTransferred;
  MonitorAutoLock lock(mMonitor);
  mMonitor.Notify();

  return NS_OK;
}

NS_IMETHODIMP
nsNamedPipeDataObserver::OnError(uint32_t aError, void *aOverlapped)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

BOOL CreateAndConnectInstance(LPOVERLAPPED aOverlapped, LPHANDLE aPipe);
BOOL ConnectToNewClient(HANDLE aPipe, LPOVERLAPPED aOverlapped);

BOOL CreateAndConnectInstance(LPOVERLAPPED aOverlapped, LPHANDLE aPipe)
{
  if (!aPipe) {
    fail("Parameter aPipe is NULL\n");
    return FALSE;
  }

  // FIXME: adjust parameters
  *aPipe = CreateNamedPipeA(
    PIPE_NAME,
    PIPE_ACCESS_DUPLEX |
    FILE_FLAG_OVERLAPPED,
    PIPE_TYPE_MESSAGE |
    PIPE_READMODE_MESSAGE |
    PIPE_WAIT,
    1,
    65536,
    65536,
    3000,
    NULL);

  if (*aPipe == INVALID_HANDLE_VALUE) {
    fail("CreateNamedPipe failed [%d]\n", GetLastError());
    return FALSE;
  }

  return ConnectToNewClient(*aPipe, aOverlapped);
}

BOOL ConnectToNewClient(HANDLE aPipe, LPOVERLAPPED aOverlapped)
{
  if (ConnectNamedPipe(aPipe, aOverlapped)) {
    fail("Unexpected, overlapped ConnectNamedPipe() always returns 0.\n");
    return FALSE;
  }

  switch (GetLastError())
  {
  case ERROR_IO_PENDING:
    return TRUE;

  case ERROR_PIPE_CONNECTED:
    if (SetEvent(aOverlapped->hEvent))
      break;

  default: // error
    fail("ConnectNamedPipe failed [%d]\n", GetLastError());
    break;
  }

  return FALSE;
}

static nsresult
CreateNamedPipe(LPHANDLE aServer, LPHANDLE aClient)
{
  OVERLAPPED overlapped;
  overlapped.hEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
  BOOL ret;

  ret = CreateAndConnectInstance(&overlapped, aServer);
  if (!ret) {
    fail("pipe server should be pending");
    return NS_ERROR_FAILURE;
  }

  *aClient = CreateFileA(PIPE_NAME,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         nullptr,
                         OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED,
                         nullptr);

  if (*aClient == INVALID_HANDLE_VALUE) {
    fail("Unable to create pipe client");
    CloseHandle(*aServer);
    return NS_ERROR_FAILURE;
  }

  DWORD pipeMode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(*aClient, &pipeMode, nullptr, nullptr)) {
    fail("SetNamedPipeHandleState error (%d)", GetLastError());
    CloseHandle(*aServer);
    CloseHandle(*aClient);
    return NS_ERROR_FAILURE;
  }

  WaitForSingleObjectEx(overlapped.hEvent, INFINITE, TRUE);

  return NS_OK;
}

int
main(int32_t argc, char* argv[])
{
  ScopedXPCOM xpcom("NamedPipeService");
  if (xpcom.failed()) {
    fail("Unable to initalize XPCOM.");
    return -1;
  }

  nsresult rv;
  nsCOMPtr<nsINamedPipeService> svc =
    do_GetService(NS_NAMEDPIPESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    fail("Unable to create named pipe service");
    return -1;
  }

  HANDLE readPipe, writePipe;
  if (NS_FAILED(rv = CreateNamedPipe(&readPipe, &writePipe))) {
    fail("Unable to create pipes %d", GetLastError());
    return -1;
  }

  RefPtr<nsNamedPipeDataObserver> readObserver =
    new nsNamedPipeDataObserver(readPipe);
  RefPtr<nsNamedPipeDataObserver> writeObserver =
    new nsNamedPipeDataObserver(writePipe);

  if (NS_WARN_IF(NS_FAILED(svc->AddDataObserver(readPipe, readObserver)))) {
    fail("Unable to add read data observer");
    return -1;
  }

  if (NS_WARN_IF(NS_FAILED(svc->AddDataObserver(writePipe, writeObserver)))) {
    fail("Unable to add read data observer");
    return -1;
  }

  if (writeObserver->Write(TEST_STR, sizeof(TEST_STR)) != sizeof(TEST_STR)) {
    fail("write error");
    return -1;
  }

  char buffer[sizeof(TEST_STR)];
  if (readObserver->Read(buffer, sizeof(buffer)) != sizeof(TEST_STR)) {
    fail("read error");
    return -1;
  }

  if (strcmp(buffer, TEST_STR) != 0) {
    fail("I/O mismatch");
    return -1;
  }

  if (NS_WARN_IF(NS_FAILED(svc->RemoveDataObserver(readPipe, readObserver)))) {
    fail("Unable to remove read data observer");
    return -1;
  }

  if (NS_WARN_IF(NS_FAILED(svc->RemoveDataObserver(writePipe, writeObserver)))) {
    fail("Unable to remove read data observer");
    return -1;
  }

  passed("Finish");
  return 0;
}