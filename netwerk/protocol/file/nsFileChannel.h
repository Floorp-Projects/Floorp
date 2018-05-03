/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFileChannel_h__
#define nsFileChannel_h__

#include "nsBaseChannel.h"
#include "nsIFileChannel.h"
#include "nsIUploadChannel.h"

class nsFileChannel : public nsBaseChannel
                    , public nsIFileChannel
                    , public nsIUploadChannel
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFILECHANNEL
  NS_DECL_NSIUPLOADCHANNEL

  explicit nsFileChannel(nsIURI *uri);

  nsresult Init();

protected:
  ~nsFileChannel() = default;

  // Called to construct a blocking file input stream for the given file.  This
  // method also returns a best guess at the content-type for the data stream.
  // NOTE: If the channel has a type hint set, contentType will be left
  // untouched. The caller should not use it in that case.
  MOZ_MUST_USE nsresult MakeFileInputStream(nsIFile *file,
                                            nsCOMPtr<nsIInputStream> &stream,
                                            nsCString &contentType, bool async);

  virtual MOZ_MUST_USE nsresult OpenContentStream(bool async,
                                                  nsIInputStream **result,
                                                  nsIChannel** channel) override;

private:
  nsCOMPtr<nsIInputStream> mUploadStream;
  int64_t mUploadLength;
  nsCOMPtr<nsIURI> mFileURI;
};

#endif // !nsFileChannel_h__
