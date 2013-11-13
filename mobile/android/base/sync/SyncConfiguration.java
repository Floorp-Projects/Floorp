/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Map.Entry;
import java.util.Set;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.crypto.PersistedCrypto5Keys;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.stage.GlobalSyncStage.Stage;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

public class SyncConfiguration {

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
  public URI             serverURL;
  public URI             clusterURL;
  public KeyBundle       syncKeyBundle;

  public CollectionKeys  collectionKeys;
  public InfoCollections infoCollections;
  public MetaGlobal      metaGlobal;
  public String          syncID;

  protected final String username;

  /**
   * Persisted collection of enabledEngineNames.
   * <p>
   * Can contain engines Android Sync is not currently aware of, such as "prefs"
   * or "addons".
   * <p>
   * Copied from latest downloaded meta/global record and used to generate a
   * fresh meta/global record for upload.
   */
  public Set<String> enabledEngineNames;

  /**
   * Names of stages to sync <it>this sync</it>, or <code>null</code> to sync
   * all known stages.
   * <p>
   * Generated <it>each sync</it> from extras bundle passed to
   * <code>SyncAdapter.onPerformSync</code> and not persisted.
   * <p>
   * Not synchronized! Set this exactly once per global session and don't modify
   * it -- especially not from multiple threads.
   */
  public Collection<String> stagesToSync;

  /**
   * Engines whose sync state has been modified by the user through
   * SelectEnginesActivity, where each key-value pair is an engine name and
   * its sync state.
   *
   * This differs from <code>enabledEngineNames</code> in that
   * <code>enabledEngineNames</code> reflects the downloaded meta/global,
   * whereas <code>userSelectedEngines</code> stores the differences in engines to
   * sync that the user has selected.
   *
   * Each engine stage will check for engine changes at the beginning of the
   * stage.
   *
   * If no engine sync state changes have been made by the user, userSelectedEngines
   * will be null, and Sync will proceed normally.
   *
   * If the user has made changes to engine syncing state, each engine will sync
   * according to the sync state specified in userSelectedEngines and propagate that
   * state to meta/global, to be uploaded.
   */
  public Map<String, Boolean> userSelectedEngines;
  public long userSelectedEnginesTimestamp;

  // Fields that maintain a reference to a SharedPreferences instance, used for
  // persistence.
  // Behavior is undefined if the PrefsSource is switched out in flight.
  public String          prefsPath;
  public PrefsSource     prefsSource;

  protected final AuthHeaderProvider authHeaderProvider;

  public static final String PREF_PREFS_VERSION = "prefs.version";
  public static final long CURRENT_PREFS_VERSION = 1;

  public static final String CLIENTS_COLLECTION_TIMESTAMP = "serverClientsTimestamp";  // When the collection was touched.
  public static final String CLIENT_RECORD_TIMESTAMP = "serverClientRecordTimestamp";  // When our record was touched.

  public static final String PREF_CLUSTER_URL = "clusterURL";
  public static final String PREF_SYNC_ID = "syncID";

  public static final String PREF_ENABLED_ENGINE_NAMES = "enabledEngineNames";
  public static final String PREF_USER_SELECTED_ENGINES_TO_SYNC = "userSelectedEngines";
  public static final String PREF_USER_SELECTED_ENGINES_TO_SYNC_TIMESTAMP = "userSelectedEnginesTimestamp";

  public static final String PREF_EARLIEST_NEXT_SYNC = "earliestnextsync";
  public static final String PREF_CLUSTER_URL_IS_STALE = "clusterurlisstale";

  public static final String PREF_ACCOUNT_GUID = "account.guid";
  public static final String PREF_CLIENT_NAME = "account.clientName";
  public static final String PREF_NUM_CLIENTS = "account.numClients";

  /**
   * Create a new SyncConfiguration instance. Pass in a PrefsSource to
   * provide access to preferences.
   */
  public SyncConfiguration(String username, AuthHeaderProvider authHeaderProvider, String prefsPath, PrefsSource prefsSource) {
    this.username = username;
    this.authHeaderProvider = authHeaderProvider;
    this.prefsPath   = prefsPath;
    this.prefsSource = prefsSource;
    this.loadFromPrefs(getPrefs());
  }

  public SharedPreferences getPrefs() {
    Logger.trace(LOG_TAG, "Returning prefs for " + prefsPath);
    return prefsSource.getPrefs(prefsPath, Utils.SHARED_PREFERENCES_MODE);
  }

  /**
   * Valid engines supported by Android Sync.
   *
   * @return Set<String> of valid engine names that Android Sync implements.
   */
  public static Set<String> validEngineNames() {
    Set<String> engineNames = new HashSet<String>();
    for (Stage stage : Stage.getNamedStages()) {
      engineNames.add(stage.getRepositoryName());
    }
    return engineNames;
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

  /**
   * Gets the engine names that are enabled in meta/global.
   *
   * @param prefs
   *          SharedPreferences that the engines are associated with.
   * @return Set<String> of the enabled engine names if they have been stored,
   *         or null otherwise.
   */
  public static Set<String> getEnabledEngineNames(SharedPreferences prefs) {
    String json = prefs.getString(PREF_ENABLED_ENGINE_NAMES, null);
    if (json == null) {
      return null;
    }
    try {
      ExtendedJSONObject o = ExtendedJSONObject.parseJSONObject(json);
      return new HashSet<String>(o.keySet());
    } catch (Exception e) {
      // enabledEngineNames can be null.
      return null;
    }
  }

  /**
   * Gets the engines whose sync states have been changed by the user through the
   * SelectEnginesActivity.
   *
   * @param prefs
   *          SharedPreferences of account that the engines are associated with.
   * @return Map<String, Boolean> of changed engines. Key is the lower-cased
   *         engine name, Value is the new sync state.
   */
  public static Map<String, Boolean> getUserSelectedEngines(SharedPreferences prefs) {
    String json = prefs.getString(PREF_USER_SELECTED_ENGINES_TO_SYNC, null);
    if (json == null) {
      return null;
    }
    try {
      ExtendedJSONObject o = ExtendedJSONObject.parseJSONObject(json);
      Map<String, Boolean> map = new HashMap<String, Boolean>();
      for (Entry<String, Object> e : o.entrySet()) {
        String key = e.getKey();
        Boolean value = (Boolean) e.getValue();
        map.put(key, value);
        // Forms depends on history. Add forms if history is selected.
        if ("history".equals(key)) {
          map.put("forms", value);
        }
      }
      // Sanity check: remove forms if history does not exist.
      if (!map.containsKey("history")) {
        map.remove("forms");
      }
      return map;
    } catch (Exception e) {
      return null;
    }
  }

  /**
   * Store a Map of engines and their sync states to prefs.
   *
   * @param prefs
   *          SharedPreferences that the engines are associated with.
   * @param selectedEngines
   *          Map<String, Boolean> of engine name to sync state
   */
  public static void storeSelectedEnginesToPrefs(SharedPreferences prefs, Map<String, Boolean> selectedEngines) {
    ExtendedJSONObject jObj = new ExtendedJSONObject();
    for (Entry<String, Boolean> e : selectedEngines.entrySet()) {
      jObj.put(e.getKey(), e.getValue());
    }
    String json = jObj.toJSONString();
    long currentTime = System.currentTimeMillis();
    Editor edit = prefs.edit();
    edit.putString(PREF_USER_SELECTED_ENGINES_TO_SYNC, json);
    edit.putLong(PREF_USER_SELECTED_ENGINES_TO_SYNC_TIMESTAMP, currentTime);
    Logger.error(LOG_TAG, "Storing user-selected engines at [" + currentTime + "].");
    edit.commit();
  }

  public void loadFromPrefs(SharedPreferences prefs) {

    if (prefs.contains(PREF_CLUSTER_URL)) {
      String u = prefs.getString(PREF_CLUSTER_URL, null);
      try {
        clusterURL = new URI(u);
        Logger.trace(LOG_TAG, "Set clusterURL from bundle: " + u);
      } catch (URISyntaxException e) {
        Logger.warn(LOG_TAG, "Ignoring bundle clusterURL (" + u + "): invalid URI.", e);
      }
    }
    if (prefs.contains(PREF_SYNC_ID)) {
      syncID = prefs.getString(PREF_SYNC_ID, null);
      Logger.trace(LOG_TAG, "Set syncID from bundle: " + syncID);
    }
    enabledEngineNames = getEnabledEngineNames(prefs);
    userSelectedEngines = getUserSelectedEngines(prefs);
    userSelectedEnginesTimestamp = prefs.getLong(PREF_USER_SELECTED_ENGINES_TO_SYNC_TIMESTAMP, 0);
    // We don't set crypto/keys here because we need the syncKeyBundle to decrypt the JSON
    // and we won't have it on construction.
    // TODO: MetaGlobal, password, infoCollections.
  }

  public void persistToPrefs() {
    this.persistToPrefs(this.getPrefs());
  }

  public void persistToPrefs(SharedPreferences prefs) {
    Editor edit = prefs.edit();
    if (clusterURL == null) {
      edit.remove(PREF_CLUSTER_URL);
    } else {
      edit.putString(PREF_CLUSTER_URL, clusterURL.toASCIIString());
    }
    if (syncID != null) {
      edit.putString(PREF_SYNC_ID, syncID);
    }
    if (enabledEngineNames == null) {
      edit.remove(PREF_ENABLED_ENGINE_NAMES);
    } else {
      ExtendedJSONObject o = new ExtendedJSONObject();
      for (String engineName : enabledEngineNames) {
        o.put(engineName, 0);
      }
      edit.putString(PREF_ENABLED_ENGINE_NAMES, o.toJSONString());
    }
    if (userSelectedEngines == null) {
      edit.remove(PREF_USER_SELECTED_ENGINES_TO_SYNC);
      edit.remove(PREF_USER_SELECTED_ENGINES_TO_SYNC_TIMESTAMP);
    }
    // Don't bother saving userSelectedEngines - these should only be changed by
    // SelectEnginesActivity.
    edit.commit();
    // TODO: keys.
  }

  public AuthHeaderProvider getAuthHeaderProvider() {
    return authHeaderProvider;
  }

  public CollectionKeys getCollectionKeys() {
    return collectionKeys;
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

  protected String infoBaseURL() {
    return clusterURL + GlobalSession.API_VERSION + "/" + username + "/info/";
  }

  public String infoCollectionsURL() {
    return infoBaseURL() + "collections";
  }

  public String infoCollectionCountsURL() {
    return infoBaseURL() + "collection_counts";
  }

  public String metaURL() {
    return storageURL() + "/meta/global";
  }

  /**
   * Return path to storage endpoint without trailing slash.
   *
   * @return storage endpoint without trailing slash.
   */
  public String storageURL() {
    return clusterURL + GlobalSession.API_VERSION + "/" + username + "/storage";
  }

  public URI collectionURI(String collection) throws URISyntaxException {
    return new URI(storageURL() + "/" + collection);
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
    String uri = storageURL() + "/" + collection + uriParams;
    return new URI(uri);
  }

  public URI wboURI(String collection, String id) throws URISyntaxException {
    return new URI(storageURL() + "/" + collection + "/" + id);
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

    Logger.trace(LOG_TAG, "Setting cluster URL to " + u.toASCIIString() +
                          (shouldPersist ? ". Persisting." : ". Not persisting."));
    clusterURL = u;
    if (shouldPersist) {
      Editor edit = prefs.edit();
      edit.putString(PREF_CLUSTER_URL, clusterURL.toASCIIString());
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
    Logger.trace(LOG_TAG, "Set cluster URL to " + clusterURL.toASCIIString() + ", given input " + u.toASCIIString());
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

  /**
   * We persist two different clients timestamps: our own record's,
   * and the timestamp for the collection.
   */
  public void persistServerClientRecordTimestamp(long timestamp) {
    getEditor().putLong(SyncConfiguration.CLIENT_RECORD_TIMESTAMP, timestamp).commit();
  }

  public long getPersistedServerClientRecordTimestamp() {
    return getPrefs().getLong(SyncConfiguration.CLIENT_RECORD_TIMESTAMP, 0);
  }

  public void persistServerClientsTimestamp(long timestamp) {
    getEditor().putLong(SyncConfiguration.CLIENTS_COLLECTION_TIMESTAMP, timestamp).commit();
  }

  public long getPersistedServerClientsTimestamp() {
    return getPrefs().getLong(SyncConfiguration.CLIENTS_COLLECTION_TIMESTAMP, 0);
  }

  public void purgeCryptoKeys() {
    if (collectionKeys != null) {
      collectionKeys.clear();
    }
    persistedCryptoKeys().purge();
  }

  public void purgeMetaGlobal() {
    metaGlobal = null;
    persistedMetaGlobal().purge();
  }

  public PersistedCrypto5Keys persistedCryptoKeys() {
    return new PersistedCrypto5Keys(getPrefs(), syncKeyBundle);
  }

  public PersistedMetaGlobal persistedMetaGlobal() {
    return new PersistedMetaGlobal(getPrefs());
  }
}
