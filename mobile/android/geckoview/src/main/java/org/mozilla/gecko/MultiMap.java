/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Defines a map that holds a collection of values against each key.
 *
 * @param <K> Key type
 * @param <T> Value type
 */
public class MultiMap<K, T> {
  private HashMap<K, List<T>> mMap;
  private final List<T> mEmptyList = Collections.unmodifiableList(new ArrayList<>());

  /**
   * Creates a MultiMap with specified initial capacity.
   *
   * @param count Initial capacity
   */
  public MultiMap(final int count) {
    mMap = new HashMap<>(count);
  }

  /** Creates a MultiMap with default initial capacity. */
  public MultiMap() {
    mMap = new HashMap<>();
  }

  private void ensure(final K key) {
    if (!mMap.containsKey(key)) {
      mMap.put(key, new ArrayList<>());
    }
  }

  /**
   * @return A map of key to the list of values associated to it
   */
  public Map<K, List<T>> asMap() {
    return mMap;
  }

  /**
   * @return The number of keys present in this map
   */
  public int size() {
    return mMap.size();
  }

  /**
   * @return whether this map is empty or not
   */
  public boolean isEmpty() {
    return mMap.isEmpty();
  }

  /**
   * Checks if a key is present in this map.
   *
   * @param key the key to check
   * @return True if the map contains this key, false otherwise.
   */
  public boolean containsKey(final @Nullable K key) {
    return mMap.containsKey(key);
  }

  /**
   * Checks if a (key, value) pair is present in this map.
   *
   * @param key the key to check
   * @param value the value to check
   * @return true if there is a value associated to the given key, false otherwise
   */
  public boolean containsEntry(final @Nullable K key, final @Nullable T value) {
    if (!mMap.containsKey(key)) {
      return false;
    }

    return mMap.get(key).contains(value);
  }

  /**
   * Gets the values associated with the given key.
   *
   * @param key the key to check
   * @return the list of values associated with keys, an empty list if no values are associated with
   *     key.
   */
  @NonNull
  public List<T> get(final @Nullable K key) {
    if (!mMap.containsKey(key)) {
      return mEmptyList;
    }

    return mMap.get(key);
  }

  /**
   * Add a (key, value) mapping to this map.
   *
   * @param key the key to add
   * @param value the value to add
   */
  @Nullable
  public void add(final @NonNull K key, final @NonNull T value) {
    ensure(key);
    mMap.get(key).add(value);
  }

  /**
   * Add a list of values to the given key.
   *
   * @param key the key to add
   * @param values the list of values to add
   * @return the final list of values or null if no value was added
   */
  @Nullable
  public List<T> addAll(final @NonNull K key, final @NonNull List<T> values) {
    if (values == null || values.isEmpty()) {
      return null;
    }

    ensure(key);

    final List<T> result = mMap.get(key);
    result.addAll(values);
    return result;
  }

  /**
   * Remove all mappings for the given key.
   *
   * @param key the key
   * @return values associated with the key or null if no values was present.
   */
  @Nullable
  public List<T> remove(final @Nullable K key) {
    return mMap.remove(key);
  }

  /**
   * Remove a (key, value) mapping from this map
   *
   * @param key the key to remove
   * @param value the value to remove
   * @return true if the (key, value) mapping was present, false otherwise
   */
  @Nullable
  public boolean remove(final @Nullable K key, final @Nullable T value) {
    if (!mMap.containsKey(key)) {
      return false;
    }

    final List<T> values = mMap.get(key);
    final boolean wasPresent = values.remove(value);

    if (values.isEmpty()) {
      mMap.remove(key);
    }

    return wasPresent;
  }

  /** Remove all mappings from this map. */
  public void clear() {
    mMap.clear();
  }

  /**
   * @return a set with all the keys for this map.
   */
  @NonNull
  public Set<K> keySet() {
    return mMap.keySet();
  }
}
