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

/*
	ConsoleApplet.java
	
	Simple Java console for MRJ.
	
	A trusted applet that provides simple console services for the MRJ plugin.
	The applet's classes must be installed in the MRJClasses folder, because
	it alters the System.in/out/err streams.
	
	by Patrick C. Beard.
 */

package netscape.console;

import java.io.*;
import java.awt.*;
import java.applet.*;
import java.awt.event.*;

public class ConsoleApplet extends Applet {
	TextArea console;

	public ConsoleApplet() {
		setLayout(new BorderLayout());	
		add(console = new TextArea(), BorderLayout.CENTER);

		Panel panel = new Panel();
		add(panel, BorderLayout.SOUTH);

		// clear console button.
		ActionListener clearConsoleListener = new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				console.setText("");
			}
		};

		Button clearConsole = new Button("Clear");
		clearConsole.addActionListener(clearConsoleListener);
		panel.add(clearConsole);

		// dump threads button.
		ActionListener dumpThreadsListener = new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				dumpThreads();
			}
		};

		Button dumpThreads = new Button("Dump Threads");
		dumpThreads.addActionListener(dumpThreadsListener);
		panel.add(dumpThreads);
	}
	
	public void init() {
		Console.init(console);
	}

	public void destroy() {
		Console.dispose();
	}
	
	public void dumpThreads() {
		System.out.println("Dumping threads...");
	}
}
