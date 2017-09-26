/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSAXAttributes_h__
#define nsSAXAttributes_h__

#include "nsISupports.h"
#include "nsISAXAttributes.h"
#include "nsISAXMutableAttributes.h"
#include "nsTArray.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

struct SAXAttr
{
  nsString uri;
  nsString localName;
  nsString qName;
  nsString type;
  nsString value;
};

class nsSAXAttributes final : public nsISAXMutableAttributes
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISAXATTRIBUTES
  NS_DECL_NSISAXMUTABLEATTRIBUTES

private:
  ~nsSAXAttributes() {}
  nsTArray<SAXAttr> mAttrs;
};

#endif // nsSAXAttributes_h__
