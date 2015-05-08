/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryProfiler.h"
#include "nsIDOMClassInfo.h"
#include "nsIGlobalObject.h"
#include "js/TypeDecls.h"
#include "xpcprivate.h"

NS_IMPL_ISUPPORTS(MemoryProfiler, nsIMemoryProfiler)

MemoryProfiler::MemoryProfiler()
{
  /* member initializers and constructor code */
}

MemoryProfiler::~MemoryProfiler()
{
  /* destructor code */
}

NS_IMETHODIMP
MemoryProfiler::StartProfiler()
{
  return NS_OK;
}

NS_IMETHODIMP
MemoryProfiler::StopProfiler()
{
  return NS_OK;
}

NS_IMETHODIMP
MemoryProfiler::ResetProfiler()
{
  return NS_OK;
}

NS_IMETHODIMP
MemoryProfiler::GetResults(JSContext *cx, JS::MutableHandle<JS::Value> aResult)
{
  return NS_OK;
}
