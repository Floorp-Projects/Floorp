/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

import netscape.plugin.Plugin;

class Simple extends Plugin {

    /*
    ** A plug-in can consist of code written in java as well as
    ** natively. Here's a dummy method.
    */
    public static int fact(int n) {
	if (n == 1)
	    return 1;
	else
	    return n * fact(n-1);
    }

    /*
    ** This instance variable is used to keep track of the number of
    ** times we've called into this plug-in.
    */
    int count;

    /*
    ** This native method will give us a way to print to stdout from java
    ** instead of just the java console.
    */
    native void printToStdout(String msg);

    /*
    ** This is a publically callable new feature that our plug-in is
    ** providing. We can call it from JavaScript, Java, or from native
    ** code.
    */
    public void doit(String text) {
	/* construct a message */
	String msg = "" + (++count) + ". " + text + "\n";
	/* write it to the console */
	System.out.print(msg);
	/* and also write it to stdout */
	printToStdout(msg);
    }
}
