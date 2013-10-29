/*
 * $HeadURL$
 * $Revision$
 * $Date$
 *
 * ====================================================================
 *
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.client.utils;

import java.util.StringTokenizer;

import ch.boye.httpclientandroidlib.annotation.Immutable;

/**
 * Implementation from pseudo code in RFC 3492.
 *
 * @since 4.0
 */
@Immutable
public class Rfc3492Idn implements Idn {
    private static final int base = 36;
    private static final int tmin = 1;
    private static final int tmax = 26;
    private static final int skew = 38;
    private static final int damp = 700;
    private static final int initial_bias = 72;
    private static final int initial_n = 128;
    private static final char delimiter = '-';
    private static final String ACE_PREFIX = "xn--";

    private int adapt(int delta, int numpoints, boolean firsttime) {
        if (firsttime) delta = delta / damp;
        else delta = delta / 2;
        delta = delta + (delta / numpoints);
        int k = 0;
        while (delta > ((base - tmin) * tmax) / 2) {
          delta = delta / (base - tmin);
          k = k + base;
        }
        return k + (((base - tmin + 1) * delta) / (delta + skew));
    }

    private int digit(char c) {
        if ((c >= 'A') && (c <= 'Z')) return (c - 'A');
        if ((c >= 'a') && (c <= 'z')) return (c - 'a');
        if ((c >= '0') && (c <= '9')) return (c - '0') + 26;
        throw new IllegalArgumentException("illegal digit: "+ c);
    }

    public String toUnicode(String punycode) {
        StringBuilder unicode = new StringBuilder(punycode.length());
        StringTokenizer tok = new StringTokenizer(punycode, ".");
        while (tok.hasMoreTokens()) {
            String t = tok.nextToken();
            if (unicode.length() > 0) unicode.append('.');
            if (t.startsWith(ACE_PREFIX)) t = decode(t.substring(4));
            unicode.append(t);
        }
        return unicode.toString();
    }

    protected String decode(String input) {
        int n = initial_n;
        int i = 0;
        int bias = initial_bias;
        StringBuilder output = new StringBuilder(input.length());
        int lastdelim = input.lastIndexOf(delimiter);
        if (lastdelim != -1) {
            output.append(input.subSequence(0, lastdelim));
            input = input.substring(lastdelim + 1);
        }

        while (input.length() > 0) {
            int oldi = i;
            int w = 1;
            for (int k = base;; k += base) {
                if (input.length() == 0) break;
                char c = input.charAt(0);
                input = input.substring(1);
                int digit = digit(c);
                i = i + digit * w; // FIXME fail on overflow
                int t;
                if (k <= bias + tmin) {
                    t = tmin;
                } else if (k >= bias + tmax) {
                    t = tmax;
                } else {
                    t = k - bias;
                }
                if (digit < t) break;
                w = w * (base - t); // FIXME fail on overflow
            }
            bias = adapt(i - oldi, output.length() + 1, (oldi == 0));
            n = n + i / (output.length() + 1); // FIXME fail on overflow
            i = i % (output.length() + 1);
            // {if n is a basic code point then fail}
            output.insert(i, (char) n);
            i++;
        }
        return output.toString();
    }

}
