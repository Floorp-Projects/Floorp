/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *   Kai Engert <kaie@netscape.com>
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
#ifndef nsEntropyCollector_h___
#define nsEntropyCollector_h___

#include "nsIEntropyCollector.h"
#include "nsIBufEntropyCollector.h"
#include "nsCOMPtr.h"

#define NS_ENTROPYCOLLECTOR_CID \
 { /* 34587f4a-be18-43c0-9112-b782b08c0add */       \
  0x34587f4a, 0xbe18, 0x43c0,                       \
 {0x91, 0x12, 0xb7, 0x82, 0xb0, 0x8c, 0x0a, 0xdd} }

class nsEntropyCollector : public nsIBufEntropyCollector
{
  public:
    nsEntropyCollector();
    virtual ~nsEntropyCollector();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIENTROPYCOLLECTOR
    NS_DECL_NSIBUFENTROPYCOLLECTOR

    enum { entropy_buffer_size = 1024 };

  protected:
    unsigned char mEntropyCache[entropy_buffer_size];
    PRInt32 mBytesCollected;
    unsigned char *mWritePointer;
    nsCOMPtr<nsIEntropyCollector> mForwardTarget;
};

#endif /* !defined nsEntropyCollector_h__ */
