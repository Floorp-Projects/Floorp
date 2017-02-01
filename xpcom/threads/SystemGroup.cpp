/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SystemGroup.h"

#include "mozilla/Move.h"
#include "nsINamed.h"

using namespace mozilla;

/* static */ nsresult
SystemGroup::Dispatch(const char* aName,
                      TaskCategory aCategory,
                      already_AddRefed<nsIRunnable>&& aRunnable)
{
  return Dispatcher::UnlabeledDispatch(aName, aCategory, Move(aRunnable));
}

/* static */ nsIEventTarget*
SystemGroup::EventTargetFor(TaskCategory aCategory)
{
  nsCOMPtr<nsIEventTarget> main = do_GetMainThread();
  return main;
}
