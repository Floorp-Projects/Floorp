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
package com.netscape.jss.asn1;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;
import com.netscape.jss.util.Assert;

/**
 * The ASN.1 type <code>GeneralizedTime</code>
 */
public class GeneralizedTime extends TimeBase implements ASN1Value {

    public static final Tag TAG = new Tag(Tag.UNIVERSAL, 24);
    public Tag getTag() {
        return TAG;
    }

    /**
     * Creates a <code>GeneralizedTime</code> from a Date.
     */
    public GeneralizedTime(Date date) {
        super(date);
    }

    protected boolean isUTC() {
        return false;
    }

    private static final GeneralizedTime.Template templateInstance =
                                new GeneralizedTime.Template();
    public static GeneralizedTime.Template getTemplate() {
        return templateInstance;
    }

    /**
     * A class for decoding <code>GeneralizedTime</code>s.
     */
    public static class Template extends TimeBase.Template
        implements ASN1Template
    {
        protected Tag getTag() {
            return TAG;
        }

        public boolean tagMatch(Tag tag) {
            return TAG.equals(tag);
        }

        protected boolean isUTC() {
            return false;
        }

        protected TimeBase generateInstance(Date date) {
            return new GeneralizedTime(date);
        }
    }
}
