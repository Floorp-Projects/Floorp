/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*
 * ProgressWindow
 *
 */
package	netscape.softupdate	;

import java.awt.* ;

class ProgressDetails extends SymProgressDetails
{
    ProgressMediator pm;

    public ProgressDetails(ProgressMediator inPm)
    {
         pm  = inPm;
         label1.setText("");
         btnCancel.setLabel(Strings.getString("s36"));
    }
    
 	protected void
	finalize() throws Throwable {
	    super.finalize();
	    if (pm != null)
			pm.DetailsHidden();
	}

    void btnCancel_Clicked(Event event) {
        hide();
        dispose();
	}

	public synchronized void hide()
	{
	    super.hide();
	    if (pm != null)
	    {
    		pm.DetailsHidden();
	    	pm  = null;
	    }
	}
	//{{DECLARE_CONTROLS
	//}}
	//{{DECLARE_MENUS
	//}}
}
