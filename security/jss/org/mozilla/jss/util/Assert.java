/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Security Services for Java.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
package org.mozilla.jss.util;

import org.mozilla.jss.util.*;

/**
 * C-style assertions in Java.
 * These methods are only active in debug mode
 * (org.mozilla.jss.Debug.DEBUG==true).
 *
 * @see org.mozilla.jss.util.Debug
 * @see org.mozilla.jss.util.AssertionException
 * @version $Revision: 1.3 $ $Date: 2002/04/19 22:42:05 $
 */
public class Assert {
    /**
     * Assert that a condition is true.  If it is not true, abort by
     * throwing an AssertionException.
     *
     * @param cond The condition that is being tested.
     * @deprecated "assert" is a keyword under JDK 1.4. Use "_assert"
     *      instead.
     */
    public static void assert(boolean cond) {
        if(Debug.DEBUG && !cond) {
            throw new org.mozilla.jss.util.AssertionException(
                "assertion failure!");
        }
    }

    /**
     * Assert that a condition is true. If it is not true, abort by throwing
     * an AssertionException.
     *
     * @param cond The condition that is being tested.
     * @param msg A message describing what is wrong if the condition is false.
     * @deprecated "assert" is a keyword under JDK 1.4. Use "_assert"
     *      instead.
     */
	public static void assert(boolean cond, String msg) {
		if(Debug.DEBUG && !cond) {
			throw new org.mozilla.jss.util.AssertionException(msg);
		}
	}

    /**
     * Assert that a condition is true.  If it is not true, abort by
     * throwing an AssertionException.
     *
     * @param cond The condition that is being tested.
     */
    public static void _assert(boolean cond) {
        if(Debug.DEBUG && !cond) {
            throw new org.mozilla.jss.util.AssertionException(
                "assertion failure!");
        }
    }

    /**
     * Assert that a condition is true. If it is not true, abort by throwing
     * an AssertionException.
     *
     * @param cond The condition that is being tested.
     * @param msg A message describing what is wrong if the condition is false.
     */
	public static void _assert(boolean cond, String msg) {
		if(Debug.DEBUG && !cond) {
			throw new org.mozilla.jss.util.AssertionException(msg);
		}
	}

    /**
     * Throw an AssertionException if this statement is reached.
     *
     * @param msg A message describing what was reached.
     */
    public static void notReached(String msg) {
        if(Debug.DEBUG) {
            throw new AssertionException("should not be reached: " + msg);
        }
    }

    /**
     * Throw an AssertionException if this statement is reached.
     */
    public static void notReached() {
        if(Debug.DEBUG) {
            throw new AssertionException();
        }
    }

    /**
     * Throw an AssertionException because functionlity is not yet implemented.
     *
     * @param msg A message describing what is not implemented.
     */
    public static void notYetImplemented(String msg) {
        if(Debug.DEBUG) {
            throw new AssertionException("not yet implemented: " + msg);
        }
    }
}
