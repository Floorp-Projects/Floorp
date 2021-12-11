/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.os.Parcel;
import android.os.Parcelable;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.collection.ArrayMap;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * Base class for (nested) runtime settings.
 *
 * <p>Handles pref-based settings. Please extend this class when adding nested settings for
 * GeckoRuntimeSettings.
 */
public abstract class RuntimeSettings implements Parcelable {
  /**
   * Base class for (nested) runtime settings builders.
   *
   * <p>Please extend this class when adding nested settings builders for GeckoRuntimeSettings.
   */
  public abstract static class Builder<Settings extends RuntimeSettings> {
    private final Settings mSettings;

    @SuppressWarnings("checkstyle:javadocmethod")
    public Builder() {
      mSettings = newSettings(null);
    }

    /**
     * Finalize and return the settings.
     *
     * @return The constructed settings.
     */
    @AnyThread
    public @NonNull Settings build() {
      return newSettings(mSettings);
    }

    @AnyThread
    protected @NonNull Settings getSettings() {
      return mSettings;
    }

    /**
     * Create a default or copy settings object.
     *
     * @param settings Settings object to copy, null for default settings.
     * @return The constructed settings object.
     */
    @AnyThread
    protected abstract @NonNull Settings newSettings(final @Nullable Settings settings);
  }

  /** Used to handle pref-based settings. */
  /* package */ class Pref<T> {
    public final String name;
    public final T defaultValue;
    private T mValue;
    private boolean mIsSet;

    public Pref(@NonNull final String name, final T defaultValue) {
      this.name = name;
      this.defaultValue = defaultValue;
      mValue = defaultValue;

      RuntimeSettings.this.addPref(this);
    }

    public void set(final T newValue) {
      mValue = newValue;
      mIsSet = true;
    }

    public void commit(final T newValue) {
      if (newValue.equals(mValue)) {
        return;
      }
      set(newValue);
      commit();
    }

    public void commit() {
      final GeckoRuntime runtime = RuntimeSettings.this.getRuntime();
      if (runtime == null) {
        return;
      }
      final GeckoBundle prefs = new GeckoBundle(1);
      addToBundle(prefs);
      runtime.setDefaultPrefs(prefs);
    }

    public T get() {
      return mValue;
    }

    public boolean isSet() {
      return mIsSet;
    }

    public void reset() {
      mValue = defaultValue;
      mIsSet = false;
    }

    private void addToBundle(final GeckoBundle bundle) {
      final T value = mIsSet ? mValue : defaultValue;
      if (value instanceof String) {
        bundle.putString(name, (String) value);
      } else if (value instanceof Integer) {
        bundle.putInt(name, (Integer) value);
      } else if (value instanceof Boolean) {
        bundle.putBoolean(name, (Boolean) value);
      } else {
        throw new UnsupportedOperationException("Unhandled pref type for " + name);
      }
    }
  }

  private RuntimeSettings mParent;
  private final ArrayList<RuntimeSettings> mChildren;
  private final ArrayList<Pref<?>> mPrefs;

  protected RuntimeSettings() {
    this(null /* parent */);
  }

  /**
   * Create settings object.
   *
   * @param parent The parent settings, specify in case of nested settings.
   */
  protected RuntimeSettings(final @Nullable RuntimeSettings parent) {
    mPrefs = new ArrayList<Pref<?>>();
    mChildren = new ArrayList<RuntimeSettings>();

    setParent(parent);
  }

  /**
   * Update the prefs based on the provided settings.
   *
   * @param settings Copy from this settings.
   */
  @AnyThread
  protected void updatePrefs(final @NonNull RuntimeSettings settings) {
    if (mPrefs.size() != settings.mPrefs.size()) {
      throw new IllegalArgumentException("Settings must be compatible");
    }

    for (int i = 0; i < mPrefs.size(); ++i) {
      if (!mPrefs.get(i).name.equals(settings.mPrefs.get(i).name)) {
        throw new IllegalArgumentException("Settings must be compatible");
      }
      if (!settings.mPrefs.get(i).isSet()) {
        continue;
      }
      // We know it is safe.
      @SuppressWarnings("unchecked")
      final Pref<Object> uncheckedPref = (Pref<Object>) mPrefs.get(i);
      uncheckedPref.commit(settings.mPrefs.get(i).get());
    }
  }

  /* package */ @Nullable
  GeckoRuntime getRuntime() {
    if (mParent != null) {
      return mParent.getRuntime();
    }
    return null;
  }

  private void setParent(final @Nullable RuntimeSettings parent) {
    mParent = parent;
    if (mParent != null) {
      mParent.addChild(this);
    }
  }

  private void addChild(final @NonNull RuntimeSettings child) {
    mChildren.add(child);
  }

  /* pacakge */ void addPref(final Pref<?> pref) {
    mPrefs.add(pref);
  }

  /**
   * Return a mapping of the prefs managed in this settings, including child settings.
   *
   * @return A key-value mapping of the prefs.
   */
  /* package */ @NonNull
  Map<String, Object> getPrefsMap() {
    final ArrayMap<String, Object> prefs = new ArrayMap<>();
    forAllPrefs(pref -> prefs.put(pref.name, pref.get()));

    return Collections.unmodifiableMap(prefs);
  }

  /**
   * Iterates through all prefs in this RuntimeSettings instance and in all children, grandchildren,
   * etc.
   */
  private void forAllPrefs(final GeckoResult.Consumer<Pref<?>> visitor) {
    for (final RuntimeSettings child : mChildren) {
      child.forAllPrefs(visitor);
    }

    for (final Pref<?> pref : mPrefs) {
      visitor.accept(pref);
    }
  }

  /**
   * Reset the prefs managed by this settings and its children.
   *
   * <p>The actual prefs values are set via {@link #getPrefsMap} during initialization and via
   * {@link Pref#commit} during runtime for individual prefs.
   */
  /* package */ void commitResetPrefs() {
    final ArrayList<String> names = new ArrayList<String>();
    forAllPrefs(pref -> names.add(pref.name));

    final GeckoBundle data = new GeckoBundle(1);
    data.putStringArray("names", names);
    EventDispatcher.getInstance().dispatch("GeckoView:ResetUserPrefs", data);
  }

  @Override // Parcelable
  @AnyThread
  public int describeContents() {
    return 0;
  }

  @Override // Parcelable
  @AnyThread
  public void writeToParcel(final Parcel out, final int flags) {
    for (final Pref<?> pref : mPrefs) {
      out.writeValue(pref.get());
    }
  }

  @AnyThread
  // AIDL code may call readFromParcel even though it's not part of Parcelable.
  @SuppressWarnings("checkstyle:javadocmethod")
  public void readFromParcel(final @NonNull Parcel source) {
    for (final Pref<?> pref : mPrefs) {
      // We know this is safe.
      @SuppressWarnings("unchecked")
      final Pref<Object> uncheckedPref = (Pref<Object>) pref;
      uncheckedPref.commit(source.readValue(getClass().getClassLoader()));
    }
  }
}
