/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/*
 * ProgressWindow
 *
 */

package	netscape.softupdate	;

import java.awt.* ;

class ProgressWindow extends SymProgressWindow
{
    ProgressMediator pm;

    public ProgressWindow(ProgressMediator inPm)
    {
         pm  = inPm;
         install.setLabel(Strings.getString("s33"));
         cancel.setLabel(Strings.getString("s34"));
         info.setLabel(Strings.getString("s35"));
    }

	void cancel_Clicked(Event event)
	{
		pm.UserCancelled();
	}

	void install_Clicked(Event event)
	{
	    pm.UserApproved();
	}
	
    void info_Clicked(Event event) {
        pm.MoreInfo();
	}

}
