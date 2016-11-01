/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsAppShellService_h
#define __nsAppShellService_h

#include "nsIAppShellService.h"
#include "nsIObserver.h"

//Interfaces Needed
#include "nsWebShellWindow.h"
#include "nsStringFwd.h"
#include "nsAutoPtr.h"
#include "nsITabParent.h"
#include "mozilla/Attributes.h"

// {0099907D-123C-4853-A46A-43098B5FB68C}
#define NS_APPSHELLSERVICE_CID \
{ 0x99907d, 0x123c, 0x4853, { 0xa4, 0x6a, 0x43, 0x9, 0x8b, 0x5f, 0xb6, 0x8c } }

class nsAppShellService final : public nsIAppShellService,
                                public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAPPSHELLSERVICE
  NS_DECL_NSIOBSERVER

  nsAppShellService();

protected:
  ~nsAppShellService();

  nsresult CreateHiddenWindowHelper(bool aIsPrivate);
  void EnsurePrivateHiddenWindow();

  nsresult JustCreateTopWindow(nsIXULWindow *aParent,
                               nsIURI *aUrl,
                               uint32_t aChromeMask,
                               int32_t aInitialWidth, int32_t aInitialHeight,
                               bool aIsHiddenWindow,
                               nsITabParent *aOpeningTab,
                               mozIDOMWindowProxy *aOpenerWindow,
                               nsWebShellWindow **aResult);
  uint32_t CalculateWindowZLevel(nsIXULWindow *aParent, uint32_t aChromeMask);

  RefPtr<nsWebShellWindow>  mHiddenWindow;
  RefPtr<nsWebShellWindow>  mHiddenPrivateWindow;
  bool                        mXPCOMWillShutDown;
  bool                        mXPCOMShuttingDown;
  uint16_t                    mModalWindowCount;
  bool                        mApplicationProvidedHiddenWindow;
  uint32_t                    mScreenId;
};

#endif
