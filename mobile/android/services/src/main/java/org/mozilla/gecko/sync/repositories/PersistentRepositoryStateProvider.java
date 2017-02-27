/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import android.support.annotation.Nullable;

import org.mozilla.gecko.background.common.PrefsBranch;

/**
 * Simple persistent implementation of a repository state provider.
 * Uses provided PrefsBranch object in order to persist values.
 *
 * Values must be committed before they become visible via getters.
 * It is a caller's responsibility to perform a commit.
 *
 * @author grisha
 */
public class PersistentRepositoryStateProvider implements RepositoryStateProvider {
    private final PrefsBranch prefs;

    private final PrefsBranch.Editor editor;

    public PersistentRepositoryStateProvider(PrefsBranch prefs) {
        this.prefs = prefs;
        // NB: It is a caller's responsibility to commit any changes it performs via setters.
        this.editor = prefs.edit();
    }

    @Override
    public boolean isPersistent() {
        return true;
    }

    @Override
    public boolean commit() {
        return this.editor.commit();
    }

    @Override
    public PersistentRepositoryStateProvider clear(String key) {
        this.editor.remove(key);
        return this;
    }

    @Override
    public PersistentRepositoryStateProvider setString(String key, String value) {
        this.editor.putString(key, value);
        return this;
    }

    @Nullable
    @Override
    public String getString(String key) {
        return this.prefs.getString(key, null);
    }

    @Override
    public PersistentRepositoryStateProvider setLong(String key, Long value) {
        this.editor.putLong(key, value);
        return this;
    }

    @Nullable
    @Override
    public Long getLong(String key) {
        if (!this.prefs.contains(key)) {
            return null;
        }
        return this.prefs.getLong(key, 0);
    }

    @Override
    public boolean resetAndCommit() {
        return this.editor
                .remove(KEY_HIGH_WATER_MARK)
                .remove(KEY_OFFSET)
                .remove(KEY_OFFSET_ORDER)
                .remove(KEY_OFFSET_SINCE)
                .commit();
    }
}
