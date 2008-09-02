/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include <windows.h>
#include <windowsx.h>
#include <process.h>

#include "thread.h"
#include "dbg.h"

static DWORD WINAPI ThreadFunction(void * lp)
{
  if (lp == NULL)
    return 0L;

  CThread * thread = (CThread *)lp;
  BOOL res = thread->init();
  thread->setInitEvent();

  if(res)
    thread->run();

  thread->shut();
  DWORD ret;
  GetExitCodeThread(thread->getHandle(), &ret);
  thread->setShutEvent();
  return ret;
}

CThread::CThread(DWORD aInitTimeOut, DWORD aShutTimeOut) :
  mThread(NULL),
  mID(0),
  mState(ts_dead),
  mEventThreadInitializationFinished(NULL),
  mEventThreadShutdownFinished(NULL),
  mInitTimeOut(aInitTimeOut),
  mShutTimeOut(aShutTimeOut),
  mEventDo(NULL),
  mAction(0)
{
  dbgOut1("CThread::CThread");
}

CThread::~CThread()
{
  dbgOut1("CThread::~CThread");
}

BOOL CThread::open(CThread * aThread)
{
  dbgOut1("CThread::open");

  mEventThreadInitializationFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
  mEventThreadShutdownFinished = CreateEvent(NULL, TRUE, FALSE, NULL);
  mEventDo = CreateEvent(NULL, TRUE, FALSE, NULL);

  mThread = (HANDLE)::_beginthreadex(0, 1024, (UINT (__stdcall *)(void *))ThreadFunction, aThread, 0L, (UINT *)&mID);

  if(mThread == NULL)
    return FALSE;

  SetThreadPriority(mThread, THREAD_PRIORITY_NORMAL);

  if(mEventThreadInitializationFinished != NULL)
    WaitForSingleObject(mEventThreadInitializationFinished, mInitTimeOut);

  return TRUE;
}

void CThread::close(CThread * aThread)
{
  switch (mState) {
    case ts_dead:
      break;
    case ts_ready:
      mState = ts_dead;
      SetEvent(mEventDo);
      if(mEventThreadShutdownFinished != NULL)
        WaitForSingleObject(mEventThreadShutdownFinished, mShutTimeOut);
      break;
    case ts_busy:
    {
      aThread->shut();
      DWORD ret;
      GetExitCodeThread(aThread->getHandle(), &ret);
      TerminateThread(mThread, ret);
      mState = ts_dead;
      break;
    }
    default:
      break;
  }

  mThread = NULL;
  CloseHandle(mEventThreadInitializationFinished);
  CloseHandle(mEventThreadShutdownFinished);
  CloseHandle(mEventDo);
  dbgOut1("CThread::close");
}

BOOL CThread::setInitEvent()
{
  return SetEvent(mEventThreadInitializationFinished);
}

BOOL CThread::setShutEvent()
{
  return SetEvent(mEventThreadShutdownFinished);
}

BOOL CThread::isAlive()
{
  return (mThread != NULL);
}

BOOL CThread::isBusy()
{
  return (mState == ts_busy);
}

HANDLE CThread::getHandle()
{
  return mThread;
}

void CThread::run()
{
  mState = ts_ready;

  while(mState != ts_dead) {
    WaitForSingleObject(mEventDo, INFINITE);
    //dbgOut2("CThread::run(): Do event signalled, %u", mState);
    ResetEvent(mEventDo);
    if(mState == ts_dead)
      return;
    mState = ts_busy;
    dispatch();
    mState = ts_ready;
  }
  mState = ts_dead;
}

BOOL CThread::tryAction(int aAction)
{
  if(mState != ts_ready)
    return FALSE;

  mAction = aAction;
  SetEvent(mEventDo);

  return TRUE;
}

BOOL CThread::doAction(int aAction)
{
  while(mState != ts_ready) {
    //dbgOut2("CThread::doAction %i -- waiting...", aAction);
    if(mState == ts_dead)
      return FALSE;
    Sleep(0);
  }

  mState = ts_busy;
  mAction = aAction;
  //dbgOut2("CThread::doAction -- about to signal %i", aAction);
  SetEvent(mEventDo);

  return TRUE;
}
