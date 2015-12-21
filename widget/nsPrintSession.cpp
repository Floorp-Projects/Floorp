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
nsPrintSession::nsPrintSession()
{
}

//-----------------------------------------------------------------------------
nsPrintSession::~nsPrintSession()
{
}

//-----------------------------------------------------------------------------
nsresult nsPrintSession::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSession::GetRemotePrintJob(RemotePrintJobChild** aRemotePrintJob)
{
  MOZ_ASSERT(aRemotePrintJob);
  RefPtr<RemotePrintJobChild> result = mRemotePrintJob;
  result.forget(aRemotePrintJob);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSession::SetRemotePrintJob(RemotePrintJobChild* aRemotePrintJob)
{
  mRemotePrintJob = aRemotePrintJob;
  return NS_OK;
}
