/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessSound.h"

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS(HeadlessSound, nsISound, nsIStreamLoaderObserver)

HeadlessSound::HeadlessSound()
{
}

HeadlessSound::~HeadlessSound()
{
}

NS_IMETHODIMP
HeadlessSound::Init()
{
  return NS_OK;
}

NS_IMETHODIMP HeadlessSound::OnStreamComplete(nsIStreamLoader *aLoader,
                                              nsISupports *context,
                                              nsresult aStatus,
                                              uint32_t dataLen,
                                              const uint8_t *data)
{
  return NS_OK;
}

NS_IMETHODIMP HeadlessSound::Beep()
{
  return NS_OK;
}

NS_IMETHODIMP HeadlessSound::Play(nsIURL *aURL)
{
  return NS_OK;
}

NS_IMETHODIMP HeadlessSound::PlayEventSound(uint32_t aEventId)
{
  return NS_OK;
}

NS_IMETHODIMP HeadlessSound::PlaySystemSound(const nsAString &aSoundAlias)
{
  return NS_OK;
}

} // namespace widget
} // namespace mozilla
