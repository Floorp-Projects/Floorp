/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHtml5SVGLoadDispatcher_h
#define nsHtml5SVGLoadDispatcher_h

#include "nsThreadUtils.h"
#include "nsIContent.h"

class nsHtml5SVGLoadDispatcher : public mozilla::Runnable
{
  private:
    nsCOMPtr<nsIContent> mElement;
    nsCOMPtr<nsIDocument> mDocument;
  public:
    explicit nsHtml5SVGLoadDispatcher(nsIContent* aElement);
    NS_IMETHOD Run() override;
};

#endif // nsHtml5SVGLoadDispatcher_h
