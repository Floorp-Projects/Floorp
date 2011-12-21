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

import java.util.Locale;
import java.util.StringTokenizer;

import ch.boye.httpclientandroidlib.annotation.Immutable;

import ch.boye.httpclientandroidlib.cookie.Cookie;
import ch.boye.httpclientandroidlib.cookie.CookieOrigin;
import ch.boye.httpclientandroidlib.cookie.CookieRestrictionViolationException;
import ch.boye.httpclientandroidlib.cookie.MalformedCookieException;

/**
 *
 * @since 4.0
 */
@Immutable
public class NetscapeDomainHandler extends BasicDomainHandler {

    public NetscapeDomainHandler() {
        super();
    }

    @Override
    public void validate(final Cookie cookie, final CookieOrigin origin)
            throws MalformedCookieException {
        super.validate(cookie, origin);
        // Perform Netscape Cookie draft specific validation
        String host = origin.getHost();
        String domain = cookie.getDomain();
        if (host.contains(".")) {
            int domainParts = new StringTokenizer(domain, ".").countTokens();

            if (isSpecialDomain(domain)) {
                if (domainParts < 2) {
                    throw new CookieRestrictionViolationException("Domain attribute \""
                        + domain
                        + "\" violates the Netscape cookie specification for "
                        + "special domains");
                }
            } else {
                if (domainParts < 3) {
                    throw new CookieRestrictionViolationException("Domain attribute \""
                        + domain
                        + "\" violates the Netscape cookie specification");
                }
            }
        }
    }

   /**
    * Checks if the given domain is in one of the seven special
    * top level domains defined by the Netscape cookie specification.
    * @param domain The domain.
    * @return True if the specified domain is "special"
    */
   private static boolean isSpecialDomain(final String domain) {
       final String ucDomain = domain.toUpperCase(Locale.ENGLISH);
       return ucDomain.endsWith(".COM")
               || ucDomain.endsWith(".EDU")
               || ucDomain.endsWith(".NET")
               || ucDomain.endsWith(".GOV")
               || ucDomain.endsWith(".MIL")
               || ucDomain.endsWith(".ORG")
               || ucDomain.endsWith(".INT");
   }

   @Override
   public boolean match(Cookie cookie, CookieOrigin origin) {
       if (cookie == null) {
           throw new IllegalArgumentException("Cookie may not be null");
       }
       if (origin == null) {
           throw new IllegalArgumentException("Cookie origin may not be null");
       }
       String host = origin.getHost();
       String domain = cookie.getDomain();
       if (domain == null) {
           return false;
       }
       return host.endsWith(domain);
   }

}
