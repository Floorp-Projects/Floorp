/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsNSSASN1Object.h"
#include "nsIComponentManager.h"
#include "secasn1.h"
#include "nsReadableUtils.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"
#include "nsXPCOMCID.h"

NS_IMPL_ISUPPORTS(nsNSSASN1Sequence, nsIASN1Sequence, nsIASN1Object)
NS_IMPL_ISUPPORTS(nsNSSASN1PrintableItem, nsIASN1PrintableItem, nsIASN1Object)

// This function is used to interpret an integer that
// was encoded in a DER buffer. This function is used
// when converting a DER buffer into a nsIASN1Object 
// structure.  This interprets the buffer in data
// as defined by the DER (Distinguised Encoding Rules) of
// ASN1.
static int
getInteger256(unsigned char *data, unsigned int nb)
{
    int val;

    switch (nb) {
      case 1:
        val = data[0];
        break;
      case 2:
        val = (data[0] << 8) | data[1];
        break;
      case 3:
        val = (data[0] << 16) | (data[1] << 8) | data[2];
        break;
      case 4:
        val = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
        break;
      default:
        return -1;
    }

    return val;
}

// This function is used to retrieve the lenght of a DER encoded
// item.  It looks to see if this a multibyte length and then
// interprets the buffer accordingly to get the actual length value.
// This funciton is used mostly while parsing the DER headers.
// 
// A DER encoded item has the following structure:
//
//  <tag><length<data consisting of lenght bytes>
static int32_t
getDERItemLength(unsigned char *data, unsigned char *end,
                 unsigned long *bytesUsed, bool *indefinite)
{
  unsigned char lbyte = *data++;
  int32_t length = -1;
  
  *indefinite = false;
  if (lbyte >= 0x80) {
    // Multibyte length
    unsigned nb = (unsigned) (lbyte & 0x7f);
    if (nb > 4) {
      return -1;
    }
    if (nb > 0) {
    
      if ((data+nb) > end) {
        return -1;
      }
      length = getInteger256(data, nb);
      if (length < 0)
        return -1;
    } else {
      *indefinite = true;
      length = 0;
    }
    *bytesUsed = nb+1;
  } else {
    length = lbyte;
    *bytesUsed = 1; 
  }
  return length;
}

static nsresult
buildASN1ObjectFromDER(unsigned char *data,
                       unsigned char *end,
                       nsIASN1Sequence *parent)
{
  nsresult rv;
  nsCOMPtr<nsIASN1Sequence> sequence;
  nsCOMPtr<nsIASN1PrintableItem> printableItem;
  nsCOMPtr<nsIASN1Object> asn1Obj;
  nsCOMPtr<nsIMutableArray> parentObjects;

  NS_ENSURE_ARG_POINTER(parent);
  if (data >= end)
    return NS_OK;

  unsigned char code, tagnum;

  // A DER item has the form of |tag|len|data
  // tag is one byte and describes the type of element
  //     we are dealing with.
  // len is a DER encoded int telling us how long the data is
  // data is a buffer that is len bytes long and has to be
  //      interpreted according to its type.
  unsigned long bytesUsed;
  bool indefinite;
  int32_t len;
  uint32_t type;

  rv = parent->GetASN1Objects(getter_AddRefs(parentObjects));
  if (NS_FAILED(rv) || !parentObjects)
    return NS_ERROR_FAILURE;
  while (data < end) {
    code = *data;
    tagnum = code & SEC_ASN1_TAGNUM_MASK;

    /*
     * NOTE: This code does not (yet) handle the high-tag-number form!
     */
    if (tagnum == SEC_ASN1_HIGH_TAG_NUMBER) {
      return NS_ERROR_FAILURE;
    }
    data++;
    len = getDERItemLength(data, end, &bytesUsed, &indefinite);
    data += bytesUsed;
    if ((len < 0) || ((data+len) > end))
      return NS_ERROR_FAILURE;

    if (code & SEC_ASN1_CONSTRUCTED) {
      if (len > 0 || indefinite) {
        sequence = new nsNSSASN1Sequence();
        switch (code & SEC_ASN1_CLASS_MASK) {
        case SEC_ASN1_UNIVERSAL:
          type = tagnum;
          break;
        case SEC_ASN1_APPLICATION:
          type = nsIASN1Object::ASN1_APPLICATION;
          break;
        case SEC_ASN1_CONTEXT_SPECIFIC:
          type = nsIASN1Object::ASN1_CONTEXT_SPECIFIC;
          break;
        case SEC_ASN1_PRIVATE:
          type = nsIASN1Object::ASN1_PRIVATE;
          break;
        default:
          NS_ERROR("Bad DER");
          return NS_ERROR_FAILURE;
        }
        sequence->SetTag(tagnum);
        sequence->SetType(type);
        rv = buildASN1ObjectFromDER(data, (len == 0) ? end : data + len, 
                                    sequence);
        asn1Obj = sequence;
      }
    } else {
      printableItem = new nsNSSASN1PrintableItem();

      asn1Obj = printableItem;
      asn1Obj->SetType(tagnum);
      asn1Obj->SetTag(tagnum); 
      printableItem->SetData((char*)data, len);
    }
    data += len;
    parentObjects->AppendElement(asn1Obj, false);
  }

  return NS_OK;
}

nsresult
CreateFromDER(unsigned char *data,
              unsigned int   len,
              nsIASN1Object **retval)
{
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence;
  *retval = nullptr;
  
  nsresult rv =  buildASN1ObjectFromDER(data, data+len, sequence);

  if (NS_SUCCEEDED(rv)) {
    // The actual object will be the first element inserted
    // into the sequence of the sequence variable we created.
    nsCOMPtr<nsIMutableArray> elements;

    sequence->GetASN1Objects(getter_AddRefs(elements));
    nsCOMPtr<nsIASN1Object> asn1Obj = do_QueryElementAt(elements, 0);
    *retval = asn1Obj;
    if (!*retval)
      return NS_ERROR_FAILURE;

    NS_ADDREF(*retval);
      
  }
  return rv; 
}

nsNSSASN1Sequence::nsNSSASN1Sequence() : mType(0),
                                         mTag(0),
                                         mIsValidContainer(true),
                                         mIsExpanded(true)
{
  /* member initializers and constructor code */
}

nsNSSASN1Sequence::~nsNSSASN1Sequence()
{
  /* destructor code */
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetASN1Objects(nsIMutableArray * *aASN1Objects)
{
  if (!mASN1Objects) {
    mASN1Objects = do_CreateInstance(NS_ARRAY_CONTRACTID);
  }
  *aASN1Objects = mASN1Objects;
  NS_IF_ADDREF(*aASN1Objects);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetASN1Objects(nsIMutableArray * aASN1Objects)
{
  mASN1Objects = aASN1Objects;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetTag(uint32_t *aTag)
{
  *aTag = mTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetTag(uint32_t aTag)
{
  mTag = aTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetType(uint32_t *aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetType(uint32_t aType)
{
  mType = aType;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetDisplayName(nsAString &aDisplayName)
{
  aDisplayName = mDisplayName;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetDisplayName(const nsAString &aDisplayName)
{
  mDisplayName = aDisplayName;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetDisplayValue(nsAString &aDisplayValue)
{
  aDisplayValue = mDisplayValue;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetDisplayValue(const nsAString &aDisplayValue)
{
  mDisplayValue = aDisplayValue;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetIsValidContainer(bool *aIsValidContainer)
{
  NS_ENSURE_ARG_POINTER(aIsValidContainer);
  *aIsValidContainer = mIsValidContainer;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Sequence::SetIsValidContainer(bool aIsValidContainer)
{
  mIsValidContainer = aIsValidContainer;
  SetIsExpanded(mIsValidContainer);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetIsExpanded(bool *aIsExpanded)
{
  NS_ENSURE_ARG_POINTER(aIsExpanded);
  *aIsExpanded = mIsExpanded;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetIsExpanded(bool aIsExpanded)
{
  mIsExpanded = aIsExpanded;
  return NS_OK;
}


nsNSSASN1PrintableItem::nsNSSASN1PrintableItem() : mType(0),
                                                   mTag(0),
                                                   mData(nullptr),
                                                   mLen(0)
{
  /* member initializers and constructor code */
}

nsNSSASN1PrintableItem::~nsNSSASN1PrintableItem()
{
  /* destructor code */
  if (mData)
    nsMemory::Free(mData);
}

/* readonly attribute wstring value; */
NS_IMETHODIMP 
nsNSSASN1PrintableItem::GetDisplayValue(nsAString &aValue)
{
  aValue = mValue;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetDisplayValue(const nsAString &aValue)
{
  mValue = aValue;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::GetTag(uint32_t *aTag)
{
  *aTag = mTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetTag(uint32_t aTag)
{
  mTag = aTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::GetType(uint32_t *aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetType(uint32_t aType)
{
  mType = aType;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetData(char *data, uint32_t len)
{
  if (len > 0) {
    if (mLen < len) {
      unsigned char* newData = (unsigned char*)nsMemory::Realloc(mData, len);
      if (!newData)
        return NS_ERROR_OUT_OF_MEMORY;

      mData = newData;
    }

    memcpy(mData, data, len);
  } else if (len == 0) {
    if (mData) {
      nsMemory::Free(mData);
      mData = nullptr;
    }
  }
  mLen = len;
  return NS_OK;  
}

NS_IMETHODIMP
nsNSSASN1PrintableItem::GetData(char **outData, uint32_t *outLen)
{
  NS_ENSURE_ARG_POINTER(outData);
  NS_ENSURE_ARG_POINTER(outLen);

  *outData = (char*)mData;
  *outLen  = mLen;
  return NS_OK;
}

/* attribute wstring displayName; */
NS_IMETHODIMP 
nsNSSASN1PrintableItem::GetDisplayName(nsAString &aDisplayName)
{
  aDisplayName = mDisplayName;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetDisplayName(const nsAString &aDisplayName)
{
  mDisplayName = aDisplayName;
  return NS_OK;
}

