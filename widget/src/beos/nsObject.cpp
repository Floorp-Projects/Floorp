/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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


#include "nsObject.h"


CList nsObject::s_liveChain;
PRMonitor *nsObject::s_liveChainMutex = PR_NewMonitor();

#ifdef _DEBUG
int32 nsObject::s_nObjects = 0;
#endif

/**
 * constructor
 */
nsObject::nsObject()
{
    //
    // Add the new object the chain of allocated nsObjects
    //
    PR_EnterMonitor(s_liveChainMutex);
    s_liveChain.Append(m_link);
    PR_ExitMonitor(s_liveChainMutex);
#ifdef _DEBUG
    s_nObjects++;
#endif
}


/**
 * destructor
 */
nsObject::~nsObject()
{
#ifdef _DEBUG
    s_nObjects--;
#endif
    //
    // Remove from the chain of allocated nsObjects
    //
    PR_EnterMonitor(s_liveChainMutex);
    m_link.Remove();
    PR_ExitMonitor(s_liveChainMutex);
}


/**
 * static utility. Delete all live objects
 */

void nsObject::DeleteAllObjects(void)
{
    PR_EnterMonitor(s_liveChainMutex);

    while (!s_liveChain.IsEmpty()) {
        nsObject *pnsObject;

        pnsObject = (nsObject*)OBJECT_PTR_FROM_CLIST(nsObject, s_liveChain.next);

        // Remove the event from the chain...
        pnsObject->m_link.Remove();
        delete pnsObject;
    }

    PR_ExitMonitor(s_liveChainMutex);
}

