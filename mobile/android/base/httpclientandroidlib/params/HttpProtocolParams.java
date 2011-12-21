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

package ch.boye.httpclientandroidlib.params;

import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.protocol.HTTP;

/**
 * Utility class for accessing protocol parameters in {@link HttpParams}.
 *
 * @since 4.0
 *
 * @see CoreProtocolPNames
 */
public final class HttpProtocolParams implements CoreProtocolPNames {

    private HttpProtocolParams() {
        super();
    }

    /**
     * Obtains value of the {@link CoreProtocolPNames#HTTP_ELEMENT_CHARSET} parameter.
     * If not set, defaults to <code>US-ASCII</code>.
     *
     * @param params HTTP parameters.
     * @return HTTP element charset.
     */
    public static String getHttpElementCharset(final HttpParams params) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        String charset = (String) params.getParameter
            (CoreProtocolPNames.HTTP_ELEMENT_CHARSET);
        if (charset == null) {
            charset = HTTP.DEFAULT_PROTOCOL_CHARSET;
        }
        return charset;
    }

    /**
     * Sets value of the {@link CoreProtocolPNames#HTTP_ELEMENT_CHARSET} parameter.
     *
     * @param params HTTP parameters.
     * @param charset HTTP element charset.
     */
    public static void setHttpElementCharset(final HttpParams params, final String charset) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        params.setParameter(CoreProtocolPNames.HTTP_ELEMENT_CHARSET, charset);
    }

    /**
     * Obtains value of the {@link CoreProtocolPNames#HTTP_CONTENT_CHARSET} parameter.
     * If not set, defaults to <code>ISO-8859-1</code>.
     *
     * @param params HTTP parameters.
     * @return HTTP content charset.
     */
    public static String getContentCharset(final HttpParams params) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        String charset = (String) params.getParameter
            (CoreProtocolPNames.HTTP_CONTENT_CHARSET);
        if (charset == null) {
            charset = HTTP.DEFAULT_CONTENT_CHARSET;
        }
        return charset;
    }

    /**
     * Sets value of the {@link CoreProtocolPNames#HTTP_CONTENT_CHARSET} parameter.
     *
     * @param params HTTP parameters.
     * @param charset HTTP content charset.
     */
    public static void setContentCharset(final HttpParams params, final String charset) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        params.setParameter(CoreProtocolPNames.HTTP_CONTENT_CHARSET, charset);
    }

    /**
     * Obtains value of the {@link CoreProtocolPNames#PROTOCOL_VERSION} parameter.
     * If not set, defaults to {@link HttpVersion#HTTP_1_1}.
     *
     * @param params HTTP parameters.
     * @return HTTP protocol version.
     */
    public static ProtocolVersion getVersion(final HttpParams params) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        Object param = params.getParameter
            (CoreProtocolPNames.PROTOCOL_VERSION);
        if (param == null) {
            return HttpVersion.HTTP_1_1;
        }
        return (ProtocolVersion)param;
    }

    /**
     * Sets value of the {@link CoreProtocolPNames#PROTOCOL_VERSION} parameter.
     *
     * @param params HTTP parameters.
     * @param version HTTP protocol version.
     */
    public static void setVersion(final HttpParams params, final ProtocolVersion version) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        params.setParameter(CoreProtocolPNames.PROTOCOL_VERSION, version);
    }

    /**
     * Obtains value of the {@link CoreProtocolPNames#USER_AGENT} parameter.
     * If not set, returns <code>null</code>.
     *
     * @param params HTTP parameters.
     * @return User agent string.
     */
    public static String getUserAgent(final HttpParams params) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        return (String) params.getParameter(CoreProtocolPNames.USER_AGENT);
    }

    /**
     * Sets value of the {@link CoreProtocolPNames#USER_AGENT} parameter.
     *
     * @param params HTTP parameters.
     * @param useragent User agent string.
     */
    public static void setUserAgent(final HttpParams params, final String useragent) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        params.setParameter(CoreProtocolPNames.USER_AGENT, useragent);
    }

    /**
     * Obtains value of the {@link CoreProtocolPNames#USE_EXPECT_CONTINUE} parameter.
     * If not set, returns <code>false</code>.
     *
     * @param params HTTP parameters.
     * @return User agent string.
     */
    public static boolean useExpectContinue(final HttpParams params) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        return params.getBooleanParameter
            (CoreProtocolPNames.USE_EXPECT_CONTINUE, false);
    }

    /**
     * Sets value of the {@link CoreProtocolPNames#USE_EXPECT_CONTINUE} parameter.
     *
     * @param params HTTP parameters.
     * @param b expect-continue flag.
     */
    public static void setUseExpectContinue(final HttpParams params, boolean b) {
        if (params == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        params.setBooleanParameter(CoreProtocolPNames.USE_EXPECT_CONTINUE, b);
    }

}
