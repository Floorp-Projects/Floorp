/*
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
	TextArea text;

	public ConsoleApplet() {
		setLayout(new BorderLayout());	
		add(text = new TextArea(), BorderLayout.CENTER);

		ActionListener dumpThreadsListener =
			new ActionListener() {
				public void actionPerformed(ActionEvent e) {
					dumpThreads();
				}
			};

		// Create a dump threads button.
		Button dumpThreads = new Button("Dump Threads");
		dumpThreads.addActionListener(dumpThreadsListener);
		add(dumpThreads, BorderLayout.SOUTH);
	}
	
	public void init() {
		Console.init(text);
	}

	public void destroy() {
		Console.dispose();
	}
	
	public void dumpThreads() {
		System.out.println("Dumping threads...");
	}
}
