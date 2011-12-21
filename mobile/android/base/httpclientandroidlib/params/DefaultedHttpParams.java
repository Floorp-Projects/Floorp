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

/**
 * {@link HttpParams} implementation that delegates resolution of a parameter
 * to the given default {@link HttpParams} instance if the parameter is not
 * present in the local one. The state of the local collection can be mutated,
 * whereas the default collection is treated as read-only.
 *
 * @since 4.0
 */
public final class DefaultedHttpParams extends AbstractHttpParams {

    private final HttpParams local;
    private final HttpParams defaults;

    /**
     * Create the defaulted set of HttpParams.
     *
     * @param local the mutable set of HttpParams
     * @param defaults the default set of HttpParams, not mutated by this class
     */
    public DefaultedHttpParams(final HttpParams local, final HttpParams defaults) {
        super();
        if (local == null) {
            throw new IllegalArgumentException("HTTP parameters may not be null");
        }
        this.local = local;
        this.defaults = defaults;
    }

    /**
     * Creates a copy of the local collection with the same default
     *
     * @deprecated
     */
    public HttpParams copy() {
        HttpParams clone = this.local.copy();
        return new DefaultedHttpParams(clone, this.defaults);
    }

    /**
     * Retrieves the value of the parameter from the local collection and, if the
     * parameter is not set locally, delegates its resolution to the default
     * collection.
     */
    public Object getParameter(final String name) {
        Object obj = this.local.getParameter(name);
        if (obj == null && this.defaults != null) {
            obj = this.defaults.getParameter(name);
        }
        return obj;
    }

    /**
     * Attempts to remove the parameter from the local collection. This method
     * <i>does not</i> modify the default collection.
     */
    public boolean removeParameter(final String name) {
        return this.local.removeParameter(name);
    }

    /**
     * Sets the parameter in the local collection. This method <i>does not</i>
     * modify the default collection.
     */
    public HttpParams setParameter(final String name, final Object value) {
        return this.local.setParameter(name, value);
    }

    /**
     *
     * @return the default HttpParams collection
     */
    public HttpParams getDefaults() {
        return this.defaults;
    }

}
