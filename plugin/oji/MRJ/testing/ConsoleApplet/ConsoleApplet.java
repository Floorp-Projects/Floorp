/*
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
