/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 *  Javier Delgadillo <javi@netscape.com>
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "nsNSSASN1Object.h"
#include "nsIComponentManager.h"
#include "secasn1.h"
#include "nsReadableUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsNSSASN1Sequence, nsIASN1Sequence, 
                                                 nsIASN1Object);
NS_IMPL_THREADSAFE_ISUPPORTS2(nsNSSASN1PrintableItem, nsIASN1PrintableItem,
                                                      nsIASN1Object);

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
static PRInt32
getDERItemLength(unsigned char *data, unsigned char *end,
                 unsigned long *bytesUsed, PRBool *indefinite)
{
  unsigned char lbyte = *data++;
  PRInt32 length = -1;
  
  *indefinite = PR_FALSE;
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
      *indefinite = PR_TRUE;
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
  nsCOMPtr<nsISupportsArray> parentObjects;

  NS_ENSURE_ARG_POINTER(parent);
  if (data >= end)
    return NS_OK;

  unsigned char code, tagnum;

  // A DER item has the form of |tag|len|data
  // tag is one byte and describes the type of elment
  //     we are dealing with.
  // len is a DER encoded int telling us how long the data is
  // data is a buffer that is len bytes long and has to be
  //      interpreted according to its type.
  unsigned long bytesUsed;
  PRBool indefinite;
  PRInt32 len;
  PRUint32 type;

  if (parent == nsnull) {
    parent = new nsNSSASN1Sequence();
    NS_IF_ADDREF(parent);
  }
  if (parent == nsnull) 
    return NS_ERROR_FAILURE;

  rv = parent->GetASN1Objects(getter_AddRefs(parentObjects));
  if (NS_FAILED(rv) || parentObjects == nsnull)
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
          NS_ASSERTION(0,"Bad DER");
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
    parentObjects->AppendElement(asn1Obj);    
  }

  return NS_OK;
}

nsresult
CreateFromDER(unsigned char *data,
              unsigned int   len,
              nsIASN1Object **retval)
{
  nsCOMPtr<nsIASN1Sequence> sequence = new nsNSSASN1Sequence;
  *retval = nsnull;
  
  nsresult rv =  buildASN1ObjectFromDER(data, data+len, sequence);

  if (NS_SUCCEEDED(rv)) {
    // The actual object will be the first element inserted
    // into the sequence of the sequence variable we created.
    nsCOMPtr<nsISupportsArray> elements;

    sequence->GetASN1Objects(getter_AddRefs(elements));
    nsCOMPtr<nsISupports> isupports = dont_AddRef(elements->ElementAt(0));
    nsCOMPtr<nsIASN1Object> asn1Obj(do_QueryInterface(isupports));
    *retval = asn1Obj;
    if (*retval == nsnull)
      return NS_ERROR_FAILURE;

    NS_ADDREF(*retval);
      
  }
  return rv; 
}

nsNSSASN1Sequence::nsNSSASN1Sequence() : mProcessObjects(PR_TRUE),
                                         mShowObjects(PR_TRUE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsNSSASN1Sequence::~nsNSSASN1Sequence()
{
  /* destructor code */
}

/* attribute nsISupportsArray ASN1Objects; */
NS_IMETHODIMP 
nsNSSASN1Sequence::GetASN1Objects(nsISupportsArray * *aASN1Objects)
{
  if (mASN1Objects == nsnull) {
    mASN1Objects = do_CreateInstance(NS_SUPPORTSARRAY_CONTRACTID);
  }
  *aASN1Objects = mASN1Objects;
  NS_IF_ADDREF(*aASN1Objects);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetASN1Objects(nsISupportsArray * aASN1Objects)
{
  mASN1Objects = aASN1Objects;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetTag(PRUint32 *aTag)
{
  *aTag = mTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetTag(PRUint32 aTag)
{
  mTag = aTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::GetType(PRUint32 *aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetType(PRUint32 aType)
{
  mType = aType;
  return NS_OK;
}

/* attribute wstring displayName; */
NS_IMETHODIMP 
nsNSSASN1Sequence::GetDisplayName(PRUnichar * *aDisplayName)
{
  NS_ENSURE_ARG_POINTER(aDisplayName);
  *aDisplayName = ToNewUnicode(mDisplayName);
  return (*aDisplayName) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetDisplayName(const PRUnichar * aDisplayName)
{
  mDisplayName.Assign(aDisplayName);
  return NS_OK;
}

/* attribute wstring displayValue; */
NS_IMETHODIMP 
nsNSSASN1Sequence::GetDisplayValue(PRUnichar * *aDisplayValue)
{
  NS_ENSURE_ARG_POINTER(aDisplayValue);
  *aDisplayValue = ToNewUnicode(mDisplayValue);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetDisplayValue(const PRUnichar * aDisplayValue)
{
  mDisplayValue.Assign(aDisplayValue);
  return NS_OK;
}

/* attribute boolean processObjects; */
NS_IMETHODIMP 
nsNSSASN1Sequence::GetProcessObjects(PRBool *aProcessObjects)
{
  NS_ENSURE_ARG_POINTER(aProcessObjects);
  *aProcessObjects = mProcessObjects;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Sequence::SetProcessObjects(PRBool aProcessObjects)
{
  mProcessObjects = aProcessObjects;
  SetShowObjects(mProcessObjects);
  return NS_OK;
}

/* attribute boolean showObjects; */
NS_IMETHODIMP 
nsNSSASN1Sequence::GetShowObjects(PRBool *aShowObjects)
{
  NS_ENSURE_ARG_POINTER(aShowObjects);
  *aShowObjects = mShowObjects;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Sequence::SetShowObjects(PRBool aShowObjects)
{
  mShowObjects = aShowObjects;
  return NS_OK;
}


nsNSSASN1PrintableItem::nsNSSASN1PrintableItem() : mData(nsnull),
                                                   mLen(0)
{
  NS_INIT_ISUPPORTS();
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
nsNSSASN1PrintableItem::GetDisplayValue(PRUnichar * *aValue)
{
  *aValue = ToNewUnicode(mValue);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetDisplayValue(const PRUnichar * aValue)
{
  mValue.Assign(aValue);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::GetTag(PRUint32 *aTag)
{
  *aTag = mTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetTag(PRUint32 aTag)
{
  mTag = aTag;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::GetType(PRUint32 *aType)
{
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetType(PRUint32 aType)
{
  mType = aType;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetData(char *data, PRUint32 len)
{
  if (len > 0) {
    if (mData) {
      if (mLen < len)
        nsMemory::Realloc(mData, len);
    } else {  
      mData = (unsigned char*)nsMemory::Alloc(len);
    }

    if (mData == nsnull)
      return NS_ERROR_FAILURE;
    nsCRT::memcpy(mData, data, len);
  } else if (len == 0) {
    if (mData) {
      nsMemory::Free(mData);
      mData = nsnull;
    }
  } else {
    NS_ASSERTION(0,"Passed in invalid buffer length to SetData");
    return NS_ERROR_FAILURE;
  }
  mLen = len;
  return NS_OK;  
}

NS_IMETHODIMP
nsNSSASN1PrintableItem::GetData(char **outData, PRUint32 *outLen)
{
  NS_ENSURE_ARG_POINTER(outData);
  NS_ENSURE_ARG_POINTER(outLen);

  *outData = (char*)mData;
  *outLen  = mLen;
  return NS_OK;
}

/* attribute wstring displayName; */
NS_IMETHODIMP 
nsNSSASN1PrintableItem::GetDisplayName(PRUnichar * *aDisplayName)
{
  NS_ENSURE_ARG_POINTER(aDisplayName);
  *aDisplayName = ToNewUnicode(mDisplayName);
  return (*aDisplayName) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsNSSASN1PrintableItem::SetDisplayName(const PRUnichar * aDisplayName)
{
  mDisplayName.Assign(aDisplayName);
  return NS_OK;
}

