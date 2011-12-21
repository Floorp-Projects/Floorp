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

import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.RequestLine;

/**
 * Basic implementation of {@link RequestLine}.
 *
 * @since 4.0
 */
public class BasicRequestLine implements RequestLine, Cloneable, Serializable {

    private static final long serialVersionUID = 2810581718468737193L;

    private final ProtocolVersion protoversion;
    private final String method;
    private final String uri;

    public BasicRequestLine(final String method,
                            final String uri,
                            final ProtocolVersion version) {
        super();
        if (method == null) {
            throw new IllegalArgumentException
                ("Method must not be null.");
        }
        if (uri == null) {
            throw new IllegalArgumentException
                ("URI must not be null.");
        }
        if (version == null) {
            throw new IllegalArgumentException
                ("Protocol version must not be null.");
        }
        this.method = method;
        this.uri = uri;
        this.protoversion = version;
    }

    public String getMethod() {
        return this.method;
    }

    public ProtocolVersion getProtocolVersion() {
        return this.protoversion;
    }

    public String getUri() {
        return this.uri;
    }

    public String toString() {
        // no need for non-default formatting in toString()
        return BasicLineFormatter.DEFAULT
            .formatRequestLine(null, this).toString();
    }

    public Object clone() throws CloneNotSupportedException {
        return super.clone();
    }

}
