/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Security Services for Java.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.jss.ssl;

import java.io.IOException;
import java.io.InterruptedIOException;
import java.net.*;
import java.util.*;

/**
 * This interface is what you should implement if you want to
 * be able to decide whether or not you want to approve the peer's cert,
 * instead of having NSS do that.
 */
public class TestClientCertificateSelectionCallback
  	  implements SSLClientCertificateSelectionCallback {

	/**
	 *  this method will be called form the native callback code
	 *  when a certificate is requested. You must return a String
	 *  which is the nickname of the certificate you wish to present.
	 *
	 *  @param nicknames A Vector of Strings. These strings are an
	 *    aid to the user to select the correct nickname. This list is
	 *    made from the list of all certs which are valid, match the
	 *    CA's trusted by the server, and which you have the private
	 *    key of. If nicknames.length is 0, you should present an
	 *    error to the user saying 'you do not have any unexpired
	 *    certificates'.
	 *  @return You must return the nickname of the certificate you
	 *    wish to use. You can return null if you do not wish to send
     *    a certificate.
	 */
	public String select(Vector nicknames) {
		Enumeration e = nicknames.elements();
		String s="",first=null;

		System.out.println("in TestClientCertificateSelectionCallback.select()  "+s);
		while (e.hasMoreElements()) {
			s = (String)e.nextElement();
			if (first == null) {
				first = s;
			}
			System.out.println("  "+s);
		}
		return first;

	}

} 


