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
		detailArea.setFont(new Font("Dialog", Font.PLAIN, 10));
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
