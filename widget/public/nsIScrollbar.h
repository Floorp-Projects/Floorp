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

#ifndef nsIScrollbar_h__
#define nsIScrollbar_h__

#include "nsIWidget.h"

// {18032AD2-B265-11d1-AA2A-000000000000}
#define NS_ISCROLLBAR_IID     \
{ 0x18032ad2, 0xb265, 0x11d1, \
{ 0xaa, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }


/**
 *
 * Scrollbar, converts mouse input into values that can be used
 * to shift the contents of a window.
 *
 */


class nsIScrollbar : public nsIWidget
{
public:
    
    /**
     * Set the scrollbar range
     * @param aEndRange set range for scrollbar from 0 to aEndRange
     *
     */
    virtual void SetMaxRange(PRUint32 aEndRange) = 0;
    
    /**
     * Get the scrollbar range
     * @return the upper end of the scrollbar range
     */
    virtual PRUint32 GetMaxRange() = 0;

    /**
     * Set the thumb position. 
     * @param aPos a value between (startRange) and (endRange - thumbSize)
     *
     */
    virtual void SetPosition(PRUint32 aPos) = 0;

    /**
     * Get the thumb position. 
     * @return a value between (startRange) and (endRange - thumbSize)
     *
     */
    virtual PRUint32 GetPosition() = 0;

    /**
     * Set the thumb size. 
     * @param aSize size of the thumb. Must be a value between 
     *        startRange and endRange
     */
    virtual void SetThumbSize(PRUint32 aSize) = 0;
    
    /**
     * Get the thumb size. 
     * @return size of the thumb. The value is between 
     *          startRange and endRange
     */
    virtual PRUint32 GetThumbSize() = 0;

    /**
     * Set the line increment.
     * @param aSize size of the line increment. The value must
     *        be between startRange and endRange
     */
    virtual void SetLineIncrement(PRUint32 aSize) = 0;
  
    /**
     * Get the line increment.
     * @return size of the line increment. The value is
     *          between startRange and endRange
     */
    virtual PRUint32 GetLineIncrement() = 0;

    /**
     * Set all scrollbar parameters at once
     * @param aMaxRange set range for scrollbar from 0 to aMaxRange
     * @param aThumbSize size of the thumb. Must be a value between 
     *        startRange and endRange
     * @param aPosition a value between (startRange) and (endRange - thumbSize)
     * @param aLineIncrement size of the line increment. The value must
     *        be between startRange and endRange
     */
    virtual void SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                               PRUint32 aPosition, PRUint32 aLineIncrement) = 0;
};

#endif // nsIScrollbar_h__
