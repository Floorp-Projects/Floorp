/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSecurityConsoleMessage.h"

NS_IMPL_ISUPPORTS(nsSecurityConsoleMessage, nsISecurityConsoleMessage)

nsSecurityConsoleMessage::nsSecurityConsoleMessage()
{
}

nsSecurityConsoleMessage::~nsSecurityConsoleMessage()
{
}

NS_IMETHODIMP
nsSecurityConsoleMessage::GetTag(nsAString& aTag)
{
  aTag = mTag;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityConsoleMessage::SetTag(const nsAString& aTag)
{
  mTag = aTag;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityConsoleMessage::GetCategory(nsAString& aCategory)
{
  aCategory = mCategory;
  return NS_OK;
}

NS_IMETHODIMP
nsSecurityConsoleMessage::SetCategory(const nsAString& aCategory)
{
  mCategory = aCategory;
  return NS_OK;
}
