/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.common;

import java.util.Set;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

public class EditorBranch implements Editor {

  private final String prefix;
  private Editor editor;

  public EditorBranch(final SharedPreferences prefs, final String prefix) {
    if (!prefix.endsWith(".")) {
      throw new IllegalArgumentException("No trailing period in prefix.");
    }
    this.prefix = prefix;
    this.editor = prefs.edit();
  }

  @Override
  public void apply() {
    this.editor.apply();
  }

  @Override
  public Editor clear() {
    this.editor = this.editor.clear();
    return this;
  }

  @Override
  public boolean commit() {
    return this.editor.commit();
  }

  @Override
  public Editor putBoolean(String key, boolean value) {
    this.editor = this.editor.putBoolean(prefix + key, value);
    return this;
  }

  @Override
  public Editor putFloat(String key, float value) {
    this.editor = this.editor.putFloat(prefix + key, value);
    return this;
  }

  @Override
  public Editor putInt(String key, int value) {
    this.editor = this.editor.putInt(prefix + key, value);
    return this;
  }

  @Override
  public Editor putLong(String key, long value) {
    this.editor = this.editor.putLong(prefix + key, value);
    return this;
  }

  @Override
  public Editor putString(String key, String value) {
    this.editor = this.editor.putString(prefix + key, value);
    return this;
  }

  // Not marking as Override, because Android <= 10 doesn't have
  // putStringSet. Neither can we implement it.
  public Editor putStringSet(String key, Set<String> value) {
    throw new RuntimeException("putStringSet not available.");
  }

  @Override
  public Editor remove(String key) {
    this.editor = this.editor.remove(prefix + key);
    return this;
  }
}