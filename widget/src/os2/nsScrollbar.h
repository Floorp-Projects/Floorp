/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsScrollbar_h__
#define nsScrollbar_h__

#include "nsWindow.h"

#include "nsIScrollbar.h"

/**
 * WC_SCROLLBAR wrapper, for NS_HORZSCROLLBAR_CID & NS_VERTSCROLLBAR_CID
 */

class nsScrollbar : public nsWindow,
                    public nsIScrollbar
{

public:
                            nsScrollbar(PRBool aIsVertical);
    virtual                 ~nsScrollbar();

      // nsISupports
    NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
    NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
    NS_IMETHOD_(nsrefcnt) Release(void);          

    // nsIScrollBar implementation
    NS_IMETHOD SetMaxRange(PRUint32 aEndRange);
    NS_IMETHOD GetMaxRange(PRUint32& aMaxRange);
    NS_IMETHOD SetPosition(PRUint32 aPos);
    NS_IMETHOD GetPosition(PRUint32& aPos);
    NS_IMETHOD SetThumbSize(PRUint32 aSize);
    NS_IMETHOD GetThumbSize(PRUint32& aSize);
    NS_IMETHOD SetLineIncrement(PRUint32 aSize);
    NS_IMETHOD GetLineIncrement(PRUint32& aSize);
    NS_IMETHOD SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                               PRUint32 aPosition, PRUint32 aLineIncrement);

    virtual  PRBool   OnScroll( ULONG msgid, MPARAM mp1, MPARAM mp2);
    NS_IMETHOD        GetBounds(nsRect &aRect);

 protected:
    // Creation hooks
    virtual  PCSZ     WindowClass();
    virtual  ULONG    WindowStyle();
    virtual PRBool OnPaint();

 private:
    void     SetThumbRange( PRUint32 aEndRange, PRUint32 aSize);
    ULONG    mPositionFlag;
    int      mLineIncrement;
    float    mScaleFactor;
    PRUint32 mThumbSize;     // cache XP thumbsize
    PRUint32 mMaxRange;      // cache XP maxrange
};

#endif
