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

#ifndef __THREAD_H__
#define __THREAD_H__

typedef enum {
  ts_dead = 0,
  ts_ready,
  ts_busy

} ThreadState;

class CThread {
private:
  HANDLE mThread;
  DWORD mID;
  HANDLE mEventThreadInitializationFinished;
  HANDLE mEventThreadShutdownFinished;
  DWORD mInitTimeOut;
  DWORD mShutTimeOut;
  HANDLE mEventDo;
  ThreadState mState;

protected:
  int mAction;

public:
  BOOL setInitEvent();
  BOOL setShutEvent();
  BOOL isAlive();
  BOOL isBusy();
  HANDLE getHandle();

  void run();

  // outside thread interface
  CThread(DWORD aInitTimeOut = INFINITE, DWORD aShutTimeOut = INFINITE);
  virtual ~CThread();

  BOOL open(CThread * aThread);
  void close(CThread * aThread);

  BOOL tryAction(int aAction);
  BOOL doAction(int aAction);

  // virtuals
  virtual BOOL init() = 0;
  virtual void shut() = 0; 
  virtual void dispatch() = 0;
};

BOOL InitThreadLib(HINSTANCE aInstance);
void ShutThreadLib(HINSTANCE aInstance);

#endif // THREAD_HPP__
