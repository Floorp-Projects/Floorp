/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Map;
import java.util.Set;

import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.stage.EnsureClusterURLStage;

import ch.boye.httpclientandroidlib.HttpResponse;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

public class SyncConfiguration implements CredentialsSource {

  public class EditorBranch implements Editor {

    private String prefix;
    private Editor editor;

    public EditorBranch(SyncConfiguration config, String prefix) {
      if (!prefix.endsWith(".")) {
        throw new IllegalArgumentException("No trailing period in prefix.");
      }
      this.prefix = prefix;
      this.editor = config.getEditor();
    }

    public void apply() {
      // Android <=r8 SharedPreferences.Editor does not contain apply() for overriding.
      this.editor.commit();
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

  /**
   * A wrapper around a portion of the SharedPreferences space.
   *
   * @author rnewman
   *
   */
  public class ConfigurationBranch implements SharedPreferences {

    private SyncConfiguration config;
    private String prefix;                // Including trailing period.

    public ConfigurationBranch(SyncConfiguration syncConfiguration,
        String prefix) {
      if (!prefix.endsWith(".")) {
        throw new IllegalArgumentException("No trailing period in prefix.");
      }
      this.config = syncConfiguration;
      this.prefix = prefix;
    }

    @Override
    public boolean contains(String key) {
      return config.getPrefs().contains(prefix + key);
    }

    @Override
    public Editor edit() {
      return new EditorBranch(config, prefix);
    }

    @Override
    public Map<String, ?> getAll() {
      // Not implemented. TODO
      return null;
    }

    @Override
    public boolean getBoolean(String key, boolean defValue) {
      return config.getPrefs().getBoolean(prefix + key, defValue);
    }

    @Override
    public float getFloat(String key, float defValue) {
      return config.getPrefs().getFloat(prefix + key, defValue);
    }

    @Override
    public int getInt(String key, int defValue) {
      return config.getPrefs().getInt(prefix + key, defValue);
    }

    @Override
    public long getLong(String key, long defValue) {
      return config.getPrefs().getLong(prefix + key, defValue);
    }

    @Override
    public String getString(String key, String defValue) {
      return config.getPrefs().getString(prefix + key, defValue);
    }

    // Not marking as Override, because Android <= 10 doesn't have
    // getStringSet. Neither can we implement it.
    public Set<String> getStringSet(String key, Set<String> defValue) {
      throw new RuntimeException("getStringSet not available.");
    }

    @Override
    public void registerOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
      config.getPrefs().registerOnSharedPreferenceChangeListener(listener);
    }

    @Override
    public void unregisterOnSharedPreferenceChangeListener(OnSharedPreferenceChangeListener listener) {
      config.getPrefs().unregisterOnSharedPreferenceChangeListener(listener);
    }
  }

  public static final String DEFAULT_USER_API = "https://auth.services.mozilla.com/user/1.0/";

  private static final String LOG_TAG = "SyncConfiguration";

  // These must be set in GlobalSession's constructor.
  public String          userAPI;
  public URI             serverURL;
  protected URI          clusterURL;
  public String          username;
  public KeyBundle       syncKeyBundle;

  public CollectionKeys  collectionKeys;
  public InfoCollections infoCollections;
  public MetaGlobal      metaGlobal;
  public String          password;
  public String          syncID;

  // Fields that maintain a reference to a SharedPreferences instance, used for
  // persistence.
  // Behavior is undefined if the PrefsSource is switched out in flight.
  public String          prefsPath;
  public PrefsSource     prefsSource;

  /**
   * Create a new SyncConfiguration instance. Pass in a PrefsSource to
   * provide access to preferences.
   */
  public SyncConfiguration(String prefsPath, PrefsSource prefsSource) {
    this.prefsPath   = prefsPath;
    this.prefsSource = prefsSource;
    this.loadFromPrefs(getPrefs());
  }

  public SharedPreferences getPrefs() {
    Logger.debug(LOG_TAG, "Returning prefs for " + prefsPath);
    return prefsSource.getPrefs(prefsPath, Utils.SHARED_PREFERENCES_MODE);
  }

  /**
   * Return a convenient accessor for part of prefs.
   * @return
   *        A ConfigurationBranch object representing this
   *        section of the preferences space.
   */
  public ConfigurationBranch getBranch(String prefix) {
    return new ConfigurationBranch(this, prefix);
  }

  public void loadFromPrefs(SharedPreferences prefs) {

    if (prefs.contains("clusterURL")) {
      String u = prefs.getString("clusterURL", null);
      try {
        clusterURL = new URI(u);
        Logger.info(LOG_TAG, "Set clusterURL from bundle: " + u);
      } catch (URISyntaxException e) {
        Logger.warn(LOG_TAG, "Ignoring bundle clusterURL (" + u + "): invalid URI.", e);
      }
    }
    if (prefs.contains("syncID")) {
      syncID = prefs.getString("syncID", null);
      Logger.info(LOG_TAG, "Set syncID from bundle: " + syncID);
    }
    // TODO: MetaGlobal, password, infoCollections, collectionKeys.
  }

  public void persistToPrefs() {
    this.persistToPrefs(this.getPrefs());
  }

  public void persistToPrefs(SharedPreferences prefs) {
    Editor edit = prefs.edit();
    if (clusterURL == null) {
      edit.remove("clusterURL");
    } else {
      edit.putString("clusterURL", clusterURL.toASCIIString());
    }
    if (syncID != null) {
      edit.putString("syncID", syncID);
    }
    edit.commit();
    // TODO: keys.
  }

  @Override
  public String credentials() {
    return username + ":" + password;
  }

  @Override
  public CollectionKeys getCollectionKeys() {
    return collectionKeys;
  }

  @Override
  public KeyBundle keyForCollection(String collection) throws NoCollectionKeysSetException {
    return getCollectionKeys().keyBundleForCollection(collection);
  }

  public void setCollectionKeys(CollectionKeys k) {
    collectionKeys = k;
  }

  public String nodeWeaveURL() {
    return this.nodeWeaveURL((this.serverURL == null) ? null : this.serverURL.toASCIIString());
  }

  public String nodeWeaveURL(String serverURL) {
    String userPart = username + "/node/weave";
    if (serverURL == null) {
      return DEFAULT_USER_API + userPart;
    }
    if (!serverURL.endsWith("/")) {
      serverURL = serverURL + "/";
    }
    return serverURL + "user/1.0/" + userPart;
  }

  public String infoURL() {
    return clusterURL + GlobalSession.API_VERSION + "/" + username + "/info/collections";
  }
  public String metaURL() {
    return clusterURL + GlobalSession.API_VERSION + "/" + username + "/storage/meta/global";
  }

  public String storageURL(boolean trailingSlash) {
    return clusterURL + GlobalSession.API_VERSION + "/" + username +
           (trailingSlash ? "/storage/" : "/storage");
  }

  public URI collectionURI(String collection, boolean full) throws URISyntaxException {
    // Do it this way to make it easier to add more params later.
    // It's pretty ugly, I'll grant.
    boolean anyParams = full;
    String  uriParams = "";
    if (anyParams) {
      StringBuilder params = new StringBuilder("?");
      if (full) {
        params.append("full=1");
      }
      uriParams = params.toString();
    }
    String uri = storageURL(true) + collection + uriParams;
    return new URI(uri);
  }

  public URI wboURI(String collection, String id) throws URISyntaxException {
    return new URI(storageURL(true) + collection + "/" + id);
  }

  public URI keysURI() throws URISyntaxException {
    return wboURI("crypto", "keys");
  }

  public URI getClusterURL() {
    return clusterURL;
  }

  public String getClusterURLString() {
    if (clusterURL == null) {
      return null;
    }
    return clusterURL.toASCIIString();
  }

  protected void setAndPersistClusterURL(URI u, SharedPreferences prefs) {
    boolean shouldPersist = (prefs != null) && (clusterURL == null);

    Logger.debug(LOG_TAG, "Setting cluster URL to " + u.toASCIIString() +
                          (shouldPersist ? ". Persisting." : ". Not persisting."));
    clusterURL = u;
    if (shouldPersist) {
      Editor edit = prefs.edit();
      edit.putString("clusterURL", clusterURL.toASCIIString());
      edit.commit();
    }
  }

  protected void setClusterURL(URI u, SharedPreferences prefs) {
    if (u == null) {
      Logger.warn(LOG_TAG, "Refusing to set cluster URL to null.");
      return;
    }
    URI uri = u.normalize();
    if (uri.toASCIIString().endsWith("/")) {
      setAndPersistClusterURL(u, prefs);
      return;
    }
    setAndPersistClusterURL(uri.resolve("/"), prefs);
    Logger.info(LOG_TAG, "Set cluster URL to " + clusterURL.toASCIIString() + ", given input " + u.toASCIIString());
  }

  public void setClusterURL(URI u) {
    setClusterURL(u, this.getPrefs());
  }

  /**
   * Used for direct management of related prefs.
   */
  public Editor getEditor() {
    return this.getPrefs().edit();
  }
}
