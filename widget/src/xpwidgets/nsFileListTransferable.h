/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsFileListTransferable_h__
#define nsFileListTransferable_h__

#include "nsITransferable.h"
#include "nsIFileListTransferable.h"
#include "nsString.h"
#include "nsCOMPtr.h"


class nsISupportsArray;
class nsDataObj;
class nsVoidArray;


/**
 * XP FileListTransferable wrapper
 */

class nsFileListTransferable : public nsIFileListTransferable, public nsITransferable
{

public:
  nsFileListTransferable();
  virtual ~nsFileListTransferable();

  //nsISupports
  NS_DECL_ISUPPORTS
  

  //////////////////////////
  //  nsITransferable
  //////////////////////////
  NS_IMETHOD FlavorsTransferableCanExport ( nsVoidArray** outFlavorList ) ;
  NS_IMETHOD GetTransferDataFlavors(nsVoidArray ** aDataFlavorList);

   // Transferable still owns |aData|. Do not delete it.
  NS_IMETHOD GetTransferData(nsString * aFlavor, void ** aData, PRUint32 * aDataLen);
  NS_IMETHOD GetAnyTransferData(nsString * aFlavor, void ** aData, PRUint32 * aDataLen);
  NS_IMETHOD_(PRBool) IsLargeDataSet();

  //////////////////////////
  // nsIFileListTransferable
  //////////////////////////
  NS_IMETHOD SetFileList(nsVoidArray * aFileList);
  NS_IMETHOD GetFileList(nsVoidArray * aFileList);

  //////////////////////////
  // Getter interface
  //////////////////////////
  NS_IMETHOD FlavorsTransferableCanImport ( nsVoidArray** outFlavorList ) ;
  
  NS_IMETHOD SetTransferData(nsString * aFlavor, void * aData, PRUint32 aDataLen); // Transferable consumes |aData|. Do not delete it.

  NS_IMETHOD AddDataFlavor(nsString * aDataFlavor);
  NS_IMETHOD RemoveDataFlavor(nsString * aDataFlavor);

  NS_IMETHOD SetConverter(nsIFormatConverter * aConverter);
  NS_IMETHOD GetConverter(nsIFormatConverter ** aConverter);


protected:
  void ClearFileList();
  NS_IMETHODIMP CopyFileList(nsVoidArray * aFromFileList,
                             nsVoidArray * aToFileList);

  nsVoidArray * mFileList;
  nsString      mFileListDataFlavor;

};

#endif // nsFileListTransferable_h__
