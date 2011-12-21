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

import ch.boye.httpclientandroidlib.HeaderElement;
import ch.boye.httpclientandroidlib.NameValuePair;
import ch.boye.httpclientandroidlib.util.CharArrayBuffer;
import ch.boye.httpclientandroidlib.util.LangUtils;

/**
 * Basic implementation of {@link HeaderElement}
 *
 * @since 4.0
 */
public class BasicHeaderElement implements HeaderElement, Cloneable {

    private final String name;
    private final String value;
    private final NameValuePair[] parameters;

    /**
     * Constructor with name, value and parameters.
     *
     * @param name header element name
     * @param value header element value. May be <tt>null</tt>
     * @param parameters header element parameters. May be <tt>null</tt>.
     *   Parameters are copied by reference, not by value
     */
    public BasicHeaderElement(
            final String name,
            final String value,
            final NameValuePair[] parameters) {
        super();
        if (name == null) {
            throw new IllegalArgumentException("Name may not be null");
        }
        this.name = name;
        this.value = value;
        if (parameters != null) {
            this.parameters = parameters;
        } else {
            this.parameters = new NameValuePair[] {};
        }
    }

    /**
     * Constructor with name and value.
     *
     * @param name header element name
     * @param value header element value. May be <tt>null</tt>
     */
    public BasicHeaderElement(final String name, final String value) {
       this(name, value, null);
    }

    public String getName() {
        return this.name;
    }

    public String getValue() {
        return this.value;
    }

    public NameValuePair[] getParameters() {
        return (NameValuePair[])this.parameters.clone();
    }

    public int getParameterCount() {
        return this.parameters.length;
    }

    public NameValuePair getParameter(int index) {
        // ArrayIndexOutOfBoundsException is appropriate
        return this.parameters[index];
    }

    public NameValuePair getParameterByName(final String name) {
        if (name == null) {
            throw new IllegalArgumentException("Name may not be null");
        }
        NameValuePair found = null;
        for (int i = 0; i < this.parameters.length; i++) {
            NameValuePair current = this.parameters[ i ];
            if (current.getName().equalsIgnoreCase(name)) {
                found = current;
                break;
            }
        }
        return found;
    }

    public boolean equals(final Object object) {
        if (this == object) return true;
        if (object instanceof HeaderElement) {
            BasicHeaderElement that = (BasicHeaderElement) object;
            return this.name.equals(that.name)
                && LangUtils.equals(this.value, that.value)
                && LangUtils.equals(this.parameters, that.parameters);
        } else {
            return false;
        }
    }

    public int hashCode() {
        int hash = LangUtils.HASH_SEED;
        hash = LangUtils.hashCode(hash, this.name);
        hash = LangUtils.hashCode(hash, this.value);
        for (int i = 0; i < this.parameters.length; i++) {
            hash = LangUtils.hashCode(hash, this.parameters[i]);
        }
        return hash;
    }

    public String toString() {
        CharArrayBuffer buffer = new CharArrayBuffer(64);
        buffer.append(this.name);
        if (this.value != null) {
            buffer.append("=");
            buffer.append(this.value);
        }
        for (int i = 0; i < this.parameters.length; i++) {
            buffer.append("; ");
            buffer.append(this.parameters[i]);
        }
        return buffer.toString();
    }

    public Object clone() throws CloneNotSupportedException {
        // parameters array is considered immutable
        // no need to make a copy of it
        return super.clone();
    }

}

