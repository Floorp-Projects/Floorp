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
    A basic extension of the java.awt.Window class
 */

package netscape.softupdate;

import java.awt.*;

public class SymProgressWindow extends Frame {
	void info_Clicked(Event event) {
	}

	void cancel_Clicked(Event event) {
	}

	void install_Clicked(Event event) {
	}

	public SymProgressWindow() {

		//{{INIT_CONTROLS
		setLayout(null);
		setResizable(false);
	    addNotify();
		resize(insets().left + insets().right + 497,insets().top + insets().bottom + 144);
		setFont(new Font("Dialog", Font.BOLD, 12));
		status = new java.awt.Label("xxxxxxxxxxxxx");
		status.reshape(insets().left + 12,insets().top + 12,472,36);
		status.setFont(new Font("Dialog", Font.BOLD, 12));
		add(status);
		progress = new java.awt.Label("xxxxxxxxxxx");
		progress.reshape(insets().left + 12,insets().top + 48,472,40);
		progress.setFont(new Font("Courier", Font.PLAIN, 14));
		add(progress);
		install = new java.awt.Button("xxxxxIxxx");
		install.reshape(insets().left + 12,insets().top + 96,108,32);
		add(install);
		cancel = new java.awt.Button("xxxxxCxxx");
		cancel.reshape(insets().left + 132,insets().top + 96,108,32);
		add(cancel);
		info = new java.awt.Button("xxxxxMxxxxx");
		info.reshape(insets().left + 376,insets().top + 96,108,32);
		add(info);
		setTitle("Untitled");
		//}}
			//{{INIT_MENUS
		//}}
}

	public boolean handleEvent(Event event) {
		if (event.target == install && event.id == Event.ACTION_EVENT) {
			install_Clicked(event);
			return true;
		}
		if (event.target == cancel && event.id == Event.ACTION_EVENT) {
			cancel_Clicked(event);
			return true;
		}
		if (event.target == info && event.id == Event.ACTION_EVENT) {
			info_Clicked(event);
			return true;
		}
		return super.handleEvent(event);
	}

	//{{DECLARE_CONTROLS
	java.awt.Label status;
	java.awt.Label progress;
	java.awt.Button install;
	java.awt.Button cancel;
	java.awt.Button info;
	//}}
	//{{DECLARE_MENUS
	//}}
}
