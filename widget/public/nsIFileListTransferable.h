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

#ifndef nsIFileListTransferable_h__
#define nsIFileListTransferable_h__

#include "nsISupports.h"
#include "nsString.h"

class nsVoidArray;

#define FileListMime "filelist"

// {E93E73B1-0197-11d3-96D4-0060B0FB9956}
#define NS_IFILELISTTRANSFERABLE_IID      \
{ 0xe93e73b1, 0x197, 0x11d3, { 0x96, 0xd4, 0x0, 0x60, 0xb0, 0xfb, 0x99, 0x56 } }

class nsIFileListTransferable : public nsISupports {

  public:

    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFILELISTTRANSFERABLE_IID)

  /**
    * Copies the list of nsFileSpecs items from aFileList to the internal data member
    *
    */
    NS_IMETHOD SetFileList(nsVoidArray * aFileList) = 0;

  /**
    * Copies the list of nsFileSpecs items from the internal data member to the 
    * aFileList nsVoidArray
    *
    */
    NS_IMETHOD GetFileList(nsVoidArray * aFileList) = 0;

};

#endif
