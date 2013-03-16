/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is an XPCOM component that implements nsIContentPrefService2.
// Although it's a JSM, it's not intended to be imported by consumers like JSMs
// are usually imported.  It's only a JSM so that nsContentPrefService.js can
// easily use it.  Consumers should access this component with the usual XPCOM
// rigmarole:
//
//   Cc["@mozilla.org/content-pref/service;1"].
//   getService(Ci.nsIContentPrefService2);
//
// That contract ID actually belongs to nsContentPrefService.js, which, when
// QI'ed to nsIContentPrefService2, returns an instance of this component.
//
// The plan is to eventually remove nsIContentPrefService and its
// implementation, nsContentPrefService.js.  At such time this file can stop
// being a JSM, and the "_cps" parts that ContentPrefService2 relies on and
// NSGetFactory and all the other XPCOM initialization goop in
// nsContentPrefService.js can be moved here.
//
// See https://bugzilla.mozilla.org/show_bug.cgi?id=699859

let EXPORTED_SYMBOLS = [
  "ContentPrefService2",
];

const { interfaces: Ci, classes: Cc, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/ContentPrefStore.jsm");

function ContentPrefService2(cps) {
  this._cps = cps;
  this._cache = cps._cache;
  this._pbStore = cps._privModeStorage;
}

ContentPrefService2.prototype = {

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
      for (let [sgroup, val] in
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
          for (let [pbGroup, pbName, pbVal] in pbPrefs) {
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
      this._stmtWithGroupClause(group, includeSubdomains,
        "SELECT groups.name AS grp, prefs.value AS value",
        "FROM prefs",
        "JOIN settings ON settings.id = prefs.settingID",
        "JOIN groups ON groups.id = prefs.groupID",
        "WHERE settings.name = :name AND prefs.groupID IN ($)"
      ) :
      this._stmt(
        "SELECT NULL AS grp, prefs.value AS value",
        "FROM prefs",
        "JOIN settings ON settings.id = prefs.settingID",
        "WHERE settings.name = :name AND prefs.groupID ISNULL"
      );
    stmt.params.name = name;
    return stmt;
  },

  _stmtWithGroupClause: function CPS2__stmtWithGroupClause(group,
                                                           includeSubdomains) {
    let stmt = this._stmt(joinArgs(Array.slice(arguments, 2)).replace("$",
      "SELECT id " +
      "FROM groups " +
      "WHERE name = :group OR " +
            "(:includeSubdomains AND name LIKE :pattern ESCAPE '/')"
    ));
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
    storesToCheck.forEach(function (store) {
      for (let [sgroup, val] in store.match(group, name, includeSubdomains)) {
        outStore.set(sgroup, name, val);
      }
    });

    let prefs = [];
    for (let [sgroup, sname, val] in outStore) {
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
      this._schedule(function () {
        this._cps._broadcastPrefSet(group, name, value);
        cbHandleCompletion(callback, Ci.nsIContentPrefCallback2.COMPLETE_OK);
      });
      return;
    }

    let stmts = [];

    // Create the setting if it doesn't exist.
    let stmt = this._stmt(
      "INSERT OR IGNORE INTO settings (id, name)",
      "VALUES((SELECT id FROM settings WHERE name = :name), :name)"
    );
    stmt.params.name = name;
    stmts.push(stmt);

    // Create the group if it doesn't exist.
    if (group) {
      stmt = this._stmt(
        "INSERT OR IGNORE INTO groups (id, name)",
        "VALUES((SELECT id FROM groups WHERE name = :group), :group)"
      );
      stmt.params.group = group;
      stmts.push(stmt);
    }

    // Finally create or update the pref.
    if (group) {
      stmt = this._stmt(
        "INSERT OR REPLACE INTO prefs (id, groupID, settingID, value)",
        "VALUES(",
          "(SELECT prefs.id",
           "FROM prefs",
           "JOIN groups ON groups.id = prefs.groupID",
           "JOIN settings ON settings.id = prefs.settingID",
           "WHERE groups.name = :group AND settings.name = :name),",
          "(SELECT id FROM groups WHERE name = :group),",
          "(SELECT id FROM settings WHERE name = :name),",
          ":value",
        ")"
      );
      stmt.params.group = group;
    }
    else {
      stmt = this._stmt(
        "INSERT OR REPLACE INTO prefs (id, groupID, settingID, value)",
        "VALUES(",
          "(SELECT prefs.id",
           "FROM prefs",
           "JOIN settings ON settings.id = prefs.settingID",
           "WHERE prefs.groupID IS NULL AND settings.name = :name),",
          "NULL,",
          "(SELECT id FROM settings WHERE name = :name),",
          ":value",
        ")"
      );
    }
    stmt.params.name = name;
    stmt.params.value = value;
    stmts.push(stmt);

    this._execStmts(stmts, {
      onDone: function onDone(reason, ok) {
        if (ok) {
          this._cache.setWithCast(group, name, value);
          this._cps._broadcastPrefSet(group, name, value);
        }
        cbHandleCompletion(callback, reason);
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

  removeGlobal: function CPS2_removeGlobal(name, context,callback) {
    this._remove(null, name, false, context, callback);
  },

  _remove: function CPS2__remove(group, name, includeSubdomains, context,
                                 callback) {
    group = this._parseGroup(group);
    checkNameArg(name);
    checkCallbackArg(callback, false);

    let stmts = [];

    // First get the matching prefs.
    stmts.push(this._commonGetStmt(group, name, includeSubdomains));

    // Delete the matching prefs.
    let stmt = this._stmtWithGroupClause(group, includeSubdomains,
      "DELETE FROM prefs",
      "WHERE settingID = (SELECT id FROM settings WHERE name = :name) AND",
            "CASE typeof(:group)",
            "WHEN 'null' THEN prefs.groupID IS NULL",
            "ELSE prefs.groupID IN ($)",
            "END"
    );
    stmt.params.name = name;
    stmts.push(stmt);

    // Delete settings and groups that are no longer used.  The NOTNULL term in
    // the subquery of the second statment is needed because of SQLite's weird
    // IN behavior vis-a-vis NULLs.  See http://sqlite.org/lang_expr.html.
    stmts.push(this._stmt(
      "DELETE FROM settings",
      "WHERE id NOT IN (SELECT DISTINCT settingID FROM prefs)"
    ));
    stmts.push(this._stmt(
      "DELETE FROM groups WHERE id NOT IN (",
        "SELECT DISTINCT groupID FROM prefs WHERE groupID NOTNULL",
      ")"
    ));

    let prefs = new ContentPrefStore();

    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        prefs.set(grp, name);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        if (ok) {
          this._cache.set(group, name, undefined);
          if (context && context.usePrivateBrowsing) {
            for (let [sgroup, ] in
                   this._pbStore.match(group, name, includeSubdomains)) {
              prefs.set(sgroup, name);
              this._pbStore.remove(sgroup, name);
            }
          }
          for (let [sgroup, , ] in prefs) {
            this._cps._broadcastPrefRemoved(sgroup, name);
          }
        }
        cbHandleCompletion(callback, reason);
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
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

    let stmts = [];

    // First get the matching prefs, then delete groups and prefs that reference
    // deleted groups.
    if (group) {
      stmts.push(this._stmtWithGroupClause(group, includeSubdomains,
        "SELECT groups.name AS grp, settings.name AS name",
        "FROM prefs",
        "JOIN settings ON settings.id = prefs.settingID",
        "JOIN groups ON groups.id = prefs.groupID",
        "WHERE prefs.groupID IN ($)"
      ));
      stmts.push(this._stmtWithGroupClause(group, includeSubdomains,
        "DELETE FROM groups WHERE id IN ($)"
      ));
      stmts.push(this._stmt(
        "DELETE FROM prefs",
        "WHERE groupID NOTNULL AND groupID NOT IN (SELECT id FROM groups)"
      ));
    }
    else {
      stmts.push(this._stmt(
        "SELECT NULL AS grp, settings.name AS name",
        "FROM prefs",
        "JOIN settings ON settings.id = prefs.settingID",
        "WHERE prefs.groupID IS NULL"
      ));
      stmts.push(this._stmt(
        "DELETE FROM prefs WHERE groupID IS NULL"
      ));
    }

    // Finally delete settings that are no longer referenced.
    stmts.push(this._stmt(
      "DELETE FROM settings",
      "WHERE id NOT IN (SELECT DISTINCT settingID FROM prefs)"
    ));

    let prefs = new ContentPrefStore();

    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        let name = row.getResultByName("name");
        prefs.set(grp, name);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        if (ok) {
          if (context && context.usePrivateBrowsing) {
            for (let [sgroup, sname, ] in this._pbStore) {
              prefs.set(sgroup, sname);
              this._pbStore.remove(sgroup, sname);
            }
          }
          for (let [sgroup, sname, ] in prefs) {
            this._cps._broadcastPrefRemoved(sgroup, sname);
          }
        }
        cbHandleCompletion(callback, reason);
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  removeAllDomains: function CPS2_removeAllDomains(context, callback) {
    checkCallbackArg(callback, false);
    let stmts = [];

    // First get the matching prefs.
    stmts.push(this._stmt(
      "SELECT groups.name AS grp, settings.name AS name",
      "FROM prefs",
      "JOIN settings ON settings.id = prefs.settingID",
      "JOIN groups ON groups.id = prefs.groupID"
    ));

    stmts.push(this._stmt(
      "DELETE FROM prefs WHERE groupID NOTNULL"
    ));
    stmts.push(this._stmt(
      "DELETE FROM groups"
    ));
    stmts.push(this._stmt(
      "DELETE FROM settings",
      "WHERE id NOT IN (SELECT DISTINCT settingID FROM prefs)"
    ));

    let prefs = new ContentPrefStore();

    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        let name = row.getResultByName("name");
        prefs.set(grp, name);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        if (ok) {
          if (context && context.usePrivateBrowsing) {
            for (let [sgroup, sname, ] in this._pbStore) {
              prefs.set(sgroup, sname);
            }
            this._pbStore.removeGrouped();
          }
          for (let [sgroup, sname, ] in prefs) {
            this._cps._broadcastPrefRemoved(sgroup, sname);
          }
        }
        cbHandleCompletion(callback, reason);
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  removeByName: function CPS2_removeByName(name, context, callback) {
    checkNameArg(name);
    checkCallbackArg(callback, false);

    let stmts = [];

    // First get the matching prefs.  Include null if any of those prefs are
    // global.
    let stmt = this._stmt(
      "SELECT groups.name AS grp",
      "FROM prefs",
      "JOIN settings ON settings.id = prefs.settingID",
      "JOIN groups ON groups.id = prefs.groupID",
      "WHERE settings.name = :name",
      "UNION",
      "SELECT NULL AS grp",
      "WHERE EXISTS (",
        "SELECT prefs.id",
        "FROM prefs",
        "JOIN settings ON settings.id = prefs.settingID",
        "WHERE settings.name = :name AND prefs.groupID IS NULL",
      ")"
    );
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
    stmts.push(this._stmt(
      "DELETE FROM groups WHERE id NOT IN (",
        "SELECT DISTINCT groupID FROM prefs WHERE groupID NOTNULL",
      ")"
    ));

    let prefs = new ContentPrefStore();

    this._execStmts(stmts, {
      onRow: function onRow(row) {
        let grp = row.getResultByName("grp");
        prefs.set(grp, name);
        this._cache.set(grp, name, undefined);
      },
      onDone: function onDone(reason, ok) {
        if (ok) {
          if (context && context.usePrivateBrowsing) {
            for (let [sgroup, sname, ] in this._pbStore) {
              if (sname === name) {
                prefs.set(sgroup, name);
                this._pbStore.remove(sgroup, name);
              }
            }
          }
          for (let [sgroup, , ] in prefs) {
            this._cps._broadcastPrefRemoved(sgroup, name);
          }
        }
        cbHandleCompletion(callback, reason);
      },
      onError: function onError(nsresult) {
        cbHandleError(callback, nsresult);
      }
    });
  },

  destroy: function CPS2_destroy() {
    for each (let stmt in this._statements) {
      stmt.finalize();
    }
  },

  /**
   * Returns the cached mozIStorageAsyncStatement for the given SQL.  If no such
   * statement is cached, one is created and cached.
   *
   * @param sql  The SQL query string.  If more than one string is given, then
   *             all are concatenated.  The concatenation process inserts
   *             spaces where appropriate and removes unnecessary contiguous
   *             spaces.  Call like _stmt("SELECT *", "FROM foo").
   * @return     The cached, possibly new, statement.
   */
  _stmt: function CPS2__stmt(sql /*, sql2, sql3, ... */) {
    let sql = joinArgs(arguments);
    if (!this._statements)
      this._statements = {};
    if (!this._statements[sql])
      this._statements[sql] = this._cps._dbConnection.createAsyncStatement(sql);
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
    this._cps._dbConnection.executeAsync(stmts, stmts.length, {
      handleResult: function handleResult(results) {
        try {
          let row = null;
          while ((row = results.getNextRow())) {
            gotRow = true;
            if (callbacks.onRow)
              callbacks.onRow.call(self, row);
          }
        }
        catch (err) {
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
        }
        catch (err) {
          Cu.reportError(err);
        }
      },
      handleError: function handleError(error) {
        try {
          if (callbacks.onError)
            callbacks.onError.call(self, Cr.NS_ERROR_FAILURE);
        }
        catch (err) {
          Cu.reportError(err);
        }
      }
    });
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
      var groupURI = Services.io.newURI(groupStr, null, null);
    }
    catch (err) {
      return groupStr;
    }
    return this._cps.grouper.group(groupURI);
  },

  _schedule: function CPS2__schedule(fn) {
    Services.tm.mainThread.dispatch(fn.bind(this),
                                    Ci.nsIThread.DISPATCH_NORMAL);
  },

  addObserverForName: function CPS2_addObserverForName(name, observer) {
    this._cps.addObserver(name, observer);
  },

  removeObserverForName: function CPS2_removeObserverForName(name, observer) {
    this._cps.removeObserver(name, observer);
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
    case "test:reset":
      let fn = subj.QueryInterface(Ci.xpcIJSWeakReference).get();
      this._reset(fn);
      break;
    case "test:db":
      let obj = subj.QueryInterface(Ci.xpcIJSWeakReference).get();
      obj.value = this._cps._dbConnection;
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

    let cps = this._cps;
    cps._observers = {};
    cps._genericObservers = [];

    let tables = ["prefs", "groups", "settings"];
    let stmts = tables.map(function (t) this._stmt("DELETE FROM", t), this);
    this._execStmts(stmts, { onDone: function () callback() });
  },

  QueryInterface: function CPS2_QueryInterface(iid) {
    let supportedIIDs = [
      Ci.nsIContentPrefService2,
      Ci.nsIObserver,
      Ci.nsISupports,
    ];
    if (supportedIIDs.some(function (i) iid.equals(i)))
      return this;
    if (iid.equals(Ci.nsIContentPrefService))
      return this._cps;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
};

function ContentPref(domain, name, value) {
  this.domain = domain;
  this.name = name;
  this.value = value;
}

ContentPref.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPref]),
};

function cbHandleResult(callback, pref) {
  safeCallback(callback, "handleResult", [pref]);
}

function cbHandleCompletion(callback, reason) {
  safeCallback(callback, "handleCompletion", [reason]);
}

function cbHandleError(callback, nsresult) {
  safeCallback(callback, "handleError", [nsresult]);
}

function safeCallback(callbackObj, methodName, args) {
  if (!callbackObj || typeof(callbackObj[methodName]) != "function")
    return;
  try {
    callbackObj[methodName].apply(callbackObj, args);
  }
  catch (err) {
    Cu.reportError(err);
  }
}

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

function joinArgs(args) {
  return Array.join(args, " ").trim().replace(/\s{2,}/g, " ");
}
