/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSAXAttributes.h"

NS_IMPL_ISUPPORTS(nsSAXAttributes, nsISAXAttributes, nsISAXMutableAttributes)

NS_IMETHODIMP
nsSAXAttributes::GetIndexFromName(const nsAString &aURI,
                                  const nsAString &aLocalName,
                                  int32_t *aResult)
{
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
nsSAXAttributes::GetIndexFromQName(const nsAString &aQName, int32_t *aResult)
{
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
nsSAXAttributes::GetLength(int32_t *aResult)
{
  *aResult = mAttrs.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetLocalName(uint32_t aIndex, nsAString &aResult)
{
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
nsSAXAttributes::GetQName(uint32_t aIndex, nsAString &aResult)
{
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
nsSAXAttributes::GetType(uint32_t aIndex, nsAString &aResult)
{
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
                                 nsAString &aResult)
{
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
nsSAXAttributes::GetTypeFromQName(const nsAString &aQName, nsAString &aResult)
{
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
nsSAXAttributes::GetURI(uint32_t aIndex, nsAString &aResult)
{
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
nsSAXAttributes::GetValue(uint32_t aIndex, nsAString &aResult)
{
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
                                  nsAString &aResult)
{
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
                                   nsAString &aResult)
{
  int32_t index = -1;
  GetIndexFromQName(aQName, &index);
  if (index >= 0) {
    aResult = mAttrs[index].value;
  } else {
    aResult.SetIsVoid(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::AddAttribute(const nsAString &aURI,
                              const nsAString &aLocalName,
                              const nsAString &aQName,
                              const nsAString &aType,
                              const nsAString &aValue)
{
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

NS_IMETHODIMP
nsSAXAttributes::Clear()
{
  mAttrs.Clear();

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::RemoveAttribute(uint32_t aIndex)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs.RemoveElementAt(aIndex);

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetAttributes(nsISAXAttributes *aAttributes)
{
  NS_ENSURE_ARG(aAttributes);

  nsresult rv;
  int32_t len;
  rv = aAttributes->GetLength(&len);
  NS_ENSURE_SUCCESS(rv, rv);

  mAttrs.Clear();
  SAXAttr *att;
  int32_t i;
  for (i = 0; i < len; ++i) {
    att = mAttrs.AppendElement();
    if (!att) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    rv = aAttributes->GetURI(i, att->uri);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aAttributes->GetLocalName(i, att->localName);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aAttributes->GetQName(i, att->qName);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aAttributes->GetType(i, att->type);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aAttributes->GetValue(i, att->value);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetAttribute(uint32_t aIndex,
                              const nsAString &aURI,
                              const nsAString &aLocalName,
                              const nsAString &aQName,
                              const nsAString &aType,
                              const nsAString &aValue)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }

  SAXAttr &att = mAttrs[aIndex];
  att.uri = aURI;
  att.localName = aLocalName;
  att.qName = aQName;
  att.type = aType;
  att.value = aValue;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetLocalName(uint32_t aIndex, const nsAString &aLocalName)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].localName = aLocalName;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetQName(uint32_t aIndex, const nsAString &aQName)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].qName = aQName;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetType(uint32_t aIndex, const nsAString &aType)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].type = aType;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetURI(uint32_t aIndex, const nsAString &aURI)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].uri = aURI;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetValue(uint32_t aIndex, const nsAString &aValue)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].value = aValue;

  return NS_OK;
}
