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
 *      Denis Issoupov <denis@macadamian.com>
 *
 */
#ifndef nsMime_h__
#define nsMime_h__

#include "nsITransferable.h"
#include "nsCOMPtr.h"

#include <qlist.h> 
#include <qcstring.h> 
#include <qmime.h> 
#include <qwidget.h> 
#include <qdragobject.h> 

class nsMimeStoreData
{
public:
   nsMimeStoreData(QCString& name, QByteArray& data);
   nsMimeStoreData(const char* name, void* rawdata, PRInt32 rawlen);

   QCString    flavorName;
   QByteArray  flavorData;
};

class nsMimeStore: public QMimeSource 
{
public:
    nsMimeStore();
    virtual ~nsMimeStore();

    virtual const char* format(int n = 0) const ;
    virtual QByteArray encodedData(const char*) const;

    PRBool AddFlavorData(const char* name, void* data, PRInt32 len);

    PRUint32  count();

protected:
    QList<nsMimeStoreData> mMimeStore;
    nsMimeStoreData*	   at(int n);
};

inline PRUint32 nsMimeStore::count() { return mMimeStore.count(); }

//----------------------------------------------------------
class nsDragObject : public QDragObject
{
    Q_OBJECT
public:
    nsDragObject(nsMimeStore* mimeStore,QWidget* dragSource = 0,
                 const char* name = 0);
    ~nsDragObject();

    const char* format(int i) const;
    virtual QByteArray encodedData(const char*) const;

protected:
    nsMimeStore* mMimeStore;
};

#endif
