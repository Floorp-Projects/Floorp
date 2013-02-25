/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WinIMEHandler.h"
#include "nsIMM32Handler.h"

#ifdef NS_ENABLE_TSF
#include "nsTextStore.h"
#endif // #ifdef NS_ENABLE_TSF

namespace mozilla {
namespace widget {

/******************************************************************************
 * IMEHandler
 ******************************************************************************/

#ifdef NS_ENABLE_TSF
bool IMEHandler::sIsInTSFMode = false;
#endif // #ifdef NS_ENABLE_TSF

// static
void
IMEHandler::Initialize()
{
#ifdef NS_ENABLE_TSF
  nsTextStore::Initialize();
  sIsInTSFMode = nsTextStore::IsInTSFMode();
#endif // #ifdef NS_ENABLE_TSF

  nsIMM32Handler::Initialize();
}

// static
void
IMEHandler::Terminate()
{
#ifdef NS_ENABLE_TSF
  if (sIsInTSFMode) {
    nsTextStore::Terminate();
    sIsInTSFMode = false;
  }
#endif // #ifdef NS_ENABLE_TSF

  nsIMM32Handler::Terminate();
}

} // namespace widget
} // namespace mozilla
