/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEmbedStream_h__
#define nsEmbedStream_h__

#include "nsCOMPtr.h"
#include "nsIOutputStream.h"
#include "nsIURI.h"
#include "nsIWebBrowser.h"

class nsEmbedStream : public nsISupports
{
public:
  nsEmbedStream();

  void InitOwner(nsIWebBrowser* aOwner);
  nsresult Init(void);

  nsresult OpenStream(nsIURI* aBaseURI, const nsACString& aContentType);
  nsresult AppendToStream(const uint8_t* aData, uint32_t aLen);
  nsresult CloseStream(void);

  NS_DECL_ISUPPORTS

protected:
  virtual ~nsEmbedStream();

private:
  nsIWebBrowser* mOwner;
  nsCOMPtr<nsIOutputStream> mOutputStream;
};

#endif // nsEmbedStream_h__
