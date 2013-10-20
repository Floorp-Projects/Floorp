/*
 * ====================================================================
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.message;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HeaderIterator;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;

/**
 * A class for combining a set of headers.
 * This class allows for multiple headers with the same name and
 * keeps track of the order in which headers were added.
 *
 *
 * @since 4.0
 */
public class HeaderGroup implements Cloneable, Serializable {

    private static final long serialVersionUID = 2608834160639271617L;

    /** The list of headers for this group, in the order in which they were added */
    private final List headers;

    /**
     * Constructor for HeaderGroup.
     */
    public HeaderGroup() {
        this.headers = new ArrayList(16);
    }

    /**
     * Removes any contained headers.
     */
    public void clear() {
        headers.clear();
    }

    /**
     * Adds the given header to the group.  The order in which this header was
     * added is preserved.
     *
     * @param header the header to add
     */
    public void addHeader(Header header) {
        if (header == null) {
            return;
        }
        headers.add(header);
    }

    /**
     * Removes the given header.
     *
     * @param header the header to remove
     */
    public void removeHeader(Header header) {
        if (header == null) {
            return;
        }
        headers.remove(header);
    }

    /**
     * Replaces the first occurence of the header with the same name. If no header with
     * the same name is found the given header is added to the end of the list.
     *
     * @param header the new header that should replace the first header with the same
     * name if present in the list.
     */
    public void updateHeader(Header header) {
        if (header == null) {
            return;
        }
        for (int i = 0; i < this.headers.size(); i++) {
            Header current = (Header) this.headers.get(i);
            if (current.getName().equalsIgnoreCase(header.getName())) {
                this.headers.set(i, header);
                return;
            }
        }
        this.headers.add(header);
    }

    /**
     * Sets all of the headers contained within this group overriding any
     * existing headers. The headers are added in the order in which they appear
     * in the array.
     *
     * @param headers the headers to set
     */
    public void setHeaders(Header[] headers) {
        clear();
        if (headers == null) {
            return;
        }
        for (int i = 0; i < headers.length; i++) {
            this.headers.add(headers[i]);
        }
    }

    /**
     * Gets a header representing all of the header values with the given name.
     * If more that one header with the given name exists the values will be
     * combined with a "," as per RFC 2616.
     *
     * <p>Header name comparison is case insensitive.
     *
     * @param name the name of the header(s) to get
     * @return a header with a condensed value or <code>null</code> if no
     * headers by the given name are present
     */
    public Header getCondensedHeader(String name) {
        Header[] headers = getHeaders(name);

        if (headers.length == 0) {
            return null;
        } else if (headers.length == 1) {
            return headers[0];
        } else {
            CharArrayBuffer valueBuffer = new CharArrayBuffer(128);
            valueBuffer.append(headers[0].getValue());
            for (int i = 1; i < headers.length; i++) {
                valueBuffer.append(", ");
                valueBuffer.append(headers[i].getValue());
            }

            return new BasicHeader(name.toLowerCase(Locale.ENGLISH), valueBuffer.toString());
        }
    }

    /**
     * Gets all of the headers with the given name.  The returned array
     * maintains the relative order in which the headers were added.
     *
     * <p>Header name comparison is case insensitive.
     *
     * @param name the name of the header(s) to get
     *
     * @return an array of length >= 0
     */
    public Header[] getHeaders(String name) {
        ArrayList headersFound = new ArrayList();

        for (int i = 0; i < headers.size(); i++) {
            Header header = (Header) headers.get(i);
            if (header.getName().equalsIgnoreCase(name)) {
                headersFound.add(header);
            }
        }

        return (Header[]) headersFound.toArray(new Header[headersFound.size()]);
    }

    /**
     * Gets the first header with the given name.
     *
     * <p>Header name comparison is case insensitive.
     *
     * @param name the name of the header to get
     * @return the first header or <code>null</code>
     */
    public Header getFirstHeader(String name) {
        for (int i = 0; i < headers.size(); i++) {
            Header header = (Header) headers.get(i);
            if (header.getName().equalsIgnoreCase(name)) {
                return header;
            }
        }
        return null;
    }

    /**
     * Gets the last header with the given name.
     *
     * <p>Header name comparison is case insensitive.
     *
     * @param name the name of the header to get
     * @return the last header or <code>null</code>
     */
    public Header getLastHeader(String name) {
        // start at the end of the list and work backwards
        for (int i = headers.size() - 1; i >= 0; i--) {
            Header header = (Header) headers.get(i);
            if (header.getName().equalsIgnoreCase(name)) {
                return header;
            }
        }

        return null;
    }

    /**
     * Gets all of the headers contained within this group.
     *
     * @return an array of length >= 0
     */
    public Header[] getAllHeaders() {
        return (Header[]) headers.toArray(new Header[headers.size()]);
    }

    /**
     * Tests if headers with the given name are contained within this group.
     *
     * <p>Header name comparison is case insensitive.
     *
     * @param name the header name to test for
     * @return <code>true</code> if at least one header with the name is
     * contained, <code>false</code> otherwise
     */
    public boolean containsHeader(String name) {
        for (int i = 0; i < headers.size(); i++) {
            Header header = (Header) headers.get(i);
            if (header.getName().equalsIgnoreCase(name)) {
                return true;
            }
        }

        return false;
    }

    /**
     * Returns an iterator over this group of headers.
     *
     * @return iterator over this group of headers.
     *
     * @since 4.0
     */
    public HeaderIterator iterator() {
        return new BasicListHeaderIterator(this.headers, null);
    }

    /**
     * Returns an iterator over the headers with a given name in this group.
     *
     * @param name      the name of the headers over which to iterate, or
     *                  <code>null</code> for all headers
     *
     * @return iterator over some headers in this group.
     *
     * @since 4.0
     */
    public HeaderIterator iterator(final String name) {
        return new BasicListHeaderIterator(this.headers, name);
    }

    /**
     * Returns a copy of this object
     *
     * @return copy of this object
     *
     * @since 4.0
     */
    public HeaderGroup copy() {
        HeaderGroup clone = new HeaderGroup();
        clone.headers.addAll(this.headers);
        return clone;
    }

    public Object clone() throws CloneNotSupportedException {
        HeaderGroup clone = (HeaderGroup) super.clone();
        clone.headers.clear();
        clone.headers.addAll(this.headers);
        return clone;
    }

    public String toString() {
        return this.headers.toString();
    }

}
