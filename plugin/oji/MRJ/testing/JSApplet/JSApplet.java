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
	JSApplet.java
	
	Tests JSObject.eval.
	
	by Patrick C. Beard.
 */

import java.io.*;
import java.awt.*;
import java.applet.*;
import java.awt.event.*;

import netscape.javascript.JSObject;

public class JSApplet extends Applet {
	TextField text;

	public void init() {
		setLayout(new BorderLayout());	
		add(text = new TextField(), BorderLayout.CENTER);

		Panel panel = new Panel();
		add(panel, BorderLayout.SOUTH);

		// eval button.
		ActionListener evalListener = new ActionListener() {
			JSObject window;
		
			public void actionPerformed(ActionEvent e) {
				if (window == null)
					window = JSObject.getWindow(JSApplet.this);
				Object result = window.eval(text.getText());
				if (result != null)
					System.out.println(result);
				text.selectAll();
			}
		};

		Button evalButton = new Button("eval");
		evalButton.addActionListener(evalListener);
		text.addActionListener(evalListener);
		panel.add(evalButton);

		// clear button.
		ActionListener clearConsoleListener = new ActionListener() {
			public void actionPerformed(ActionEvent e) {
				text.setText("");
			}
		};

		Button clearConsole = new Button("clear");
		clearConsole.addActionListener(clearConsoleListener);
		panel.add(clearConsole);
	}

	public void destroy() {
	}
}
