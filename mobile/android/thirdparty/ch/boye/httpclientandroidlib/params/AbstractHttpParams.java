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

import ch.boye.httpclientandroidlib.params.HttpParams;

/**
 * Abstract base class for parameter collections.
 * Type specific setters and getters are mapped to the abstract,
 * generic getters and setters.
 *
 * @since 4.0
 */
public abstract class AbstractHttpParams implements HttpParams {

    /**
     * Instantiates parameters.
     */
    protected AbstractHttpParams() {
        super();
    }

    public long getLongParameter(final String name, long defaultValue) {
        Object param = getParameter(name);
        if (param == null) {
            return defaultValue;
        }
        return ((Long)param).longValue();
    }

    public HttpParams setLongParameter(final String name, long value) {
        setParameter(name, new Long(value));
        return this;
    }

    public int getIntParameter(final String name, int defaultValue) {
        Object param = getParameter(name);
        if (param == null) {
            return defaultValue;
        }
        return ((Integer)param).intValue();
    }

    public HttpParams setIntParameter(final String name, int value) {
        setParameter(name, new Integer(value));
        return this;
    }

    public double getDoubleParameter(final String name, double defaultValue) {
        Object param = getParameter(name);
        if (param == null) {
            return defaultValue;
        }
        return ((Double)param).doubleValue();
    }

    public HttpParams setDoubleParameter(final String name, double value) {
        setParameter(name, new Double(value));
        return this;
    }

    public boolean getBooleanParameter(final String name, boolean defaultValue) {
        Object param = getParameter(name);
        if (param == null) {
            return defaultValue;
        }
        return ((Boolean)param).booleanValue();
    }

    public HttpParams setBooleanParameter(final String name, boolean value) {
        setParameter(name, value ? Boolean.TRUE : Boolean.FALSE);
        return this;
    }

    public boolean isParameterTrue(final String name) {
        return getBooleanParameter(name, false);
    }

    public boolean isParameterFalse(final String name) {
        return !getBooleanParameter(name, false);
    }

} // class AbstractHttpParams
