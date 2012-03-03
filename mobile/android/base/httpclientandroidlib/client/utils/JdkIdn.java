/*
 * $HeadURL$
 * $Revision$
 * $Date$
 *
 * ====================================================================
 *
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */

package ch.boye.httpclientandroidlib.client.utils;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import ch.boye.httpclientandroidlib.annotation.Immutable;

/**
 * Uses the java.net.IDN class through reflection.
 *
 * @since 4.0
 */
@Immutable
public class JdkIdn implements Idn {
    private final Method toUnicode;

    /**
     *
     * @throws ClassNotFoundException if java.net.IDN is not available
     */
    public JdkIdn() throws ClassNotFoundException {
        Class<?> clazz = Class.forName("java.net.IDN");
        try {
            toUnicode = clazz.getMethod("toUnicode", String.class);
        } catch (SecurityException e) {
            // doesn't happen
            throw new IllegalStateException(e.getMessage(), e);
        } catch (NoSuchMethodException e) {
            // doesn't happen
            throw new IllegalStateException(e.getMessage(), e);
        }
    }

    public String toUnicode(String punycode) {
        try {
            return (String) toUnicode.invoke(null, punycode);
        } catch (IllegalAccessException e) {
            throw new IllegalStateException(e.getMessage(), e);
        } catch (InvocationTargetException e) {
            Throwable t = e.getCause();
            throw new RuntimeException(t.getMessage(), t);
        }
    }

}
