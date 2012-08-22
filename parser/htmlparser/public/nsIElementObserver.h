/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 * MODULE NOTES:
 * @update  rickg 03.23.2000  //removed unused NS_PARSER_SUBJECT and predecl of nsString
 * 
 */

#ifndef nsIElementObserver_h__
#define nsIElementObserver_h__

#include "nsISupports.h"
#include "prtypes.h"
#include "nsHTMLTags.h"
#include "nsTArray.h"


// {4672AA04-F6AE-11d2-B3B7-00805F8A6670}
#define NS_IELEMENTOBSERVER_IID      \
{ 0x4672aa04, 0xf6ae, 0x11d2, { 0xb3, 0xb7, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0x70 } }


class nsIElementObserver : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IELEMENTOBSERVER_IID)

  enum { IS_DOCUMENT_WRITE = 1U };

  /* Subject call observer when the parser hit the tag */
  NS_IMETHOD Notify(nsISupports* aDocShell, 
                    nsISupports* aChannel,
                    const PRUnichar* aTag, 
                    const nsTArray<nsString>* aKeys, 
                    const nsTArray<nsString>* aValues,
                    const uint32_t aFlags) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIElementObserver, NS_IELEMENTOBSERVER_IID)

#endif /* nsIElementObserver_h__ */

