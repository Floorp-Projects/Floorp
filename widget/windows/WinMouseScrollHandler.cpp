/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif // MOZ_LOGGING
#include "prlog.h"

#include "WinMouseScrollHandler.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

#ifdef PR_LOGGING
PRLogModuleInfo* gMouseScrollLog = nsnull;
#endif

MouseScrollHandler* MouseScrollHandler::sInstance = nsnull;


/******************************************************************************
 *
 * MouseScrollHandler
 *
 ******************************************************************************/

/* static */
void
MouseScrollHandler::Initialize()
{
#ifdef PR_LOGGING
  if (!gMouseScrollLog) {
    gMouseScrollLog = PR_NewLogModule("MouseScrollHandlerWidgets");
  }
#endif
}

/* static */
void
MouseScrollHandler::Shutdown()
{
  delete sInstance;
  sInstance = nsnull;
}

/* static */
MouseScrollHandler*
MouseScrollHandler::GetInstance()
{
  if (!sInstance) {
    sInstance = new MouseScrollHandler();
  }
  return sInstance;
}

MouseScrollHandler::MouseScrollHandler()
{
  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll: Creating an instance, this=%p, sInstance=%p",
     this, sInstance));
}

MouseScrollHandler::~MouseScrollHandler()
{
  PR_LOG(gMouseScrollLog, PR_LOG_ALWAYS,
    ("MouseScroll: Destroying an instance, this=%p, sInstance=%p",
     this, sInstance));
}

/* static */
bool
MouseScrollHandler::ProcessMessage(nsWindow* aWindow, UINT msg,
                                   WPARAM wParam, LPARAM lParam,
                                   LRESULT *aRetValue, bool &aEatMessage)
{
  return false;
}


} // namespace widget
} // namespace mozilla
