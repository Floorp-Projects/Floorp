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

package com.netscape.jss.util;

/**
 * Represents a password callback, which is called to login to the key
 * database and to PKCS #11 tokens.
 * <p>The simplest implementation of a PasswordCallback is a Password object.
 * 
 * @see com.netscape.jss.util.Password
 * @see com.netscape.jss.util.NullPasswordCallback
 * @see com.netscape.jss.util.ConsolePasswordCallback
 * @see com.netscape.jss.CryptoManager#setPasswordCallback
 */
public interface PasswordCallback {

    /**
     * This exception is thrown if the <code>PasswordCallback</code>
     * wants to stop guessing passwords.
     */
    public static class GiveUpException extends Exception {
        public GiveUpException() { super(); }
        public GiveUpException(String mesg) { super(mesg); }
    }

    /**
     * Supplies a password. This is called on the first attempt; if it
     * returns the wrong password, <code>getPasswordAgain</code> will
     * be called on subsequent attempts. 
     *
	 * @param info Information about the token that is being logged into.
	 * @return The password.  This password object is owned	by and will
     *      be cleared by the caller.
     * @exception GiveUpException If the callback does not want to supply
     *  a password.
     */
	public Password getPasswordFirstAttempt(PasswordCallbackInfo info)
		throws GiveUpException;

    /**
     * Tries supplying a password again. This callback will be called if
	 * the first callback returned an invalid password.  It will be called
     * repeatedly until it returns a correct password, or it gives up by
     * throwing a <code>GiveUpException</code>.
     *
	 * @param info Information about the token that is being logged into.
	 * @returns The password.  This password object is owned by and will
     *      be cleared by the caller.
     * @exception GiveUpException If the callback does not want to supply
     *  a password.  This may often be the case if the first attempt failed.
     */
    public Password getPasswordAgain(PasswordCallbackInfo info)
        throws GiveUpException;
}
