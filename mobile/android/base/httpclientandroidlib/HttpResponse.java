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

package ch.boye.httpclientandroidlib;

import java.util.Locale;

/**
 * After receiving and interpreting a request message, a server responds
 * with an HTTP response message.
 * <pre>
 *     Response      = Status-Line
 *                     *(( general-header
 *                      | response-header
 *                      | entity-header ) CRLF)
 *                     CRLF
 *                     [ message-body ]
 * </pre>
 *
 * @since 4.0
 */
public interface HttpResponse extends HttpMessage {

    /**
     * Obtains the status line of this response.
     * The status line can be set using one of the
     * {@link #setStatusLine setStatusLine} methods,
     * or it can be initialized in a constructor.
     *
     * @return  the status line, or <code>null</code> if not yet set
     */
    StatusLine getStatusLine();

    /**
     * Sets the status line of this response.
     *
     * @param statusline the status line of this response
     */
    void setStatusLine(StatusLine statusline);

    /**
     * Sets the status line of this response.
     * The reason phrase will be determined based on the current
     * {@link #getLocale locale}.
     *
     * @param ver       the HTTP version
     * @param code      the status code
     */
    void setStatusLine(ProtocolVersion ver, int code);

    /**
     * Sets the status line of this response with a reason phrase.
     *
     * @param ver       the HTTP version
     * @param code      the status code
     * @param reason    the reason phrase, or <code>null</code> to omit
     */
    void setStatusLine(ProtocolVersion ver, int code, String reason);

    /**
     * Updates the status line of this response with a new status code.
     * The status line can only be updated if it is available. It must
     * have been set either explicitly or in a constructor.
     * <br/>
     * The reason phrase will be updated according to the new status code,
     * based on the current {@link #getLocale locale}. It can be set
     * explicitly using {@link #setReasonPhrase setReasonPhrase}.
     *
     * @param code the HTTP status code.
     *
     * @throws IllegalStateException
     *          if the status line has not be set
     *
     * @see HttpStatus
     * @see #setStatusLine(StatusLine)
     * @see #setStatusLine(ProtocolVersion,int)
     */
    void setStatusCode(int code)
        throws IllegalStateException;

    /**
     * Updates the status line of this response with a new reason phrase.
     * The status line can only be updated if it is available. It must
     * have been set either explicitly or in a constructor.
     *
     * @param reason    the new reason phrase as a single-line string, or
     *                  <code>null</code> to unset the reason phrase
     *
     * @throws IllegalStateException
     *          if the status line has not be set
     *
     * @see #setStatusLine(StatusLine)
     * @see #setStatusLine(ProtocolVersion,int)
     */
    void setReasonPhrase(String reason)
        throws IllegalStateException;

    /**
     * Obtains the message entity of this response, if any.
     * The entity is provided by calling {@link #setEntity setEntity}.
     *
     * @return  the response entity, or
     *          <code>null</code> if there is none
     */
    HttpEntity getEntity();

    /**
     * Associates a response entity with this response.
     *
     * @param entity    the entity to associate with this response, or
     *                  <code>null</code> to unset
     */
    void setEntity(HttpEntity entity);

    /**
     * Obtains the locale of this response.
     * The locale is used to determine the reason phrase
     * for the {@link #setStatusCode status code}.
     * It can be changed using {@link #setLocale setLocale}.
     *
     * @return  the locale of this response, never <code>null</code>
     */
    Locale getLocale();

    /**
     * Changes the locale of this response.
     * If there is a status line, it's reason phrase will be updated
     * according to the status code and new locale.
     *
     * @param loc       the new locale
     *
     * @see #getLocale getLocale
     * @see #setStatusCode setStatusCode
     */
    void setLocale(Locale loc);
}
