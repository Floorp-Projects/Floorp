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
	JNIThread.java
	
	Provides a communication channel between native threads and Java threads.
	
	by Patrick C. Beard.
 */

package netscape.oji;

public class JNIThread extends Thread {
	private int fSecureEnv;

	public JNIThread(int secureEnv) {
		super("JNIThread->0x" + Integer.toHexString(secureEnv));
		fSecureEnv = secureEnv;
		setPriority(NORM_PRIORITY);
		// setPriority(MAX_PRIORITY);
		start();
	}
	
	public native void run();
}
