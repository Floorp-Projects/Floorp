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

package ch.boye.httpclientandroidlib.client.methods;

import java.net.URI;
import java.util.HashSet;
import java.util.Set;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.HeaderElement;
import ch.boye.httpclientandroidlib.HeaderIterator;
import ch.boye.httpclientandroidlib.HttpResponse;

/**
 * HTTP OPTIONS method.
 * <p>
 * The HTTP OPTIONS method is defined in section 9.2 of
 * <a href="http://www.ietf.org/rfc/rfc2616.txt">RFC2616</a>:
 * <blockquote>
 *  The OPTIONS method represents a request for information about the
 *  communication options available on the request/response chain
 *  identified by the Request-URI. This method allows the client to
 *  determine the options and/or requirements associated with a resource,
 *  or the capabilities of a server, without implying a resource action
 *  or initiating a resource retrieval.
 * </blockquote>
 * </p>
 *
 * @since 4.0
 */
@NotThreadSafe
public class HttpOptions extends HttpRequestBase {

    public final static String METHOD_NAME = "OPTIONS";

    public HttpOptions() {
        super();
    }

    public HttpOptions(final URI uri) {
        super();
        setURI(uri);
    }

    /**
     * @throws IllegalArgumentException if the uri is invalid.
     */
    public HttpOptions(final String uri) {
        super();
        setURI(URI.create(uri));
    }

    @Override
    public String getMethod() {
        return METHOD_NAME;
    }

    public Set<String> getAllowedMethods(final HttpResponse response) {
        if (response == null) {
            throw new IllegalArgumentException("HTTP response may not be null");
        }

        HeaderIterator it = response.headerIterator("Allow");
        Set<String> methods = new HashSet<String>();
        while (it.hasNext()) {
            Header header = it.nextHeader();
            HeaderElement[] elements = header.getElements();
            for (HeaderElement element : elements) {
                methods.add(element.getName());
            }
        }
        return methods;
    }

}
