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
	JNIUtils.java
 */

package netscape.oji;

public class JNIUtils {
	/**
	 * Returns a local reference for a passed-in global reference.
	 */
	public static Object NewLocalRef(Object object) {
		return object;
	}
	
	/**
	 * Returns the currently running thread.
	 */
	public static Object GetCurrentThread() {
		return Thread.currentThread();
	}
	
	
	/**
	 * Stub SecurityManager class, to expose access to class loaders.
	 */
	static class StubSecurityManager extends SecurityManager {
		public ClassLoader getCurrentClassLoader() {
			return currentClassLoader();
		}
	}
	
	private static StubSecurityManager stubManager = new StubSecurityManager();
	
	/**
	 * Returns the current class loader.
	 */
	public static Object GetCurrentClassLoader() {
		return stubManager.getCurrentClassLoader();
	}
	
	public static Object GetObjectClassLoader(Object object) {
		return object.getClass().getClassLoader();
	}
}
