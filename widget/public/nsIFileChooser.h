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

#ifndef nsIFileChooser_h__
#define nsIFileChooser_h__

#include "nsISupports.h"
#include "nscore.h" // for NS_WIDGET

#define NS_IFILECHOOSER_IID                          \
{ /* b1ec7fc0-17bf-11d3-9337-00104ba0fd40 */         \
    0xb1ec7fc0,                                      \
    0x17bf,                                          \
    0x11d3,                                          \
    {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_FILECHOOSER_CID                           \
{ /* b7a15700-17bf-11d3-9337-00104ba0fd40 */         \
    0xb7a15700,                                      \
    0x17bf,                                          \
    0x11d3,                                          \
    {0x93, 0x37, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIFileChooser : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFILECHOOSER_IID);

    NS_IMETHOD Init() = 0;

    // The "choose" functions present the file picker UI in order to set the
    // value of the file spec.
    NS_IMETHOD ChooseOutputFile(const char* windowTitle,
                                const char* suggestedLeafName) = 0;

    // The mask for standard filters is given as follows:
    enum StandardFilterMask {
        eAllReadable     = (1<<0), 
        eHTMLFiles       = (1<<1),
        eXMLFiles        = (1<<2),
        eImageFiles      = (1<<3),
        eAllFiles        = (1<<4),
	
        // Mask containing all the above default filters
        eAllStandardFilters = (eAllReadable
                               | eHTMLFiles
                               | eXMLFiles
                               | eImageFiles
                               | eAllFiles),
	
        // The "extra filter" bit should be set if the "extra filter"
        // is passed in to chooseInputFile.
        eExtraFilter     = (1<<31)
    };

    enum { kNumStandardFilters = 5 };

    NS_IMETHOD ChooseInputFile(const char* title,
                               StandardFilterMask standardFilterMask,
                               const char* extraFilterTitle,
                               const char* extraFilter) = 0;

    NS_IMETHOD ChooseDirectory(const char* title) = 0;

    NS_IMETHOD GetURLString(char* *result) = 0;
};

extern NS_WIDGET
nsresult NS_NewFileChooser(nsIFileChooser* *result);

#endif // nsIFileChooser_h__
