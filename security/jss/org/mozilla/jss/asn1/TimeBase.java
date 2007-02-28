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
package org.mozilla.jss.asn1;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.util.Calendar;
import java.util.Date;
import java.util.TimeZone;
import org.mozilla.jss.util.Assert;

public abstract class TimeBase implements ASN1Value {

    public static final Form FORM = Form.PRIMITIVE;
    abstract public Tag getTag();

    private Date date;

    public Date toDate() {
        return date;
    }

    abstract protected boolean isUTC();

    private TimeBase() { }

    public TimeBase(Date date) {
        this.date = date;
    }

    public void encode(OutputStream ostream) throws IOException {
        encode(getTag(), ostream);
    }

    /**
     * Write the DER-encoding of this TimeBase.
     */
    public void encode(Tag implicit, OutputStream ostream) throws IOException {

        if( isUTC() ) {
            // length will always be 13
            (new ASN1Header(implicit, FORM, 13)).encode(ostream);
        } else {
            // length will always be 15
            (new ASN1Header(implicit, FORM, 15)).encode(ostream);
        }
        
        int i=0, val;

        // DER-encoding mandates GMT time zone
        Calendar cal = Calendar.getInstance( TimeZone.getTimeZone("GMT") );
        cal.setTime( date );

        if( isUTC() ) {
            val = cal.get(Calendar.YEAR);
            ostream.write( ((val % 100) / 10) + '0' );
            ostream.write(  (val % 10) + '0' );
        } else {
            val = cal.get(Calendar.YEAR);
            ostream.write( ((val % 10000) / 1000) + '0' );
            ostream.write( ((val % 1000) / 100) + '0' );
            ostream.write( ((val % 100) / 10) + '0' );
            ostream.write(  (val % 10) + '0' );
        }

        val = cal.get(Calendar.MONTH) + 1;
        Assert._assert( val >= 1 && val <= 12 );
        ostream.write( (val / 10) + '0' );
        ostream.write( (val % 10) + '0' );

        val = cal.get(Calendar.DAY_OF_MONTH);
        Assert._assert( val >=1 && val <= 31 );
        ostream.write( (val / 10) + '0' );
        ostream.write( (val % 10) + '0' );

        val = cal.get(Calendar.HOUR_OF_DAY);
        Assert._assert( val >= 0 && val <= 23 );
        ostream.write( (val / 10) + '0' );
        ostream.write( (val % 10) + '0' );

        val = cal.get(Calendar.MINUTE);
        Assert._assert( val >=0 && val <= 59 );
        ostream.write( (val / 10) + '0' );
        ostream.write( (val % 10) + '0' );

        val = cal.get(Calendar.SECOND);
        Assert._assert( val >= 0 && val <= 59 );
        ostream.write( (val / 10) + '0' );
        ostream.write( (val % 10) + '0' );

        ostream.write('Z');
    }

    public abstract static class Template {

        protected abstract boolean isUTC();

        protected abstract Tag getTag();

        protected abstract TimeBase generateInstance(Date date);

        public boolean tagMatch(Tag tag) {
            return getTag().equals(tag);
        }

        public ASN1Value decode(InputStream istream)
            throws IOException, InvalidBERException
        {
            return decode(getTag(), istream);
        }

        public ASN1Value decode(Tag implicitTag, InputStream istream)
            throws IOException, InvalidBERException
        {
            PrintableString.Template pst = new PrintableString.Template();
            PrintableString ps = (PrintableString)
                                        pst.decode(implicitTag, istream);
            char[] chars = ps.toCharArray();
            int i=0;
            int year, month, day, hour, minute, second, hourOff, minOff;

            //////////////////////////////////////////
            // Get year
            //
            if( isUTC() ) {
                checkBounds(i, 2, chars.length);
                year = (chars[i] - '0') * 10;
                year += chars[i+1] - '0';

                // Y2K HACK!!!!! But this is what the spec says to do.
                // The range is 1970 to 2069
                if( year < 70 ) {
                    year += 2000;
                } else {
                    year += 1900;
                }
                i += 2;
            } else {
                checkBounds(i, 4, chars.length);
                year = (chars[i] - '0') * 1000;
                year += (chars[i+1] - '0') * 100;
                year += (chars[i+2] - '0') * 10;
                year += (chars[i+3] - '0');
                checkRange(year, 0, 9999, "year");
                i += 4;
            }
            
            //////////////////////////////////////////
            // get month
            //
            month = 0;
            checkBounds(i, 2, chars.length);
            month = (chars[i] - '0') * 10;
            month += chars[i+1] - '0';
            checkRange(month, 1, 12, "month");
            month--; // Java months start at 0
            i += 2;

            //////////////////////////////////////////
            // get day
            //
            checkBounds(i, 2, chars.length);
            day = (chars[i] - '0') * 10;
            day += chars[i+1] - '0';
            checkRange(day, 1, 31, "day");
            i += 2;

            //////////////////////////////////////////
            // get hour
            //
            checkBounds(i, 2, chars.length);
            hour = (chars[i] - '0') * 10;
            hour += chars[i+1] - '0';
            checkRange(hour, 0, 23, "hour");
            i += 2;

            //////////////////////////////////////////
            // get minute
            //
            checkBounds(i, 2, chars.length);
            minute = (chars[i] - '0') * 10;
            minute += chars[i+1] - '0';
            checkRange(minute, 0, 59, "minute");
            i += 2;

            //////////////////////////////////////////
            // get second, if it's there
            //
            if( i < chars.length  && chars[i] >= '0' && chars[i] <= '9' ) {
                checkBounds(i, 2, chars.length);
                second = (chars[i] - '0') * 10;
                second += chars[i+1] - '0';
                checkRange(second, 0, 59, "second");
                i += 2;
            } else {
                second = 0;
            }

            //////////////////////////////////////////
            // Skip milliseconds for GeneralizedTime.  There are no
            // milliseconds in UTCTime.
            //
            if( ! isUTC() ) {
                while(  i < chars.length &&
                    chars[i] != '+' &&
                    chars[i] != '-' &&
                    chars[i] != 'Z' )
                {
                    i++;
                }
            }

            //////////////////////////////////////////
            // get time zone
            //
            TimeZone tz;
            if( i < chars.length ) {
                checkBounds(i, 1, chars.length);
                if( chars[i] == '+' || chars[i] == '-') {
                    checkBounds(i+1, 4, chars.length);
                    hourOff = (chars[i+1] - '0') * 10;
                    hourOff += chars[i+2] - '0';
                    minOff = (chars[i+3] - '0') * 10;
                    minOff += chars[i+4] - '0';
                    checkRange(hourOff, 0, 23, "hour offset");
                    checkRange(minOff, 0, 59, "minute offset");
                    if( chars[i] == '-' ) {
                        hourOff = -hourOff;
                        minOff = -minOff;
                    }
		    i += 5;
                    tz = (TimeZone) TimeZone.getTimeZone("GMT").clone();
                    tz.setRawOffset( ((hourOff*60)+minOff)*60*1000 );
                } else if( chars[i] == 'Z' ) {
                    i += 1;
                    hourOff = minOff = 0;
                    tz = (TimeZone) TimeZone.getTimeZone("GMT").clone();
                } else {
                    throw new InvalidBERException("Invalid character "+
                        chars[i]);
                }
            } else {
                if( isUTC() ) {
                    // Only UTC requires timezone
                    throw new InvalidBERException("no timezone specified for"+
                        " UTCTime");
                }
                // No timezone specified, use local time.
                // This is generally a bad idea, because who knows what the
                // local timezone is? But the spec allows it.
                tz = TimeZone.getDefault();
            }

            // make sure we ate all the characters, there were no stragglers
            // at the end
            if( i != chars.length ) {
                throw new InvalidBERException("Extra characters at end");
            }

            // Create a calendar object from the date and time zone.
            Calendar cal = Calendar.getInstance( tz );
            cal.set(year, month, day, hour, minute, second);
            
            return generateInstance(cal.getTime());
        }

        private static void
        checkRange(int val, int low, int high, String field)
            throws InvalidBERException
        {
            if( val < low || val > high ) {
                throw new InvalidBERException("Invalid "+field);
            }
        }

        private static void
        checkBounds(int index, int increment, int bound)
            throws InvalidBERException
        {
            if(index+increment > bound) {
                throw new InvalidBERException("Too few characters in " +
                    "TimeBase");
            }
        }
    }
}
