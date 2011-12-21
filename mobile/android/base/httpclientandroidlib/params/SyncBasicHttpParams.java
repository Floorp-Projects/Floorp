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
 */

package ch.boye.httpclientandroidlib.params;

/**
 * Thread-safe extension of the {@link BasicHttpParams}.
 *
 * @since 4.1
 */
public class SyncBasicHttpParams extends BasicHttpParams {

    private static final long serialVersionUID = 5387834869062660642L;

    public SyncBasicHttpParams() {
        super();
    }

    public synchronized boolean removeParameter(final String name) {
        return super.removeParameter(name);
    }

    public synchronized HttpParams setParameter(final String name, final Object value) {
        return super.setParameter(name, value);
    }

    public synchronized Object getParameter(final String name) {
        return super.getParameter(name);
    }

    public synchronized boolean isParameterSet(final String name) {
        return super.isParameterSet(name);
    }

    public synchronized boolean isParameterSetLocally(final String name) {
        return super.isParameterSetLocally(name);
    }

    public synchronized void setParameters(final String[] names, final Object value) {
        super.setParameters(names, value);
    }

    public synchronized void clear() {
        super.clear();
    }

    public synchronized Object clone() throws CloneNotSupportedException {
        return super.clone();
    }

}
