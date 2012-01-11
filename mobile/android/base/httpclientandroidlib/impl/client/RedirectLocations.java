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

package ch.boye.httpclientandroidlib.impl.client;

import java.net.URI;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

/**
 * This class represents a collection of {@link URI}s used as redirect locations.
 *
 * @since 4.0
 */
@NotThreadSafe // HashSet is not synch.
public class RedirectLocations {

    private final Set<URI> unique;
    private final List<URI> all;

    public RedirectLocations() {
        super();
        this.unique = new HashSet<URI>();
        this.all = new ArrayList<URI>();
    }

    /**
     * Test if the URI is present in the collection.
     */
    public boolean contains(final URI uri) {
        return this.unique.contains(uri);
    }

    /**
     * Adds a new URI to the collection.
     */
    public void add(final URI uri) {
        this.unique.add(uri);
        this.all.add(uri);
    }

    /**
     * Removes a URI from the collection.
     */
    public boolean remove(final URI uri) {
        boolean removed = this.unique.remove(uri);
        if (removed) {
            Iterator<URI> it = this.all.iterator();
            while (it.hasNext()) {
                URI current = it.next();
                if (current.equals(uri)) {
                    it.remove();
                }
            }
        }
        return removed;
    }

    /**
     * Returns all redirect {@link URI}s in the order they were added to the collection.
     *
     * @return list of all URIs
     *
     * @since 4.1
     */
    public List<URI> getAll() {
        return new ArrayList<URI>(this.all);
    }

}
