/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import android.support.annotation.Nullable;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Simple non-persistent implementation of a repository state provider.
 *
 * Just like in the persistent implementation, changes to values are visible only after a commit.
 *
 * @author grisha
 */
public class NonPersistentRepositoryStateProvider implements RepositoryStateProvider {
    // We'll have at least OFFSET and H.W.M. values set.
    private final int INITIAL_CAPACITY = 2;
    private final Map<String, Object> nonCommittedValuesMap = Collections.synchronizedMap(
            new HashMap<String, Object>(INITIAL_CAPACITY)
    );

    // NB: Any changes are made by creating a new map instead of altering an existing one.
    private volatile Map<String, Object> committedValuesMap = new HashMap<>(INITIAL_CAPACITY);

    @Override
    public boolean isPersistent() {
        return false;
    }

    @Override
    public boolean commit() {
        committedValuesMap = new HashMap<>(nonCommittedValuesMap);
        return true;
    }

    @Override
    public NonPersistentRepositoryStateProvider clear(String key) {
        nonCommittedValuesMap.remove(key);
        return this;
    }

    @Override
    public NonPersistentRepositoryStateProvider setString(String key, String value) {
        nonCommittedValuesMap.put(key, value);
        return this;
    }

    @Nullable
    @Override
    public String getString(String key) {
        return (String) committedValuesMap.get(key);
    }

    @Override
    public NonPersistentRepositoryStateProvider setLong(String key, Long value) {
        nonCommittedValuesMap.put(key, value);
        return this;
    }

    @Nullable
    @Override
    public Long getLong(String key) {
        return (Long) committedValuesMap.get(key);
    }

    @Override
    public boolean resetAndCommit() {
        nonCommittedValuesMap.clear();
        return commit();
    }
}
