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

public class SymProgressDetails extends Frame {
	void btnCancel_Clicked(Event event) {


		//{{CONNECTION
		// Disable the Frame
		disable();
		//}}
	}



	public SymProgressDetails() {

		//{{INIT_CONTROLS
		setLayout(null);
		addNotify();
		resize(insets().left + insets().right + 562,insets().top + insets().bottom + 326);
		setFont(new Font("Dialog", Font.BOLD, 14));
		label1 = new java.awt.Label("xxxxxIxxxxxx");
		label1.reshape(insets().left + 12,insets().top + 12,536,36);
		label1.setFont(new Font("Dialog", Font.PLAIN, 14));
		add(label1);
		detailArea = new java.awt.TextArea();
		detailArea.reshape(insets().left + 12,insets().top + 60,536,216);
		add(detailArea);
		btnCancel = new java.awt.Button("xxxCxxx");
		btnCancel.reshape(insets().left + 464,insets().top + 288,84,26);
		add(btnCancel);
		setTitle("Untitled");
		//}}
			//{{INIT_MENUS
		//}}
}

	public boolean handleEvent(Event event) {
		if (event.target == btnCancel && event.id == Event.ACTION_EVENT) {
			btnCancel_Clicked(event);
			return true;
		}
		return super.handleEvent(event);
	}

	//{{DECLARE_CONTROLS
	java.awt.Label label1;
	java.awt.TextArea detailArea;
	java.awt.Button btnCancel;
	//}}
	//{{DECLARE_MENUS
	//}}
}
