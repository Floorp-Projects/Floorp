/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#ifndef nsFileChooser_h__
#define nsFileChooser_h__

#include "nsIFileChooser.h"
#include "nsFileSpec.h"

class nsFileChooser : public nsIFileChooser {
public:
    nsFileChooser();
    virtual ~nsFileChooser();

    static NS_METHOD
    Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);
    
    // from nsIFileChooser:
    NS_IMETHOD Init();

    NS_IMETHOD ChooseOutputFile(const char* windowTitle,
                                const char* suggestedLeafName);

    NS_IMETHOD ChooseInputFile(const char* title,
                               StandardFilterMask standardFilterMask,
                               const char* extraFilterTitle,
                               const char* extraFilter);

    NS_IMETHOD ChooseDirectory(const char* title);

    NS_IMETHOD GetURLString(char* *result);

    // from nsISupports:
    NS_DECL_ISUPPORTS

protected:
    nsFileSpec mFileSpec;
};

#endif // nsFileChooser_h__
