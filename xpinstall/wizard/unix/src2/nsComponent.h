/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _NS_COMPONENT_H_
#define _NS_COMPONENT_H_

#include "XIDefines.h"
#include "XIErrors.h"
#include <malloc.h>

#include "nsComponentList.h"

class nsComponent
{
public:
    nsComponent();
    ~nsComponent();

    nsComponent *   Duplicate();

/*--------------------------------------------------------------*
 *   Accessors/Mutators
 *--------------------------------------------------------------*/
    int             SetDescShort(char *aDescShort);
    char *          GetDescShort();
    int             SetDescLong(char *aDescLong);
    char *          GetDescLong();
    int             SetArchive(char *aAcrhive);
    char *          GetArchive();
    int             SetSize(int aSize);
    int             GetSize();
    int             SetURL(char *aURL, int aIndex);
    char *          GetURL(int aIndex);
    int             AddDependency(nsComponent *aDependent);
    int             RemoveDependency(nsComponent *aIndependent);
    nsComponentList *GetDependencies();
    int             SetSelected();
    int             SetUnselected();
    int             IsSelected();
    int             SetInvisible();
    int             SetVisible();
    int             IsInvisible(); 
    int             SetLaunchApp();
    int             SetDontLaunchApp();
    int             IsLaunchApp();
    int             SetNext(nsComponent *aComponent);
    int             InitNext();
    nsComponent     *GetNext();
    int             SetIndex(int aIndex);
    int             GetIndex();
    int             AddRef();
    int             Release();
    int             InitRefCount();
    
/*---------------------------------------------------------------*
 *   Attributes
 *---------------------------------------------------------------*/
    enum 
    {
        NO_ATTR     = 0x00000000,
        SELECTED    = 0x00000001,
        INVISIBLE   = 0x00000010,
        LAUNCHAPP   = 0x00000100
    };

private:
    char            *mDescShort;
    char            *mDescLong;
    char            *mArchive;
    int             mSize;
    char            *mURL[MAX_URLS];
    nsComponentList *mDependencies;
    int             mAttributes;
    nsComponent     *mNext;
    int             mIndex;
    int             mRefCount;
};

#endif /* _NS_COMPONENT_H_ */
