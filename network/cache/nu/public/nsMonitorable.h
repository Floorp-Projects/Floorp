/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* nsMonitorable. This is a cool class too. Its an easy of adding monitors
 * actually PRMonitor behaviour to existing classes. The nsMonitorLocker is 
 * an excursion class that locks the scope in which is declared (on stack). 
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsMonitorable_h__
#define nsMonitorable_h__

//#include "nsISupports.h"

#include "prmon.h"
#include "prlog.h"

class nsMonitorable//: public nsISupports
{

public:
            nsMonitorable();
    virtual ~nsMonitorable();

/*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    PRBool      Lock(void);
    void        Unlock(void);

protected:

    class MonitorLocker
    {
    public:
        MonitorLocker(nsMonitorable* i_pThis);
        ~MonitorLocker();
    private:
        nsMonitorable* m_pMonitorable;
    };

    PRMonitor*          m_pMonitor;

private:
    nsMonitorable(const nsMonitorable& o);
    nsMonitorable& operator=(const nsMonitorable& o);
};

inline
nsMonitorable::nsMonitorable(void):m_pMonitor(PR_NewMonitor())
{
}

inline
nsMonitorable::~nsMonitorable()
{
    if (m_pMonitor)
    {
    PR_DestroyMonitor(m_pMonitor);
    m_pMonitor = 0;
    }
}

inline
PRBool nsMonitorable::Lock(void)
{
    if (!m_pMonitor)
    {
        m_pMonitor = PR_NewMonitor();
        if (!m_pMonitor)
            return PR_FALSE;
    }
    PR_EnterMonitor(m_pMonitor);
    return PR_TRUE;
}


inline
void nsMonitorable::Unlock(void)
{
    PR_ASSERT(m_pMonitor);
    if (m_pMonitor)
        PR_ExitMonitor(m_pMonitor);
}

#endif // nsMonitorable_h__

