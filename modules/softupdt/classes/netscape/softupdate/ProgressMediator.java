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
