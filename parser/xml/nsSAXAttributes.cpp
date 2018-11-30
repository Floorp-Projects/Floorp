/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSAXAttributes.h"

NS_IMPL_ISUPPORTS(nsSAXAttributes, nsISAXAttributes)

NS_IMETHODIMP
nsSAXAttributes::GetIndexFromName(const nsAString &aURI,
                                  const nsAString &aLocalName,
                                  int32_t *aResult) {
  int32_t len = mAttrs.Length();
  int32_t i;
  for (i = 0; i < len; ++i) {
    const SAXAttr &att = mAttrs[i];
    if (att.localName.Equals(aLocalName) && att.uri.Equals(aURI)) {
      *aResult = i;
      return NS_OK;
    }
  }
  *aResult = -1;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetIndexFromQName(const nsAString &aQName, int32_t *aResult) {
  int32_t len = mAttrs.Length();
  int32_t i;
  for (i = 0; i < len; ++i) {
    const SAXAttr &att = mAttrs[i];
    if (att.qName.Equals(aQName)) {
      *aResult = i;
      return NS_OK;
    }
  }
  *aResult = -1;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetLength(int32_t *aResult) {
  *aResult = mAttrs.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetLocalName(uint32_t aIndex, nsAString &aResult) {
  uint32_t len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.localName;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetQName(uint32_t aIndex, nsAString &aResult) {
  uint32_t len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.qName;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetType(uint32_t aIndex, nsAString &aResult) {
  uint32_t len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.type;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetTypeFromName(const nsAString &aURI,
                                 const nsAString &aLocalName,
                                 nsAString &aResult) {
  int32_t index = -1;
  GetIndexFromName(aURI, aLocalName, &index);
  if (index >= 0) {
    aResult = mAttrs[index].type;
  } else {
    aResult.SetIsVoid(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetTypeFromQName(const nsAString &aQName, nsAString &aResult) {
  int32_t index = -1;
  GetIndexFromQName(aQName, &index);
  if (index >= 0) {
    aResult = mAttrs[index].type;
  } else {
    aResult.SetIsVoid(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetURI(uint32_t aIndex, nsAString &aResult) {
  uint32_t len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.uri;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetValue(uint32_t aIndex, nsAString &aResult) {
  uint32_t len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.value;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetValueFromName(const nsAString &aURI,
                                  const nsAString &aLocalName,
                                  nsAString &aResult) {
  int32_t index = -1;
  GetIndexFromName(aURI, aLocalName, &index);
  if (index >= 0) {
    aResult = mAttrs[index].value;
  } else {
    aResult.SetIsVoid(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetValueFromQName(const nsAString &aQName,
                                   nsAString &aResult) {
  int32_t index = -1;
  GetIndexFromQName(aQName, &index);
  if (index >= 0) {
    aResult = mAttrs[index].value;
  } else {
    aResult.SetIsVoid(true);
  }

  return NS_OK;
}

nsresult nsSAXAttributes::AddAttribute(const nsAString &aURI,
                                       const nsAString &aLocalName,
                                       const nsAString &aQName,
                                       const nsAString &aType,
                                       const nsAString &aValue) {
  SAXAttr *att = mAttrs.AppendElement();
  if (!att) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  att->uri = aURI;
  att->localName = aLocalName;
  att->qName = aQName;
  att->type = aType;
  att->value = aValue;

  return NS_OK;
}
