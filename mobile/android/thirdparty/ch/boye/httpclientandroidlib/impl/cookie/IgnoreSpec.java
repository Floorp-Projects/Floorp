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

package ch.boye.httpclientandroidlib.impl.cookie;

import java.util.Collections;
import java.util.List;

import ch.boye.httpclientandroidlib.Header;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.cookie.Cookie;
import ch.boye.httpclientandroidlib.cookie.CookieOrigin;
import ch.boye.httpclientandroidlib.cookie.MalformedCookieException;

/**
 * CookieSpec that ignores all cookies
 *
 * @since 4.1
 */
@NotThreadSafe // superclass is @NotThreadSafe
public class IgnoreSpec extends CookieSpecBase {

    public int getVersion() {
        return 0;
    }

    public List<Cookie> parse(Header header, CookieOrigin origin)
            throws MalformedCookieException {
        return Collections.emptyList();
    }

    public List<Header> formatCookies(List<Cookie> cookies) {
        return Collections.emptyList();
    }

    public Header getVersionHeader() {
        return null;
    }
}
