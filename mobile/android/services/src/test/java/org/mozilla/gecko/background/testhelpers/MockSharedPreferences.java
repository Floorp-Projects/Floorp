/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import android.content.SharedPreferences;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * A programmable mock content provider.
 */
public class MockSharedPreferences implements SharedPreferences, SharedPreferences.Editor {
  private HashMap<String, Object> mValues;
  private HashMap<String, Object> mTempValues;

  public MockSharedPreferences() {
    mValues = new HashMap<String, Object>();
    mTempValues = new HashMap<String, Object>();
  }

  public Editor edit() {
    return this;
  }

  public boolean contains(String key) {
    return mValues.containsKey(key);
  }

  public Map<String, ?> getAll() {
    return new HashMap<String, Object>(mValues);
  }

  public boolean getBoolean(String key, boolean defValue) {
    if (mValues.containsKey(key)) {
      return ((Boolean)mValues.get(key)).booleanValue();
    }
    return defValue;
  }

  public float getFloat(String key, float defValue) {
    if (mValues.containsKey(key)) {
      return ((Float)mValues.get(key)).floatValue();
    }
    return defValue;
  }

  public int getInt(String key, int defValue) {
    if (mValues.containsKey(key)) {
      return ((Integer)mValues.get(key)).intValue();
    }
    return defValue;
  }

  public long getLong(String key, long defValue) {
    if (mValues.containsKey(key)) {
      return ((Long)mValues.get(key)).longValue();
    }
    return defValue;
  }

  public String getString(String key, String defValue) {
    if (mValues.containsKey(key))
      return (String)mValues.get(key);
    return defValue;
  }

  @SuppressWarnings("unchecked")
  public Set<String> getStringSet(String key, Set<String> defValues) {
    if (mValues.containsKey(key)) {
      return (Set<String>) mValues.get(key);
    }
    return defValues;
  }

  public void registerOnSharedPreferenceChangeListener(
    OnSharedPreferenceChangeListener listener) {
    throw new UnsupportedOperationException();
  }

  public void unregisterOnSharedPreferenceChangeListener(
    OnSharedPreferenceChangeListener listener) {
    throw new UnsupportedOperationException();
  }

  public Editor putBoolean(String key, boolean value) {
    mTempValues.put(key, Boolean.valueOf(value));
    return this;
  }

  public Editor putFloat(String key, float value) {
    mTempValues.put(key, value);
    return this;
  }

  public Editor putInt(String key, int value) {
    mTempValues.put(key, value);
    return this;
  }

  public Editor putLong(String key, long value) {
    mTempValues.put(key, value);
    return this;
  }

  public Editor putString(String key, String value) {
    mTempValues.put(key, value);
    return this;
  }

  public Editor putStringSet(String key, Set<String> values) {
    mTempValues.put(key, values);
    return this;
  }

  public Editor remove(String key) {
    mTempValues.remove(key);
    return this;
  }

  public Editor clear() {
    mTempValues.clear();
    return this;
  }

  @SuppressWarnings("unchecked")
  public boolean commit() {
    mValues = (HashMap<String, Object>)mTempValues.clone();
    return true;
  }

  public void apply() {
    commit();
  }
}
