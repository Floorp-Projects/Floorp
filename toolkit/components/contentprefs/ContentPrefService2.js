/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, classes: Cc, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/ContentPrefUtils.jsm");
Cu.import("resource://gre/modules/ContentPrefStore.jsm");

const CACHE_MAX_GROUP_ENTRIES = 100;

const GROUP_CLAUSE = `
  SELECT id
  FROM groups
  WHERE name = :group OR
        (:includeSubdomains AND name LIKE :pattern ESCAPE '/')
`;

function ContentPrefService2() {
  if (Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_CONTENT) {
    return Cu.import("resource://gre/modules/ContentPrefServiceChild.jsm")
             .ContentPrefServiceChild;
  }

  // If this throws an exception, it causes the getService call to fail,
  // but the next time a consumer tries to retrieve the service, we'll try
  // to initialize the database again, which might work if the failure
  // was due to a temporary condition (like being out of disk space).
  this._dbInit();

  Services.obs.addObserver(this, "last-pb-context-exited");

  // Observe shutdown so we can shut down the database connection.
  Services.obs.addObserver(this, "xpcom-shutdown");
}

const cache = new ContentPrefStore();
cache.set = function CPS_cache_set(group, name, val) {
  Object.getPrototypeOf(this).set.apply(this, arguments);
  let groupCount = this._groups.size;
  if (groupCount >= CACHE_MAX_GROUP_ENTRIES) {
    // Clean half of the entries
    for (let [group, name, ] of this) {
      this.remove(group, name);
      groupCount--;
      if (groupCount < CACHE_MAX_GROUP_ENTRIES / 2)
        break;
    }
  }
};

const privModeStorage = new ContentPrefStore();

ContentPrefService2.prototype = {
  // XPCOM Plumbing

  classID: Components.ID("{e3f772f3-023f-4b32-b074-36cf0fd5d414}"),

  // Destruction

  _destroy: function ContentPrefService__destroy() {
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.obs.removeObserver(this, "last-pb-context-exited");

    this.destroy();

    this._dbConnection.asyncClose(() => {
      Services.obs.notifyObservers(null, "content-prefs-db-closed");
    });

    // Delete references to XPCOM components to make sure we don't leak them
    // (although we haven't observed leakage in tests).  Also delete references
    // in _observers and _genericObservers to avoid cycles with those that
    // refer to us and don't remove themselves from those observer pools.
    delete this._observers;
    delete this._genericObservers;
    delete this.__grouper;
  },


  // in-memory cache and private-browsing stores

  _cache: cache,
  _pbStore: privModeStorage,

  // nsIContentPrefService

  getByName: function CPS2_getByName(name, context, callback) {
    checkNameArg(name);
    checkCallbackArg(callback, true);

    // Some prefs may be in both the database and the private browsing store.
    // Notify the caller of such prefs only once, using the values from private
    // browsing.
    let pbPrefs = new ContentPrefStore();
    if (context && context.usePrivateBrowsing) {
      for (let [sgroup, sname, val] of this._pbStore) {
        if (sname == name) {
          pbPrefs.set(sgroup, sname, val);
        }
      }
    }

    let stmt1 = this._stmt(`
      SELECT groups.name AS grp, prefs.value AS value
      FROM prefs
      JOIN settings ON settings.id = prefs.settingID
      JOIN groups ON groups.id = prefs.groupID
      WHERE settings.name = :name
    `);
    stmt1.params.name = name;

    let stmt2 = this._stmt(`
      SELECT NULL AS grp, prefs.value AS value
      FROM prefs
      JOIN settings ON settings.id = prefs.settingID
      WHERE settings.name = :name AND prefs.groupID ISNULL
    `);
    stmt2.params.name = name;

    this._execStmts([stmt1, stmt2], {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        let val = row.getResultByName("value");
        this._cache.set(grp, name, val);
        if (!pbPrefs.has(grp, name))
          cbHandleResult(callback, new ContentPref(grp, name, val));
      },
      onDone: function onDone(reason, ok, gotRow) {
        if (ok) {
          for (let [pbGroup, pbName, pbVal] of pbPrefs) {
            cbHandleResult(callback, new ContentPref(pbGroup, pbName, pbVal));
          }
        }
        cbHandleCompletion(callback, reason);
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  getByDomainAndName: function CPS2_getByDomainAndName(group, name, context,
                                                       callback) {
    checkGroupArg(group);
    this._get(group, name, false, context, callback);
  },

  getBySubdomainAndName: function CPS2_getBySubdomainAndName(group, name,
                                                             context,
                                                             callback) {
    checkGroupArg(group);
    this._get(group, name, true, context, callback);
  },

  getGlobal: function CPS2_getGlobal(name, context, callback) {
    this._get(null, name, false, context, callback);
  },

  _get: function CPS2__get(group, name, includeSubdomains, context, callback) {
    group = this._parseGroup(group);
    checkNameArg(name);
    checkCallbackArg(callback, true);

    // Some prefs may be in both the database and the private browsing store.
    // Notify the caller of such prefs only once, using the values from private
    // browsing.
    let pbPrefs = new ContentPrefStore();
    if (context && context.usePrivateBrowsing) {
      for (let [sgroup, val] of
             this._pbStore.match(group, name, includeSubdomains)) {
        pbPrefs.set(sgroup, name, val);
      }
    }

    this._execStmts([this._commonGetStmt(group, name, includeSubdomains)], {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        let val = row.getResultByName("value");
        this._cache.set(grp, name, val);
        if (!pbPrefs.has(group, name))
          cbHandleResult(callback, new ContentPref(grp, name, val));
      },
      onDone: function onDone(reason, ok, gotRow) {
        if (ok) {
          if (!gotRow)
            this._cache.set(group, name, undefined);
          for (let [pbGroup, pbName, pbVal] of pbPrefs) {
            cbHandleResult(callback, new ContentPref(pbGroup, pbName, pbVal));
          }
        }
        cbHandleCompletion(callback, reason);
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  _commonGetStmt: function CPS2__commonGetStmt(group, name, includeSubdomains) {
    let stmt = group ?
      this._stmtWithGroupClause(group, includeSubdomains, `
        SELECT groups.name AS grp, prefs.value AS value
        FROM prefs
        JOIN settings ON settings.id = prefs.settingID
        JOIN groups ON groups.id = prefs.groupID
        WHERE settings.name = :name AND prefs.groupID IN (${GROUP_CLAUSE})
      `) :
      this._stmt(`
        SELECT NULL AS grp, prefs.value AS value
        FROM prefs
        JOIN settings ON settings.id = prefs.settingID
        WHERE settings.name = :name AND prefs.groupID ISNULL
      `);
    stmt.params.name = name;
    return stmt;
  },

  _stmtWithGroupClause: function CPS2__stmtWithGroupClause(group,
                                                           includeSubdomains,
                                                           sql) {
    let stmt = this._stmt(sql);
    stmt.params.group = group;
    stmt.params.includeSubdomains = includeSubdomains || false;
    stmt.params.pattern = "%." + stmt.escapeStringForLIKE(group, "/");
    return stmt;
  },

  getCachedByDomainAndName: function CPS2_getCachedByDomainAndName(group,
                                                                   name,
                                                                   context) {
    checkGroupArg(group);
    let prefs = this._getCached(group, name, false, context);
    return prefs[0] || null;
  },

  getCachedBySubdomainAndName: function CPS2_getCachedBySubdomainAndName(group,
                                                                         name,
                                                                         context,
                                                                         len) {
    checkGroupArg(group);
    let prefs = this._getCached(group, name, true, context);
    if (len)
      len.value = prefs.length;
    return prefs;
  },

  getCachedGlobal: function CPS2_getCachedGlobal(name, context) {
    let prefs = this._getCached(null, name, false, context);
    return prefs[0] || null;
  },

  _getCached: function CPS2__getCached(group, name, includeSubdomains,
                                       context) {
    group = this._parseGroup(group);
    checkNameArg(name);

    let storesToCheck = [this._cache];
    if (context && context.usePrivateBrowsing)
      storesToCheck.push(this._pbStore);

    let outStore = new ContentPrefStore();
    storesToCheck.forEach(function(store) {
      for (let [sgroup, val] of store.match(group, name, includeSubdomains)) {
        outStore.set(sgroup, name, val);
      }
    });

    let prefs = [];
    for (let [sgroup, sname, val] of outStore) {
      prefs.push(new ContentPref(sgroup, sname, val));
    }
    return prefs;
  },

  set: function CPS2_set(group, name, value, context, callback) {
    checkGroupArg(group);
    this._set(group, name, value, context, callback);
  },

  setGlobal: function CPS2_setGlobal(name, value, context, callback) {
    this._set(null, name, value, context, callback);
  },

  _set: function CPS2__set(group, name, value, context, callback) {
    group = this._parseGroup(group);
    checkNameArg(name);
    checkValueArg(value);
    checkCallbackArg(callback, false);

    if (context && context.usePrivateBrowsing) {
      this._pbStore.set(group, name, value);
      this._schedule(function() {
        cbHandleCompletion(callback, Ci.nsIContentPrefCallback2.COMPLETE_OK);
        this._notifyPrefSet(group, name, value, context.usePrivateBrowsing);
      });
      return;
    }

    // Invalidate the cached value so consumers accessing the cache between now
    // and when the operation finishes don't get old data.
    this._cache.remove(group, name);

    let stmts = [];

    // Create the setting if it doesn't exist.
    let stmt = this._stmt(`
      INSERT OR IGNORE INTO settings (id, name)
      VALUES((SELECT id FROM settings WHERE name = :name), :name)
    `);
    stmt.params.name = name;
    stmts.push(stmt);

    // Create the group if it doesn't exist.
    if (group) {
      stmt = this._stmt(`
        INSERT OR IGNORE INTO groups (id, name)
        VALUES((SELECT id FROM groups WHERE name = :group), :group)
      `);
      stmt.params.group = group;
      stmts.push(stmt);
    }

    // Finally create or update the pref.
    if (group) {
      stmt = this._stmt(`
        INSERT OR REPLACE INTO prefs (id, groupID, settingID, value, timestamp)
        VALUES(
          (SELECT prefs.id
           FROM prefs
           JOIN groups ON groups.id = prefs.groupID
           JOIN settings ON settings.id = prefs.settingID
           WHERE groups.name = :group AND settings.name = :name),
          (SELECT id FROM groups WHERE name = :group),
          (SELECT id FROM settings WHERE name = :name),
          :value,
          :now
        )
      `);
      stmt.params.group = group;
    } else {
      stmt = this._stmt(`
        INSERT OR REPLACE INTO prefs (id, groupID, settingID, value, timestamp)
        VALUES(
          (SELECT prefs.id
           FROM prefs
           JOIN settings ON settings.id = prefs.settingID
           WHERE prefs.groupID IS NULL AND settings.name = :name),
          NULL,
          (SELECT id FROM settings WHERE name = :name),
          :value,
          :now
        )
      `);
    }
    stmt.params.name = name;
    stmt.params.value = value;
    stmt.params.now = Date.now() / 1000;
    stmts.push(stmt);

    this._execStmts(stmts, {
      onDone: function onDone(reason, ok) {
        if (ok)
          this._cache.setWithCast(group, name, value);
        cbHandleCompletion(callback, reason);
        if (ok)
          this._notifyPrefSet(group, name, value, context && context.usePrivateBrowsing);
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  removeByDomainAndName: function CPS2_removeByDomainAndName(group, name,
                                                             context,
                                                             callback) {
    checkGroupArg(group);
    this._remove(group, name, false, context, callback);
  },

  removeBySubdomainAndName: function CPS2_removeBySubdomainAndName(group, name,
                                                                   context,
                                                                   callback) {
    checkGroupArg(group);
    this._remove(group, name, true, context, callback);
  },

  removeGlobal: function CPS2_removeGlobal(name, context, callback) {
    this._remove(null, name, false, context, callback);
  },

  _remove: function CPS2__remove(group, name, includeSubdomains, context,
                                 callback) {
    group = this._parseGroup(group);
    checkNameArg(name);
    checkCallbackArg(callback, false);

    // Invalidate the cached values so consumers accessing the cache between now
    // and when the operation finishes don't get old data.
    for (let sgroup of this._cache.matchGroups(group, includeSubdomains)) {
      this._cache.remove(sgroup, name);
    }

    let stmts = [];

    // First get the matching prefs.
    stmts.push(this._commonGetStmt(group, name, includeSubdomains));

    // Delete the matching prefs.
    let stmt = this._stmtWithGroupClause(group, includeSubdomains, `
      DELETE FROM prefs
      WHERE settingID = (SELECT id FROM settings WHERE name = :name) AND
            CASE typeof(:group)
            WHEN 'null' THEN prefs.groupID IS NULL
            ELSE prefs.groupID IN (${GROUP_CLAUSE})
            END
    `);
    stmt.params.name = name;
    stmts.push(stmt);

    stmts = stmts.concat(this._settingsAndGroupsCleanupStmts());

    let prefs = new ContentPrefStore();

    let isPrivate = context && context.usePrivateBrowsing;
    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        prefs.set(grp, name, undefined);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        if (ok) {
          this._cache.set(group, name, undefined);
          if (isPrivate) {
            for (let [sgroup, ] of
                   this._pbStore.match(group, name, includeSubdomains)) {
              prefs.set(sgroup, name, undefined);
              this._pbStore.remove(sgroup, name);
            }
          }
        }
        cbHandleCompletion(callback, reason);
        if (ok) {
          for (let [sgroup, , ] of prefs) {
            this._notifyPrefRemoved(sgroup, name, isPrivate);
          }
        }
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  // Deletes settings and groups that are no longer used.
  _settingsAndGroupsCleanupStmts() {
    // The NOTNULL term in the subquery of the second statment is needed because of
    // SQLite's weird IN behavior vis-a-vis NULLs.  See http://sqlite.org/lang_expr.html.
    return [
      this._stmt(`
        DELETE FROM settings
        WHERE id NOT IN (SELECT DISTINCT settingID FROM prefs)
      `),
      this._stmt(`
        DELETE FROM groups WHERE id NOT IN (
          SELECT DISTINCT groupID FROM prefs WHERE groupID NOTNULL
        )
      `)
    ];
  },

  removeByDomain: function CPS2_removeByDomain(group, context, callback) {
    checkGroupArg(group);
    this._removeByDomain(group, false, context, callback);
  },

  removeBySubdomain: function CPS2_removeBySubdomain(group, context, callback) {
    checkGroupArg(group);
    this._removeByDomain(group, true, context, callback);
  },

  removeAllGlobals: function CPS2_removeAllGlobals(context, callback) {
    this._removeByDomain(null, false, context, callback);
  },

  _removeByDomain: function CPS2__removeByDomain(group, includeSubdomains,
                                                 context, callback) {
    group = this._parseGroup(group);
    checkCallbackArg(callback, false);

    // Invalidate the cached values so consumers accessing the cache between now
    // and when the operation finishes don't get old data.
    for (let sgroup of this._cache.matchGroups(group, includeSubdomains)) {
      this._cache.removeGroup(sgroup);
    }

    let stmts = [];

    // First get the matching prefs, then delete groups and prefs that reference
    // deleted groups.
    if (group) {
      stmts.push(this._stmtWithGroupClause(group, includeSubdomains, `
        SELECT groups.name AS grp, settings.name AS name
        FROM prefs
        JOIN settings ON settings.id = prefs.settingID
        JOIN groups ON groups.id = prefs.groupID
        WHERE prefs.groupID IN (${GROUP_CLAUSE})
      `));
      stmts.push(this._stmtWithGroupClause(group, includeSubdomains,
        `DELETE FROM groups WHERE id IN (${GROUP_CLAUSE})`
      ));
      stmts.push(this._stmt(`
        DELETE FROM prefs
        WHERE groupID NOTNULL AND groupID NOT IN (SELECT id FROM groups)
      `));
    } else {
      stmts.push(this._stmt(`
        SELECT NULL AS grp, settings.name AS name
        FROM prefs
        JOIN settings ON settings.id = prefs.settingID
        WHERE prefs.groupID IS NULL
      `));
      stmts.push(this._stmt(
        "DELETE FROM prefs WHERE groupID IS NULL"
      ));
    }

    // Finally delete settings that are no longer referenced.
    stmts.push(this._stmt(`
      DELETE FROM settings
      WHERE id NOT IN (SELECT DISTINCT settingID FROM prefs)
    `));

    let prefs = new ContentPrefStore();

    let isPrivate = context && context.usePrivateBrowsing;
    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        let name = row.getResultByName("name");
        prefs.set(grp, name, undefined);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        if (ok && isPrivate) {
          for (let [sgroup, sname, ] of this._pbStore) {
            if (!group ||
                (!includeSubdomains && group == sgroup) ||
                (includeSubdomains && sgroup && this._pbStore.groupsMatchIncludingSubdomains(group, sgroup))) {
              prefs.set(sgroup, sname, undefined);
              this._pbStore.remove(sgroup, sname);
            }
          }
        }
        cbHandleCompletion(callback, reason);
        if (ok) {
          for (let [sgroup, sname, ] of prefs) {
            this._notifyPrefRemoved(sgroup, sname, isPrivate);
          }
        }
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  _removeAllDomainsSince: function CPS2__removeAllDomainsSince(since, context, callback) {
    checkCallbackArg(callback, false);

    since /= 1000;

    // Invalidate the cached values so consumers accessing the cache between now
    // and when the operation finishes don't get old data.
    // Invalidate all the group cache because we don't know which groups will be removed.
    this._cache.removeAllGroups();

    let stmts = [];

    // Get prefs that are about to be removed to notify about their removal.
    let stmt = this._stmt(`
      SELECT groups.name AS grp, settings.name AS name
      FROM prefs
      JOIN settings ON settings.id = prefs.settingID
      JOIN groups ON groups.id = prefs.groupID
      WHERE timestamp >= :since
    `);
    stmt.params.since = since;
    stmts.push(stmt);

    // Do the actual remove.
    stmt = this._stmt(`
      DELETE FROM prefs WHERE groupID NOTNULL AND timestamp >= :since
    `);
    stmt.params.since = since;
    stmts.push(stmt);

    // Cleanup no longer used values.
    stmts = stmts.concat(this._settingsAndGroupsCleanupStmts());

    let prefs = new ContentPrefStore();
    let isPrivate = context && context.usePrivateBrowsing;
    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        let name = row.getResultByName("name");
        prefs.set(grp, name, undefined);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        // This nukes all the groups in _pbStore since we don't have their timestamp
        // information.
        if (ok && isPrivate) {
          for (let [sgroup, sname, ] of this._pbStore) {
            if (sgroup) {
              prefs.set(sgroup, sname, undefined);
            }
          }
          this._pbStore.removeAllGroups();
        }
        cbHandleCompletion(callback, reason);
        if (ok) {
          for (let [sgroup, sname, ] of prefs) {
            this._notifyPrefRemoved(sgroup, sname, isPrivate);
          }
        }
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  removeAllDomainsSince: function CPS2_removeAllDomainsSince(since, context, callback) {
    this._removeAllDomainsSince(since, context, callback);
  },

  removeAllDomains: function CPS2_removeAllDomains(context, callback) {
    this._removeAllDomainsSince(0, context, callback);
  },

  removeByName: function CPS2_removeByName(name, context, callback) {
    checkNameArg(name);
    checkCallbackArg(callback, false);

    // Invalidate the cached values so consumers accessing the cache between now
    // and when the operation finishes don't get old data.
    for (let [group, sname, ] of this._cache) {
      if (sname == name)
        this._cache.remove(group, name);
    }

    let stmts = [];

    // First get the matching prefs.  Include null if any of those prefs are
    // global.
    let stmt = this._stmt(`
      SELECT groups.name AS grp
      FROM prefs
      JOIN settings ON settings.id = prefs.settingID
      JOIN groups ON groups.id = prefs.groupID
      WHERE settings.name = :name
      UNION
      SELECT NULL AS grp
      WHERE EXISTS (
        SELECT prefs.id
        FROM prefs
        JOIN settings ON settings.id = prefs.settingID
        WHERE settings.name = :name AND prefs.groupID IS NULL
      )
    `);
    stmt.params.name = name;
    stmts.push(stmt);

    // Delete the target settings.
    stmt = this._stmt(
      "DELETE FROM settings WHERE name = :name"
    );
    stmt.params.name = name;
    stmts.push(stmt);

    // Delete prefs and groups that are no longer used.
    stmts.push(this._stmt(
      "DELETE FROM prefs WHERE settingID NOT IN (SELECT id FROM settings)"
    ));
    stmts.push(this._stmt(`
      DELETE FROM groups WHERE id NOT IN (
        SELECT DISTINCT groupID FROM prefs WHERE groupID NOTNULL
      )
    `));

    let prefs = new ContentPrefStore();
    let isPrivate = context && context.usePrivateBrowsing;

    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        prefs.set(grp, name, undefined);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        if (ok && isPrivate) {
          for (let [sgroup, sname, ] of this._pbStore) {
            if (sname === name) {
              prefs.set(sgroup, name, undefined);
              this._pbStore.remove(sgroup, name);
            }
          }
        }
        cbHandleCompletion(callback, reason);
        if (ok) {
          for (let [sgroup, , ] of prefs) {
            this._notifyPrefRemoved(sgroup, name, isPrivate);
          }
        }
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  destroy: function CPS2_destroy() {
    if (this._statements) {
      for (let sql in this._statements) {
        let stmt = this._statements[sql];
        stmt.finalize();
      }
    }
  },

  /**
   * Returns the cached mozIStorageAsyncStatement for the given SQL.  If no such
   * statement is cached, one is created and cached.
   *
   * @param sql  The SQL query string.
   * @return     The cached, possibly new, statement.
   */
  _stmt: function CPS2__stmt(sql) {
    if (!this._statements)
      this._statements = {};
    if (!this._statements[sql])
      this._statements[sql] = this._dbConnection.createAsyncStatement(sql);
    return this._statements[sql];
  },

  /**
   * Executes some async statements.
   *
   * @param stmts      An array of mozIStorageAsyncStatements.
   * @param callbacks  An object with the following methods:
   *                   onRow(row) (optional)
   *                     Called once for each result row.
   *                     row: A mozIStorageRow.
   *                   onDone(reason, reasonOK, didGetRow) (required)
   *                     Called when done.
   *                     reason: A nsIContentPrefService2.COMPLETE_* value.
   *                     reasonOK: reason == nsIContentPrefService2.COMPLETE_OK.
   *                     didGetRow: True if onRow was ever called.
   *                   onError(nsresult) (optional)
   *                     Called on error.
   *                     nsresult: The error code.
   */
  _execStmts: function CPS2__execStmts(stmts, callbacks) {
    let self = this;
    let gotRow = false;
    this._dbConnection.executeAsync(stmts, stmts.length, {
      handleResult: function handleResult(results) {
        try {
          let row = null;
          while ((row = results.getNextRow())) {
            gotRow = true;
            if (callbacks.onRow)
              callbacks.onRow.call(self, row);
          }
        } catch (err) {
          Cu.reportError(err);
        }
      },
      handleCompletion: function handleCompletion(reason) {
        try {
          let ok = reason == Ci.mozIStorageStatementCallback.REASON_FINISHED;
          callbacks.onDone.call(self,
                                ok ? Ci.nsIContentPrefCallback2.COMPLETE_OK :
                                  Ci.nsIContentPrefCallback2.COMPLETE_ERROR,
                                ok, gotRow);
        } catch (err) {
          Cu.reportError(err);
        }
      },
      handleError: function handleError(error) {
        try {
          if (callbacks.onError)
            callbacks.onError.call(self, Cr.NS_ERROR_FAILURE);
        } catch (err) {
          Cu.reportError(err);
        }
      }
    });
  },

  __grouper: null,
  get _grouper() {
    if (!this.__grouper)
      this.__grouper = Cc["@mozilla.org/content-pref/hostname-grouper;1"].
                       getService(Ci.nsIContentURIGrouper);
    return this.__grouper;
  },

  /**
   * Parses the domain (the "group", to use the database's term) from the given
   * string.
   *
   * @param groupStr  Assumed to be either a string or falsey.
   * @return          If groupStr is a valid URL string, returns the domain of
   *                  that URL.  If groupStr is some other nonempty string,
   *                  returns groupStr itself.  Otherwise returns null.
   */
  _parseGroup: function CPS2__parseGroup(groupStr) {
    if (!groupStr)
      return null;
    try {
      var groupURI = Services.io.newURI(groupStr);
    } catch (err) {
      return groupStr;
    }
    return this._grouper.group(groupURI);
  },

  _schedule: function CPS2__schedule(fn) {
    Services.tm.dispatchToMainThread(fn.bind(this));
  },

  // A hash of arrays of observers, indexed by setting name.
  _observers: {},

  // An array of generic observers, which observe all settings.
  _genericObservers: [],

  addObserverForName: function CPS2_addObserverForName(aName, aObserver) {
    var observers;
    if (aName) {
      if (!this._observers[aName])
        this._observers[aName] = [];
      observers = this._observers[aName];
    } else
      observers = this._genericObservers;

    if (observers.indexOf(aObserver) == -1)
      observers.push(aObserver);
  },

  removeObserverForName: function CPS2_removeObserverForName(aName, aObserver) {
    var observers;
    if (aName) {
      if (!this._observers[aName])
        return;
      observers = this._observers[aName];
    } else
      observers = this._genericObservers;

    if (observers.indexOf(aObserver) != -1)
      observers.splice(observers.indexOf(aObserver), 1);
  },

  /**
   * Construct a list of observers to notify about a change to some setting,
   * putting setting-specific observers before before generic ones, so observers
   * that initialize individual settings (like the page style controller)
   * execute before observers that display multiple settings and depend on them
   * being initialized first (like the content prefs sidebar).
   */
  _getObservers: function ContentPrefService__getObservers(aName) {
    var observers = [];

    if (aName && this._observers[aName])
      observers = observers.concat(this._observers[aName]);
    observers = observers.concat(this._genericObservers);

    return observers;
  },

  /**
   * Notify all observers about the removal of a preference.
   */
  _notifyPrefRemoved: function ContentPrefService__notifyPrefRemoved(aGroup, aName, aIsPrivate) {
    for (var observer of this._getObservers(aName)) {
      try {
        observer.onContentPrefRemoved(aGroup, aName, aIsPrivate);
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  },

  /**
   * Notify all observers about a preference change.
   */
  _notifyPrefSet: function ContentPrefService__notifyPrefSet(aGroup, aName, aValue, aIsPrivate) {
    for (var observer of this._getObservers(aName)) {
      try {
        observer.onContentPrefSet(aGroup, aName, aValue, aIsPrivate);
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  },

  extractDomain: function CPS2_extractDomain(str) {
    return this._parseGroup(str);
  },

  /**
   * Tests use this as a backchannel by calling it directly.
   *
   * @param subj   This value depends on topic.
   * @param topic  The backchannel "method" name.
   * @param data   This value depends on topic.
   */
  observe: function CPS2_observe(subj, topic, data) {
    switch (topic) {
    case "xpcom-shutdown":
      this._destroy();
      break;
    case "last-pb-context-exited":
      this._pbStore.removeAll();
      break;
    case "test:reset":
      let fn = subj.QueryInterface(Ci.xpcIJSWeakReference).get();
      this._reset(fn);
      break;
    case "test:db":
      let obj = subj.QueryInterface(Ci.xpcIJSWeakReference).get();
      obj.value = this._dbConnection;
      break;
    }
  },

  /**
   * Removes all state from the service.  Used by tests.
   *
   * @param callback  A function that will be called when done.
   */
  _reset: function CPS2__reset(callback) {
    this._pbStore.removeAll();
    this._cache.removeAll();

    this._observers = {};
    this._genericObservers = [];

    let tables = ["prefs", "groups", "settings"];
    let stmts = tables.map(t => this._stmt(`DELETE FROM ${t}`));
    this._execStmts(stmts, { onDone: () => callback() });
  },

  QueryInterface: function CPS2_QueryInterface(iid) {
    let supportedIIDs = [
      Ci.nsIContentPrefService2,
      Ci.nsIObserver,
      Ci.nsISupports,
    ];
    if (supportedIIDs.some(i => iid.equals(i)))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },


  // Database Creation & Access

  _dbVersion: 4,

  _dbSchema: {
    tables: {
      groups:     "id           INTEGER PRIMARY KEY, \
                   name         TEXT NOT NULL",

      settings:   "id           INTEGER PRIMARY KEY, \
                   name         TEXT NOT NULL",

      prefs:      "id           INTEGER PRIMARY KEY, \
                   groupID      INTEGER REFERENCES groups(id), \
                   settingID    INTEGER NOT NULL REFERENCES settings(id), \
                   value        BLOB, \
                   timestamp    INTEGER NOT NULL DEFAULT 0" // Storage in seconds, API in ms. 0 for migrated values.
    },
    indices: {
      groups_idx: {
        table: "groups",
        columns: ["name"]
      },
      settings_idx: {
        table: "settings",
        columns: ["name"]
      },
      prefs_idx: {
        table: "prefs",
        columns: ["timestamp", "groupID", "settingID"]
      }
    }
  },

  _dbConnection: null,

  // _dbInit and the methods it calls (_dbCreate, _dbMigrate, and version-
  // specific migration methods) must be careful not to call any method
  // of the service that assumes the database connection has already been
  // initialized, since it won't be initialized until at the end of _dbInit.

  _dbInit: function ContentPrefService__dbInit() {
    var dbFile = Services.dirsvc.get("ProfD", Ci.nsIFile);
    dbFile.append("content-prefs.sqlite");

    var dbConnection;

    if (!dbFile.exists())
      dbConnection = this._dbCreate(dbFile);
    else {
      try {
        dbConnection = Services.storage.openDatabase(dbFile);
      } catch (e) {
        // If the connection isn't ready after we open the database, that means
        // the database has been corrupted, so we back it up and then recreate it.
        if (e.result != Cr.NS_ERROR_FILE_CORRUPTED)
          throw e;
        dbConnection = this._dbBackUpAndRecreate(dbFile, dbConnection);
      }

      // Get the version of the schema in the file.
      var version = dbConnection.schemaVersion;

      // Try to migrate the schema in the database to the current schema used by
      // the service.  If migration fails, back up the database and recreate it.
      if (version != this._dbVersion) {
        try {
          this._dbMigrate(dbConnection, version, this._dbVersion);
        } catch (ex) {
          Cu.reportError("error migrating DB: " + ex + "; backing up and recreating");
          dbConnection = this._dbBackUpAndRecreate(dbFile, dbConnection);
        }
      }
    }

    // Turn off disk synchronization checking to reduce disk churn and speed up
    // operations when prefs are changed rapidly (such as when a user repeatedly
    // changes the value of the browser zoom setting for a site).
    //
    // Note: this could cause database corruption if the OS crashes or machine
    // loses power before the data gets written to disk, but this is considered
    // a reasonable risk for the not-so-critical data stored in this database.
    //
    // If you really don't want to take this risk, however, just set the
    // toolkit.storage.synchronous pref to 1 (NORMAL synchronization) or 2
    // (FULL synchronization), in which case mozStorageConnection::Initialize
    // will use that value, and we won't override it here.
    if (!Services.prefs.prefHasUserValue("toolkit.storage.synchronous"))
      dbConnection.executeSimpleSQL("PRAGMA synchronous = OFF");

    this._dbConnection = dbConnection;
  },

  _dbCreate: function ContentPrefService__dbCreate(aDBFile) {
    var dbConnection = Services.storage.openDatabase(aDBFile);

    try {
      this._dbCreateSchema(dbConnection);
      dbConnection.schemaVersion = this._dbVersion;
    } catch (ex) {
      // If we failed to create the database (perhaps because the disk ran out
      // of space), then remove the database file so we don't leave it in some
      // half-created state from which we won't know how to recover.
      dbConnection.close();
      aDBFile.remove(false);
      throw ex;
    }

    return dbConnection;
  },

  _dbCreateSchema: function ContentPrefService__dbCreateSchema(aDBConnection) {
    this._dbCreateTables(aDBConnection);
    this._dbCreateIndices(aDBConnection);
  },

  _dbCreateTables: function ContentPrefService__dbCreateTables(aDBConnection) {
    for (let name in this._dbSchema.tables)
      aDBConnection.createTable(name, this._dbSchema.tables[name]);
  },

  _dbCreateIndices: function ContentPrefService__dbCreateIndices(aDBConnection) {
    for (let name in this._dbSchema.indices) {
      let index = this._dbSchema.indices[name];
      let statement = `
        CREATE INDEX IF NOT EXISTS ${name} ON ${index.table}
        (${index.columns.join(", ")})
      `;
      aDBConnection.executeSimpleSQL(statement);
    }
  },

  _dbBackUpAndRecreate: function ContentPrefService__dbBackUpAndRecreate(aDBFile,
                                                                         aDBConnection) {
    Services.storage.backupDatabaseFile(aDBFile, "content-prefs.sqlite.corrupt");

    // Close the database, ignoring the "already closed" exception, if any.
    // It'll be open if we're here because of a migration failure but closed
    // if we're here because of database corruption.
    try { aDBConnection.close(); } catch (ex) {}

    aDBFile.remove(false);

    let dbConnection = this._dbCreate(aDBFile);

    return dbConnection;
  },

  _dbMigrate: function ContentPrefService__dbMigrate(aDBConnection, aOldVersion, aNewVersion) {
    /**
     * Migrations should follow the template rules in bug 1074817 comment 3 which are:
     * 1. Migration should be incremental and non-breaking.
     * 2. It should be idempotent because one can downgrade an upgrade again.
     * On downgrade:
     * 1. Decrement schema version so that upgrade runs the migrations again.
     */
    aDBConnection.beginTransaction();

    try {
       /**
       * If the schema version is 0, that means it was never set, which means
       * the database was somehow created without the schema being applied, perhaps
       * because the system ran out of disk space (although we check for this
       * in _createDB) or because some other code created the database file without
       * applying the schema.  In any case, recover by simply reapplying the schema.
       */
      if (aOldVersion == 0) {
        this._dbCreateSchema(aDBConnection);
      } else {
        for (let i = aOldVersion; i < aNewVersion; i++) {
          let migrationName = "_dbMigrate" + i + "To" + (i + 1);
          if (typeof this[migrationName] != "function") {
            throw new Error("no migrator function from version " + aOldVersion + " to version " +
                            aNewVersion);
          }
          this[migrationName](aDBConnection);
        }
      }
      aDBConnection.schemaVersion = aNewVersion;
      aDBConnection.commitTransaction();
    } catch (ex) {
      aDBConnection.rollbackTransaction();
      throw ex;
    }
  },

  _dbMigrate1To2: function ContentPrefService___dbMigrate1To2(aDBConnection) {
    aDBConnection.executeSimpleSQL("ALTER TABLE groups RENAME TO groupsOld");
    aDBConnection.createTable("groups", this._dbSchema.tables.groups);
    aDBConnection.executeSimpleSQL(`
      INSERT INTO groups (id, name)
      SELECT id, name FROM groupsOld
    `);

    aDBConnection.executeSimpleSQL("DROP TABLE groupers");
    aDBConnection.executeSimpleSQL("DROP TABLE groupsOld");
  },

  _dbMigrate2To3: function ContentPrefService__dbMigrate2To3(aDBConnection) {
    this._dbCreateIndices(aDBConnection);
  },

  _dbMigrate3To4: function ContentPrefService__dbMigrate3To4(aDBConnection) {
    // Add timestamp column if it does not exist yet. This operation is idempotent.
    try {
      let stmt = aDBConnection.createStatement("SELECT timestamp FROM prefs");
      stmt.finalize();
    } catch (e) {
      aDBConnection.executeSimpleSQL("ALTER TABLE prefs ADD COLUMN timestamp INTEGER NOT NULL DEFAULT 0");
    }

    // To modify prefs_idx drop it and create again.
    aDBConnection.executeSimpleSQL("DROP INDEX IF EXISTS prefs_idx");
    this._dbCreateIndices(aDBConnection);
  },
};

function checkGroupArg(group) {
  if (!group || typeof(group) != "string")
    throw invalidArg("domain must be nonempty string.");
}

function checkNameArg(name) {
  if (!name || typeof(name) != "string")
    throw invalidArg("name must be nonempty string.");
}

function checkValueArg(value) {
  if (value === undefined)
    throw invalidArg("value must not be undefined.");
}

function checkCallbackArg(callback, required) {
  if (callback && !(callback instanceof Ci.nsIContentPrefCallback2))
    throw invalidArg("callback must be an nsIContentPrefCallback2.");
  if (!callback && required)
    throw invalidArg("callback must be given.");
}

function invalidArg(msg) {
  return Components.Exception(msg, Cr.NS_ERROR_INVALID_ARG);
}


function HostnameGrouper() {}

HostnameGrouper.prototype = {
  // XPCOM Plumbing

  classID:          Components.ID("{8df290ae-dcaa-4c11-98a5-2429a4dc97bb}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIContentURIGrouper]),

  // nsIContentURIGrouper

  group: function HostnameGrouper_group(aURI) {
    var group;

    try {
      // Accessing the host property of the URI will throw an exception
      // if the URI is of a type that doesn't have a host property.
      // Otherwise, we manually throw an exception if the host is empty,
      // since the effect is the same (we can't derive a group from it).

      group = aURI.host;
      if (!group)
        throw new Error("can't derive group from host; no host in URI");
    } catch (ex) {
      // If we don't have a host, then use the entire URI (minus the query,
      // reference, and hash, if possible) as the group.  This means that URIs
      // like about:mozilla and about:blank will be considered separate groups,
      // but at least they'll be grouped somehow.

      // This also means that each individual file: URL will be considered
      // its own group.  This seems suboptimal, but so does treating the entire
      // file: URL space as a single group (especially if folks start setting
      // group-specific capabilities prefs).

      // XXX Is there something better we can do here?

      try {
        var url = aURI.QueryInterface(Ci.nsIURL);
        group = aURI.prePath + url.filePath;
      } catch (ex) {
        group = aURI.spec;
      }
    }

    return group;
  }
};

// XPCOM Plumbing

var components = [ContentPrefService2, HostnameGrouper];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
