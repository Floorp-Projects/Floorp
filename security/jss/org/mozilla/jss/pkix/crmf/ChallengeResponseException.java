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
package org.mozilla.jss.pkix.crmf;

/**
 * An exception thrown when challenge response pop is requested.
 */
public class ChallengeResponseException extends java.lang.Exception {

    private ChallengeResponseException child=null;

    public ChallengeResponseException(String mesg) {
        super(mesg);
    }

    public ChallengeResponseException(ChallengeResponseException e, String mesg) {
        super(mesg);
        child = e;
    }

    /**
     * Prints out the exception class and error message, including
     * all the nested exceptions.
     */
    public String toString() {
        if(child != null) {
            return (super.toString()+ " >> " + child.toStringNested());
        } else {
            return super.toString();
        }
    }

    /**
     * Prints out the error message of this exception, including all the
     * nested exceptions.
     */
    public String toStringNested() {
        if(child != null) {
            return ( getMessage() + " >> " + child.toStringNested());
        } else {
            return getMessage();
        }
    }
}
