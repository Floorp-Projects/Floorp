//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Url Classifier code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsCheckSummedOutputStream_h__
#define nsCheckSummedOutputStream_h__

#include "nsILocalFile.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsICryptoHash.h"
#include "nsNetCID.h"
#include "nsString.h"
#include "../../../netwerk/base/src/nsFileStreams.h"
#include "nsToolkitCompsCID.h"

class nsCheckSummedOutputStream : public nsSafeFileOutputStream
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // Size of MD5 hash in bytes
  static const PRUint32 CHECKSUM_SIZE = 16;

  nsCheckSummedOutputStream() {}
  virtual ~nsCheckSummedOutputStream() { nsSafeFileOutputStream::Close(); }

  NS_IMETHOD Finish();
  NS_IMETHOD Write(const char *buf, PRUint32 count, PRUint32 *result);
  NS_IMETHOD Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm, PRInt32 behaviorFlags);

protected:
  nsCOMPtr<nsICryptoHash> mHash;
  nsCAutoString mCheckSum;
};

// returns a file output stream which can be QI'ed to nsIFileOutputStream.
inline nsresult
NS_NewCheckSummedOutputStream(nsIOutputStream **result,
                              nsIFile         *file,
                              PRInt32         ioFlags       = -1,
                              PRInt32         perm          = -1,
                              PRInt32         behaviorFlags =  0)
{
    nsCOMPtr<nsIFileOutputStream> out = new nsCheckSummedOutputStream();
    nsresult rv = out->Init(file, ioFlags, perm, behaviorFlags);
    if (NS_SUCCEEDED(rv))
      NS_ADDREF(*result = out);  // cannot use nsCOMPtr::swap
    return rv;
}

#endif
