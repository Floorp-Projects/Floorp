/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* Implementation of xptiTypelibGuts. */

#include "xptiprivate.h"

MOZ_DECL_CTOR_COUNTER(xptiTypelibGuts)

xptiTypelibGuts::xptiTypelibGuts(XPTHeader* aHeader)
     :  mHeader(aHeader), 
        mInfoArray(nsnull)
{
    MOZ_COUNT_CTOR(xptiTypelibGuts);

    NS_ASSERTION(mHeader, "bad param");

    if(mHeader->num_interfaces)
    {
        mInfoArray =  new xptiInterfaceInfo*[GetInfoCount()];
        if(mInfoArray)
            memset(mInfoArray, 0, sizeof(xptiInterfaceInfo*) * GetInfoCount());
        else    
            mHeader = nsnull;
    }        
}

xptiTypelibGuts::~xptiTypelibGuts()
{
    MOZ_COUNT_DTOR(xptiTypelibGuts);

    if(mHeader && mInfoArray)
        for(PRUint16 i = 0; i < GetInfoCount(); ++i)
            NS_IF_RELEASE(mInfoArray[i]);
    delete [] mInfoArray;
}

xptiTypelibGuts* 
xptiTypelibGuts::Clone()
{
    xptiTypelibGuts* clone = new xptiTypelibGuts(mHeader);
    if(clone && clone->IsValid())
        for(PRUint16 i = 0; i < GetInfoCount(); ++i)
            clone->SetInfoAt(i, GetInfoAtNoAddRef(i));
    return clone;
}

nsresult
xptiTypelibGuts::GetInfoAt(PRUint16 i, xptiInterfaceInfo** ptr)
{
    NS_ASSERTION(mHeader,"bad state!");
    NS_ASSERTION(mInfoArray,"bad state!");
    NS_ASSERTION(i < GetInfoCount(),"bad param!");
    NS_ASSERTION(ptr,"bad param!");

    NS_IF_ADDREF(*ptr = mInfoArray[i]);
    return NS_OK;
}        

xptiInterfaceInfo* 
xptiTypelibGuts::GetInfoAtNoAddRef(PRUint16 i)
{
    NS_ASSERTION(mHeader,"bad state!");
    NS_ASSERTION(mInfoArray,"bad state!");
    NS_ASSERTION(i < GetInfoCount(),"bad param!");

    return mInfoArray[i];
}        


nsresult
xptiTypelibGuts::SetInfoAt(PRUint16 i, xptiInterfaceInfo* ptr)
{
    NS_ASSERTION(mHeader,"bad state!");
    NS_ASSERTION(mInfoArray,"bad state!");
    NS_ASSERTION(i < GetInfoCount(),"bad param!");

    NS_IF_ADDREF(ptr);
    NS_IF_RELEASE(mInfoArray[i]);
    mInfoArray[i] = ptr;
    return NS_OK;
}        
