/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * 		Denis Issoupov <denis@macadamian.com>
 *
 */

#include "nsDragService.h"
#include "nsIServiceManager.h"
#include "nsWidget.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsClipboard.h"
#include "nsMime.h"

#include <qapplication.h>
#include <qclipboard.h>

//----------------------------------------------------------
nsMimeStoreData::nsMimeStoreData(QCString& name, QByteArray& data)
{
  flavorName = name;
  flavorData = data;
}

nsMimeStoreData::nsMimeStoreData(const char* name,void* rawdata,PRInt32 rawlen)
{
  flavorName = name;
  flavorData.assign((char*)rawdata,(unsigned int)rawlen);
}

//----------------------------------------------------------
nsMimeStore::nsMimeStore()
{
  mMimeStore.setAutoDelete(TRUE);     
}

nsMimeStore::~nsMimeStore()
{
}

const char* nsMimeStore::format(int n) const
{
  if (n >= (int)mMimeStore.count())
    return 0;

  // because of const
  QList<nsMimeStoreData> *pMimeStore = (QList<nsMimeStoreData>*)&mMimeStore;  

  nsMimeStoreData* msd;  
  msd = pMimeStore->at((unsigned int)n);
  return msd->flavorName;
}

QByteArray nsMimeStore::encodedData(const char* name) const
{
  QByteArray qba;

  // because of const
  QList<nsMimeStoreData> *pMimeStore = (QList<nsMimeStoreData>*)&mMimeStore;  

  nsMimeStoreData* msd;
  for (msd = pMimeStore->first(); msd != 0; msd = pMimeStore->next()) {
    if (strcmp(name, msd->flavorName) == 0) {
      qba = msd->flavorData;
      return qba;
    }
  }
#ifdef NS_DEBUG
  printf("nsMimeStore::encodedData requested unkown %s\n", name);
#endif
  return qba;
}

PRBool nsMimeStore::AddFlavorData(const char* name, void* data, PRInt32 len)
{
  nsMimeStoreData* msd;
  for (msd = mMimeStore.first(); msd != 0; msd = mMimeStore.next()) {
     if (strcmp(name, msd->flavorName) == 0)
       return PR_FALSE;
  }
  mMimeStore.append(new nsMimeStoreData(name, data, len));

  if (strcmp(name, kUnicodeMime) == 0) {
    // let's look if text/plain is not present yet
    for (msd = mMimeStore.first(); msd != 0; msd = mMimeStore.next()) {
      if (strcmp("text/plain", msd->flavorName) == 0)
         return PR_TRUE;
    }
    // if we find text/unicode, also advertise text/plain (which we will convert
    // on our own in nsDataObj::GetText().
    len /= 2;
    QChar* qch = new QChar[len];
    for (PRInt32 c = 0; c < len; c++)
       qch[c] = (QChar)((PRUnichar*)data)[c];

    QString ascii(qch, len);
    delete qch;
    char *as = new char[len];
    for (PRInt32 c = 0; c < len; c++)
      as[c] = ((const char*)ascii)[c];

    // let's text/plain be first for stupid programms 
    mMimeStore.insert(0, new nsMimeStoreData("text/plain", as, len));
  }
  return PR_TRUE;
}

//----------------------------------------------------------
nsDragObject::nsDragObject(nsMimeStore* mimeStore,QWidget* dragSource,
                           const char* name)
  : QDragObject(dragSource, name)
{
  if (!mimeStore)
    NS_ASSERTION(PR_TRUE, "Invalid  pointer.");

  mMimeStore = mimeStore;
}

nsDragObject::~nsDragObject()
{
  delete mMimeStore;
}

const char* nsDragObject::format(int i) const
{
  if (i >= (int)mMimeStore->count())
    return 0;

  const char* frm = mMimeStore->format(i);
#ifdef NS_DEBUG
  printf("nsDragObject::format i=%i %s\n",i, frm);
#endif  
  return frm;
}

QByteArray nsDragObject::encodedData(const char* frm) const
{
#ifdef NS_DEBUG
  printf("nsDragObject::encodedData %s\n",frm);
#endif
  return mMimeStore->encodedData(frm);
}
