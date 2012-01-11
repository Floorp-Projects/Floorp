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
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Jason Voll
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

package org.mozilla.gecko.sync.crypto;



/*
 * All info in these objects should be decoded (i.e. not BaseXX encoded).
 */
public class CryptoInfo {

	private byte[] message;
	private byte[] iv;
	private byte[] hmac;
	private KeyBundle keys;

	/*
	 * Constructor typically used when encrypting
	 */
	public CryptoInfo(byte[] message, KeyBundle keys) {
	    this.setMessage(message);
	    this.setKeys(keys);
	}

	/*
	 * Constructor typically used when decrypting
	 */
	public CryptoInfo(byte[] message, byte[] iv, byte[] hmac, KeyBundle keys) {
	    this.setMessage(message);
	    this.setIV(iv);
	    this.setHMAC(hmac);
	    this.setKeys(keys);
	}

    public byte[] getMessage() {
        return message;
    }

    public void setMessage(byte[] message) {
        this.message = message;
    }

    public byte[] getIV() {
        return iv;
    }

    public void setIV(byte[] iv) {
        this.iv = iv;
    }

    public byte[] getHMAC() {
        return hmac;
    }

    public void setHMAC(byte[] hmac) {
        this.hmac = hmac;
    }

    public KeyBundle getKeys() {
        return keys;
    }

    public void setKeys(KeyBundle keys) {
        this.keys = keys;
    }

}
