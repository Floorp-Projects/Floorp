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

package org.mozilla.jss.pkix.cmc;

import org.mozilla.jss.asn1.*;
import java.io.*;
import org.mozilla.jss.util.Assert;

/**
 * CMCStatusInfo <i>OtherInfo</i>:
 * <pre>
 *   OtherInfo ::= CHOICE { 
 *       failInfo INTEGER, 
 *       pendInfo PendInfo 
 *   } 
 * </pre>
 */
public class OtherInfo implements ASN1Value {
    // CMCFailInfo constants
    public static final int BAD_ALG = 0;
    public static final int BAD_MESSAGE_CHECK = 1;
    public static final int BAD_REQUEST = 2;
    public static final int BAD_TIME = 3;
    public static final int BAD_CERT_ID = 4;
    public static final int UNSUPORTED_EXT = 5;
    public static final int MUST_ARCHIVE_KEYS = 6;
    public static final int BAD_IDENTITY = 7;
    public static final int POP_REQUIRED = 8;
    public static final int POP_FAILED = 9;
    public static final int NO_KEY_REUSE = 10;
    public static final int INTERNAL_CA_ERROR = 11;
    public static final int TRY_LATER = 12;

    public static final String[] FAIL_INFO = {"bad algorithm",
												"bad message check",
												"bad request",
												"bad time",
												"bad certificate id",
												"unsupported extensions",
												"must archive keys",
												"bad identity",
												"POP required",
												"POP failed",
												"no key reuse",
												"internal ca error",
												"try later"};
    /**
     * The type of OtherInfo.
     */
    public static class Type {
        private Type() { }

        static Type FAIL = new Type();
        static Type PEND = new Type();
    }
    public static Type FAIL = Type.FAIL;
    public static Type PEND = Type.PEND;

    ///////////////////////////////////////////////////////////////////////
    // members and member access
    ///////////////////////////////////////////////////////////////////////

    private Type type;
    private INTEGER failInfo; // if type == FAIL
    private PendInfo pendInfo; // if type == PEND

    ///////////////////////////////////////////////////////////////////////
    // Constructors
    ///////////////////////////////////////////////////////////////////////

    // no default constructor
    public OtherInfo() { }

    /** 
     * Constructs a OtherInfo from its components.
     *
     * @param type The type of the otherInfo.
     * @param failInfo the CMCFailInfo code.
     * @param pendInfo the pending information.
     */
    public OtherInfo(Type type, INTEGER failInfo, PendInfo pendInfo) {
        this.type = type;
        this.failInfo = failInfo;
        this.pendInfo = pendInfo;
    }

    ///////////////////////////////////////////////////////////////////////
    // accessors
    ///////////////////////////////////////////////////////////////////////

    /**
     * Returns the type of OtherInfo: <ul>
     * <li><code>FAIL</code>
     * <li><code>PEND</code>
     * </ul>
     */
    public Type getType() {
        return type;
    }

    /**
     * If type == FAIL, returns the failInfo field. Otherwise,
     * returns null.
     */
    public INTEGER getFailInfo() {
        return failInfo;
    }

    /**
     * If type == PEND, returns the pendInfo field. Otherwise,
     * returns null.
     */
    public PendInfo getPendInfo() {
        return pendInfo;
    }

    ///////////////////////////////////////////////////////////////////////
    // DER decoding/encoding
    ///////////////////////////////////////////////////////////////////////

    public Tag getTag() {
		// return the subType's tag
        if( type == FAIL ) {
            return INTEGER.TAG;
        } else {
            Assert._assert( type == PEND );
            return PendInfo.TAG;
        }
    }

    public void encode(OutputStream ostream) throws IOException {

        if( type == FAIL ) {
            failInfo.encode(ostream);
        } else {
            Assert._assert( type == PEND );
            pendInfo.encode(ostream);
        }
    }

    public void encode(Tag implicitTag, OutputStream ostream)
            throws IOException {
			//Assert.notReached("A CHOICE cannot be implicitly tagged " +implicitTag.getNum());
			encode(ostream);
    }

    private static final Template templateInstance = new Template();
    public static Template getTemplate() {
        return templateInstance;
    }

    /**
     * A Template for decoding a OtherInfo.
     */
    public static class Template implements ASN1Template {

        private CHOICE.Template choicet;

        public Template() {
            choicet = new CHOICE.Template();
            choicet.addElement( INTEGER.getTemplate() );
            choicet.addElement( PendInfo.getTemplate() );
        }

        public boolean tagMatch(Tag tag) {
            return choicet.tagMatch(tag);
        }

        public ASN1Value decode(InputStream istream)
                throws InvalidBERException, IOException {
            CHOICE c = (CHOICE) choicet.decode(istream);

            if( c.getTag().equals(INTEGER.TAG) ) {
                return new OtherInfo(FAIL, (INTEGER) c.getValue() , null);
            } else {
                Assert._assert( c.getTag().equals(PendInfo.TAG) );
                return new OtherInfo(PEND, null, (PendInfo) c.getValue());
            }
        }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
                throws InvalidBERException, IOException {
				//Assert.notReached("A CHOICE cannot be implicitly tagged");
				return decode(istream);
		}
	}
}
