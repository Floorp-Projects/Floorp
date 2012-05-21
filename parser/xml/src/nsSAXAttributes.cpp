/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSAXAttributes.h"

NS_IMPL_ISUPPORTS2(nsSAXAttributes, nsISAXAttributes, nsISAXMutableAttributes)

NS_IMETHODIMP
nsSAXAttributes::GetIndexFromName(const nsAString &aURI,
                                  const nsAString &aLocalName,
                                  PRInt32 *aResult)
{
  PRInt32 len = mAttrs.Length();
  PRInt32 i;
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
nsSAXAttributes::GetIndexFromQName(const nsAString &aQName, PRInt32 *aResult)
{
  PRInt32 len = mAttrs.Length();
  PRInt32 i;
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
nsSAXAttributes::GetLength(PRInt32 *aResult)
{
  *aResult = mAttrs.Length();
  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetLocalName(PRUint32 aIndex, nsAString &aResult)
{
  PRUint32 len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.localName;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetQName(PRUint32 aIndex, nsAString &aResult)
{
  PRUint32 len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.qName;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetType(PRUint32 aIndex, nsAString &aResult)
{
  PRUint32 len = mAttrs.Length();
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
  PRInt32 index = -1;
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
  PRInt32 index = -1;
  GetIndexFromQName(aQName, &index);
  if (index >= 0) {
    aResult = mAttrs[index].type;
  } else {
    aResult.SetIsVoid(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetURI(PRUint32 aIndex, nsAString &aResult)
{
  PRUint32 len = mAttrs.Length();
  if (aIndex >= len) {
    aResult.SetIsVoid(true);
  } else {
    const SAXAttr &att = mAttrs[aIndex];
    aResult = att.uri;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::GetValue(PRUint32 aIndex, nsAString &aResult)
{
  PRUint32 len = mAttrs.Length();
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
  PRInt32 index = -1;
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
  PRInt32 index = -1;
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
nsSAXAttributes::RemoveAttribute(PRUint32 aIndex)
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
  PRInt32 len;
  rv = aAttributes->GetLength(&len);
  NS_ENSURE_SUCCESS(rv, rv);

  mAttrs.Clear();
  SAXAttr *att;
  PRInt32 i;
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
nsSAXAttributes::SetAttribute(PRUint32 aIndex,
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
nsSAXAttributes::SetLocalName(PRUint32 aIndex, const nsAString &aLocalName)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].localName = aLocalName;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetQName(PRUint32 aIndex, const nsAString &aQName)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].qName = aQName;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetType(PRUint32 aIndex, const nsAString &aType)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].type = aType;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetURI(PRUint32 aIndex, const nsAString &aURI)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].uri = aURI;

  return NS_OK;
}

NS_IMETHODIMP
nsSAXAttributes::SetValue(PRUint32 aIndex, const nsAString &aValue)
{
  if (aIndex >= mAttrs.Length()) {
    return NS_ERROR_FAILURE;
  }
  mAttrs[aIndex].value = aValue;

  return NS_OK;
}
