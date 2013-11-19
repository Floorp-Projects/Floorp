/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIOUtil.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsStreamUtils.h"

NS_IMPL_ISUPPORTS1(nsIOUtil, nsIIOUtil)

NS_IMETHODIMP
nsIOUtil::InputStreamIsBuffered(nsIInputStream* aStream, bool* _retval)
{
  if (NS_WARN_IF(!aStream))
    return NS_ERROR_INVALID_ARG;
  *_retval = NS_InputStreamIsBuffered(aStream);
  return NS_OK;
}

NS_IMETHODIMP
nsIOUtil::OutputStreamIsBuffered(nsIOutputStream* aStream, bool* _retval)
{
  if (NS_WARN_IF(!aStream))
    return NS_ERROR_INVALID_ARG;
  *_retval = NS_OutputStreamIsBuffered(aStream);
  return NS_OK;
}
