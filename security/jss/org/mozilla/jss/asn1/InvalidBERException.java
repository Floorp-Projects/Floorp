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
package org.mozilla.jss.asn1;

import java.util.Vector;

/**
 * An exception thrown when BER decoding fails.
 */
public class InvalidBERException extends java.lang.Exception {

    private InvalidBERException child=null;
    private Vector mesgList = new Vector();

    public InvalidBERException(String mesg) {
        super(mesg);
    }

    public void append(String mesg) {
        mesgList.addElement(mesg);
    }

    public InvalidBERException(InvalidBERException e, String mesg) {
        super(mesg);
        child = e;
    }

    /**
     * Prints out the exception class and error message, including
     * all the nested exceptions.
     */
    private void appendMessages(StringBuffer sb) {
        int numMessages = mesgList.size();
        for( int i=numMessages-1; i >= 0; --i ) {
            sb.append(mesgList.elementAt(i));
            sb.append(" >> ");
        }
        sb.append(getMessage());
    }

    public String toString() {
        StringBuffer sb = new StringBuffer();
        sb.append( this.getClass().getName() );
        sb.append(": ");
        appendMessages(sb);
        return sb.toString();
    }

    public String toStringNested() {
        StringBuffer sb = new StringBuffer();
        appendMessages(sb);
        if( child != null ) {
            sb.append(" >> ");
            sb.append( child.toStringNested() );
        }
        return sb.toString();
    }
        
    public static class EOF extends InvalidBERException {
        public EOF() {
            super("Unexpected end-of-file encountered");
        }
    }

	public static class InvalidChar extends InvalidBERException {
		public InvalidChar(byte b, int offset) {
			super("Invalid character ("+b+") encountered at offset "+offset);
		}
		public InvalidChar(char c, int offset) {
			super("Invalid character ("+c+") encountered at offset"+offset);
		}
	}
}
