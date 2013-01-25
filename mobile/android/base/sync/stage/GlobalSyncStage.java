/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.stage;

import java.util.Collection;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.Map;

import org.mozilla.gecko.sync.GlobalSession;


public interface GlobalSyncStage {
  public static enum Stage {
    idle,                       // Start state.
    checkPreconditions,         // Preparation of the basics. TODO: clear status
    ensureClusterURL,           // Setting up where we talk to.
    fetchInfoCollections,       // Take a look at timestamps.
    fetchMetaGlobal,
    ensureKeysStage,
    /*
    ensureSpecialRecords,
    updateEngineTimestamps,
    */
    syncClientsEngine(SyncClientsEngineStage.STAGE_NAME),
    /*
    processFirstSyncPref,
    processClientCommands,
    updateEnabledEngines,
    */
    syncTabs("tabs"),
    syncPasswords("passwords"),
    syncBookmarks("bookmarks"),
    syncHistory("history"),
    syncFormHistory("forms"),

    uploadMetaGlobal,
    completed;

    // Maintain a mapping from names ("bookmarks") to Stage enumerations (syncBookmarks).
    private static final Map<String, Stage> named = new HashMap<String, Stage>();
    static {
      for (Stage s : EnumSet.allOf(Stage.class)) {
        if (s.getRepositoryName() != null) {
          named.put(s.getRepositoryName(), s);
        }
      }
    }

    public static Stage byName(final String name) {
      if (name == null) {
        return null;
      }
      return named.get(name);
    }

    /**
     * @return an immutable collection of Stages.
     */
    public static Collection<Stage> getNamedStages() {
      return Collections.unmodifiableCollection(named.values());
    }

    // Each Stage tracks its repositoryName.
    private final String repositoryName;
    public String getRepositoryName() {
      return repositoryName;
    }

    private Stage() {
      this.repositoryName = null;
    }

    private Stage(final String name) {
      this.repositoryName = name;
    }
  }

  public void execute(GlobalSession session) throws NoSuchStageException;
  public void resetLocal(GlobalSession session);
  public void wipeLocal(GlobalSession session) throws Exception;

  /**
   * What storage version number this engine supports.
   * <p>
   * Used to generate a fresh meta/global record for upload.
   * @return a version number or <code>null</code> to never include this engine in a fresh meta/global record.
   */
  public Integer getStorageVersion();
}
