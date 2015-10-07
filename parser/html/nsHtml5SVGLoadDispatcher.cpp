/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHtml5SVGLoadDispatcher.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "nsIDocument.h"

using namespace mozilla;

nsHtml5SVGLoadDispatcher::nsHtml5SVGLoadDispatcher(nsIContent* aElement)
  : mElement(aElement)
  , mDocument(mElement->OwnerDoc())
{
  mDocument->BlockOnload();
}

NS_IMETHODIMP
nsHtml5SVGLoadDispatcher::Run()
{
  WidgetEvent event(true, eSVGLoad);
  event.mFlags.mBubbles = false;
  // Do we care about forcing presshell creation if it hasn't happened yet?
  // That is, should this code flush or something?  Does it really matter?
  // For that matter, do we really want to try getting the prescontext?
  // Does this event ever want one?
  nsRefPtr<nsPresContext> ctx;
  nsCOMPtr<nsIPresShell> shell = mElement->OwnerDoc()->GetShell();
  if (shell) {
    ctx = shell->GetPresContext();
  }
  EventDispatcher::Dispatch(mElement, ctx, &event);
  // Unblocking onload on the same document that it was blocked even if
  // the element has moved between docs since blocking.
  mDocument->UnblockOnload(false);
  return NS_OK;
}
