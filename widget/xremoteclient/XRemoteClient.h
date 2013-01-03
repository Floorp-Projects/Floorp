/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <X11/X.h>
#include <X11/Xlib.h>

#include "nsRemoteClient.h"

class XRemoteClient : public nsRemoteClient
{
public:
  XRemoteClient();
  ~XRemoteClient();

  virtual nsresult Init();
  virtual nsresult SendCommand(const char *aProgram, const char *aUsername,
                               const char *aProfile, const char *aCommand,
                               const char* aDesktopStartupID,
                               char **aResponse, bool *aSucceeded);
  virtual nsresult SendCommandLine(const char *aProgram, const char *aUsername,
                                   const char *aProfile,
                                   int32_t argc, char **argv,
                                   const char* aDesktopStartupID,
                                   char **aResponse, bool *aSucceeded);
  void Shutdown();

private:

  Window         CheckWindow      (Window aWindow);
  Window         CheckChildren    (Window aWindow);
  nsresult       GetLock          (Window aWindow, bool *aDestroyed);
  nsresult       FreeLock         (Window aWindow);
  Window         FindBestWindow   (const char *aProgram,
                                   const char *aUsername,
                                   const char *aProfile,
                                   bool aSupportsCommandLine);
  nsresult     SendCommandInternal(const char *aProgram, const char *aUsername,
                                   const char *aProfile, const char *aCommand,
                                   int32_t argc, char **argv,
                                   const char* aDesktopStartupID,
                                   char **aResponse, bool *aWindowFound);
  nsresult       DoSendCommand    (Window aWindow,
                                   const char *aCommand,
                                   const char* aDesktopStartupID,
                                   char **aResponse,
                                   bool *aDestroyed);
  nsresult       DoSendCommandLine(Window aWindow,
                                   int32_t argc, char **argv,
                                   const char* aDesktopStartupID,
                                   char **aResponse,
                                   bool *aDestroyed);
  bool           WaitForResponse  (Window aWindow, char **aResponse,
                                   bool *aDestroyed, Atom aCommandAtom);

  Display       *mDisplay;

  Atom           mMozVersionAtom;
  Atom           mMozLockAtom;
  Atom           mMozCommandAtom;
  Atom           mMozCommandLineAtom;
  Atom           mMozResponseAtom;
  Atom           mMozWMStateAtom;
  Atom           mMozUserAtom;
  Atom           mMozProfileAtom;
  Atom           mMozProgramAtom;

  char          *mLockData;

  bool           mInitialized;
};
