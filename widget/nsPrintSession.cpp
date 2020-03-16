/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSession.h"

#include "mozilla/layout/RemotePrintJobChild.h"

typedef mozilla::layout::RemotePrintJobChild RemotePrintJobChild;

//*****************************************************************************
//***    nsPrintSession
//*****************************************************************************

NS_IMPL_ISUPPORTS(nsPrintSession, nsIPrintSession, nsISupportsWeakReference)

//-----------------------------------------------------------------------------
nsPrintSession::nsPrintSession() = default;

//-----------------------------------------------------------------------------
nsPrintSession::~nsPrintSession() = default;

//-----------------------------------------------------------------------------
nsresult nsPrintSession::Init() { return NS_OK; }

RemotePrintJobChild* nsPrintSession::GetRemotePrintJob() {
  return mRemotePrintJob;
}

void nsPrintSession::SetRemotePrintJob(RemotePrintJobChild* aRemotePrintJob) {
  mRemotePrintJob = aRemotePrintJob;
}
