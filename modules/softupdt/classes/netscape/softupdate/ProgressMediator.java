/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
/*
 * ProgressMediator.java    4/16/97
 * @author atotic
 */
package	netscape.softupdate;

/**
 * Relays progress messages, and displays status
 * in the progress window
 */

class ProgressMediator    {

    private ProgressWindow	progress;	// Progress	window, guard against null when derefing
    private ProgressDetails details;   // Details window
    private SoftwareUpdate su;  // Object whose progress we are monitoring

    ProgressMediator( SoftwareUpdate inSu)
    {
        su = inSu;
        progress = null;
        details = null;
    }
	protected void
	finalize() throws Throwable
	{
	    CleanUp();
	}

    /* Hide and dereference all the windows */
    private void
    CleanUp()
    {
        if ( progress != null )
        {
            //ProgressWindow p2 = progress;
            progress.hide();
            progress.dispose();
        }
        if ( details != null)
             details.btnCancel_Clicked(null);
        progress = null;
        details = null;
        su = null;
    }

    private void
    ShowProgress()
    {
        if ( su.GetSilent())
            return;
        if ( progress == null )
 		    progress = new ProgressWindow(this);
	    
	    progress.toFront();
	    progress.show();
        progress.toFront();
    }
    
    /* Shows the details window */
    private void
    ShowDetails()
    {
        if ( su.GetSilent())
            return;

        if (details == null)
        {
            details = new ProgressDetails(this);
            // Text
            details.setTitle( Strings.details_WinTitle() + su.GetUserPackageName() );
            details.label1.setText(Strings.details_Explain(su.GetUserPackageName()));
            // Fill in the current progress of the install
            java.util.Enumeration e = su.GetInstallQueue();
            while (	e.hasMoreElements() )
			{
				InstallObject ie = (InstallObject)e.nextElement();
	            details.detailArea.appendText(ie.toString() + "\n");
			}
            details.show();
           
        }
        details.toFront();
    }
    

    protected void
    DetailsHidden()
    {
        details = null;
    }

/* ProgressWindow API */

    protected void
    UserCancelled()
    {
        if (su != null)
            su.UserCancelled();
    }

    protected void
    UserApproved()
    {
        if (su != null)
            su.UserApproved();
    }
    
    protected void
    MoreInfo()
    {
        ShowDetails();
    }

/* SofwareUpdate API bellow */

    protected void
    StartInstall()
    {
        ShowProgress();
        if (progress != null)
        {
           // Set up the text
     	    progress.setTitle(Strings.progress_Title() + su.GetUserPackageName());
     	    progress.status.setText(Strings.progress_GettingReady() + su.GetUserPackageName());
     	    progress.progress.setText("");
        // Disable/enable controls
            progress.install.disable();
        }
    }
    
    protected void
    ConfirmWithUser()
    {
        ShowProgress();
        if ( progress != null )
        {
     	    progress.progress.setText("");
            progress.status.setText(Strings.progress_ReadyToInstall1() + 
                                    su.GetUserPackageName());
            progress.install.enable();           
        }
        else    // Give user approval right away if there is no UI
            UserApproved();
    }

    /* flash item on visible progress window */
    protected void
    SetStatus(InstallObject install)
    {
        if (progress != null)
            progress.progress.setText( install.toString() );
    }

    /* Add it to the details list if showing */
    protected void
    ScheduleForInstall(InstallObject install)
    {
        String s = install.toString();
        if (details != null)
            details.detailArea.appendText(s + "\n");
    }

    /* SmartUpdate is done */
    void
    Complete()
    {
        CleanUp();
    }
}
