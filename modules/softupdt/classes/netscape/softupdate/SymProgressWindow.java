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
		progress.setFont(new Font("Dialog", Font.PLAIN, 10));
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
