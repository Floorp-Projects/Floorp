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

import java.io.CharConversionException;

/**
 * Stores a password.  <code>clear</code> should be
 * called when the password is no longer needed so that the sensitive
 * information is not left in memory.
 * <p>A <code>Password</code> can be used as a hard-coded
 * <code>PasswordCallback</code>.
 * @see PasswordCallback
 */
public class Password implements PasswordCallback, Cloneable,
        java.io.Serializable
    {

    /**
     * Don't use this if you aren't Password.
     */
    private Password() {
        cleared = true;
    }

    /**
     * Creates a Password from a char array, then wipes the char array.
     * @param pw A char[] containing the password.  This array will be
     *      cleared (set to zeroes) by the constructor.
     */
    public Password(char[] pw) {
        int i;
        int length = pw.length;

        cleared = false;

        password = new char[length];
        for(i=0; i < length; i++) {
            password[i] = pw[i];
            pw[i] = 0;
        }
    }

    /**
     * An implementation of
     * <code>PasswordCallback.getPasswordFirstAttempt</code>.  This allows
     * a <code>Password</code> object to be treated as a
     * <code>PasswordCallback</code>.  This method simply returns a clone
     * of the password.
     *
     * @returns A copy of the password.  The caller is responsible for
     *  clearing this copy.
     */
    public synchronized Password
	getPasswordFirstAttempt(PasswordCallbackInfo info)
        throws PasswordCallback.GiveUpException {
			if(cleared) {
				throw new PasswordCallback.GiveUpException();
			}
            return (Password)this.clone();
    }

	/**
	 * Compares this password to another and returns true if they
	 * 	are the same.
	 */
	public synchronized boolean
	equals(Object obj) {
		if(obj == null || !(obj instanceof Password)) {
			return false;
		}
		Password pw = (Password)obj;
		if( pw.password==null || password==null) {
			return false;
		}
		if( pw.password.length != password.length ) {
			return false;
		}
		for(int i=0; i < password.length; i++) {
			if(pw.password[i] != password[i]) {
				return false;
			}
		}
		return true;
	}

    /**
     * An implementation of <code>PasswordCallback.getPasswordAgain</code>.
     * This allows a <code>Password</code> object to be used as a
     * <code>PasswordCallback</code>.  This method is only called after
     * a call to <code>getPasswordFirstAttempt</code> returned the wrong
     * password.  This means the password is incorrect and there's no
     * sense returning it again, so a <code>GiveUpException</code> is thrown.
     */
    public synchronized Password
	getPasswordAgain(PasswordCallbackInfo info)
        throws PasswordCallback.GiveUpException {
            throw new PasswordCallback.GiveUpException();
    }

    /**
     * Returns the char array underlying this password. It must not be
     * modified in any way.
     */
    public synchronized char[] getChars() {
        return password;
    }

    /**
     * Returns a char array that is a copy of the password.
     * The caller is responsible for wiping the returned array,
     * for example using <code>wipeChars</code>.
     */
    public synchronized char[] getCharCopy() {
        return (char[]) password.clone();
    }

    /**
     * Returns a null-terminated byte array that is the byte-encoding of
     * this password.
     * The returned array is a copy of the password.
     * The caller is responsible for wiping the returned array,
     * for example using <code>wipeChars</code>.
     */
    synchronized byte[] getByteCopy() {
        return charToByte( (char[]) password.clone() );
    }

    /**
     * Clears the password so that sensitive data is no longer present
     * in memory. This should be called as soon as the password is no
     * longer needed.
     */
    public synchronized void clear() {
        int i;
        int len = password.length;

        for(i=0; i < len; i++) {
            password[i] = 0;
        }
        cleared = true;
    }

    /**
     * Clones the password.  The resulting clone will be completely independent
     * of the parent, which means it will have to be separately cleared.
     */
    public synchronized Object clone() {
        Password dolly = new Password();

        dolly.password = (char[]) password.clone();
        dolly.cleared = cleared;
        return dolly;
    }
          

    /**
     * The finalizer clears the sensitive information before releasing
     * it to the garbage collector, but it should have been cleared manually
     * before this point anyway.
     */
    protected void finalize() throws Throwable {
        if(Debug.DEBUG && !cleared) {
            System.err.println("ERROR: Password was garbage collected before"+
                " it was cleared.");
        }
        clear();
    }

	/**
	 * Converts a char array to a null-terminated byte array using a standard
	 * encoding, which is currently UTF8. The caller is responsible for
	 * clearing the copy (with <code>wipeBytes</code>, for example).
	 *
	 * @param charArray A character array, which should not be null. It will
	 *		be wiped with zeroes.
	 * @returns A copy of the charArray, converted from Unicode to UTF8. It
	 * 	is the responsibility of the caller to clear the output byte array;
	 *	<code>wipeBytes</code> is ideal for this purpose.
	 * @see Password#wipeBytes
	 */
	public static byte[] charToByte(char[] charArray)
	{
		byte[] byteArray;
		Assert._assert(charArray != null);
		try {
			byteArray = UTF8Converter.UnicodeToUTF8NullTerm(charArray);
		} catch(CharConversionException e) {
			Assert.notReached("Password could not be converted from"
				+" Unicode");
			byteArray = new byte[] {0};
		} finally {
			wipeChars(charArray);
		}
		return byteArray;
	}

	/**
	 * Wipes a byte array by setting all its elements to zero.
     * <code>null</code> must not be passed in.
	 */
	public static void wipeBytes(byte[] byteArray) {
		Assert._assert(byteArray != null);
		UTF8Converter.wipeBytes(byteArray);
	}

	/**
	 * Wipes a char array by setting all its elements to zero.
     * <code>null</code> must not be passed in.
	 */
	public static void wipeChars(char[] charArray) {
		int i;
		Assert._assert(charArray != null);
		for(i=0; i < charArray.length; i++) {
			charArray[i] = 0;
		}
	}

	/**
	 * Reads a password from the console with echo disabled. This is a blocking
	 * call which will return after the user types a newline.
     * It only works with ASCII password characters.
     * The call is synchronized because it alters terminal settings in
     * a way that is not thread-safe.
	 *
     * @exception org.mozilla.jss.util.PasswordCallback.GiveUpException
     *      If the user enters no password (just hits
     *      <code>&lt;enter&gt;</code>).
	 * @return The password the user entered at the command line.
 	 */
	public synchronized static native Password readPasswordFromConsole()
        throws PasswordCallback.GiveUpException;
        
    // The password, stored as a char[] so we can clear it.  Passwords
    // should never be stored in Strings because Strings can't be cleared.
    private char[] password;

    // true if the char[] has been cleared of sensitive information
    private boolean cleared;
}
