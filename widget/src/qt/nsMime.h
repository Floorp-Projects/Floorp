/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Lars Knoll <knoll@kde.org>
 *      Zack Rusin <zack@kde.org>
 *      Denis Issoupov <denis@macadamian.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsMime_h__
#define nsMime_h__

#include "nsITransferable.h"
#include "nsCOMPtr.h"

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
    PRBool ContainsFlavor(const char* name);
    PRUint32  count();

protected:
    mutable QPtrList<nsMimeStoreData> mMimeStore;
    nsMimeStoreData*       at(int n);
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
