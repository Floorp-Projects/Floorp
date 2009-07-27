/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

function FormAutoComplete() {
    this.init();
}

FormAutoComplete.prototype = {
    classDescription: "FormAutoComplete",
    contractID: "@mozilla.org/satchel/form-autocomplete;1",
    classID: Components.ID("{c11c21b2-71c9-4f87-a0f8-5e13f50495fd}"),
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormAutoComplete, Ci.nsISupportsWeakReference]),

    __logService : null, // Console logging service, used for debugging.
    get _logService() {
        if (!this.__logService)
            this.__logService = Cc["@mozilla.org/consoleservice;1"].
                                getService(Ci.nsIConsoleService);
        return this.__logService;
    },

    __formHistory : null,
    get _formHistory() {
        if (!this.__formHistory)
            this.__formHistory = Cc["@mozilla.org/satchel/form-history;1"].
                                 getService(Ci.nsIFormHistory2);
        return this.__formHistory;
    },

    __observerService : null, // Observer Service, for notifications
    get _observerService() {
        if (!this.__observerService)
            this.__observerService = Cc["@mozilla.org/observer-service;1"].
                                     getService(Ci.nsIObserverService);
        return this.__observerService;
    },

    _prefBranch        : null,
    _debug             : false, // mirrors browser.formfill.debug
    _enabled           : true,  // mirrors browser.formfill.enable preference
    _agedWeight        : 2,
    _bucketSize        : 5,
    _maxTimeGroupings  : 25,
    _timeGroupingSize  : 7 * 24 * 60 * 60 * 1000 * 1000,
    _expireDays        : null,

    init : function() {
        // Preferences. Add observer so we get notified of changes.
        this._prefBranch = Cc["@mozilla.org/preferences-service;1"].
                           getService(Ci.nsIPrefService).getBranch("browser.formfill.");
        this._prefBranch.QueryInterface(Ci.nsIPrefBranch2);
        this._prefBranch.addObserver("", this.observer, false);
        this.observer._self = this;

        this._debug   = this._prefBranch.getBoolPref("debug");
        this._enabled = this._prefBranch.getBoolPref("enable");
        this._agedWeight = this._prefBranch.getIntPref("agedWeight");
        this._bucketSize = this._prefBranch.getIntPref("bucketSize");
        this._maxTimeGroupings = this._prefBranch.getIntPref("maxTimeGroupings");
        this._timeGroupingSize = this._prefBranch.getIntPref("timeGroupingSize") * 1000 * 1000;
        this._expireDays = this._getFormExpiryDays();

        this._dbStmts = [];

        this._observerService.addObserver(this.observer, "xpcom-shutdown", false);
    },

    observer : {
        _self : null,

        QueryInterface : XPCOMUtils.generateQI([Ci.nsIObserver,
                                                Ci.nsISupportsWeakReference]),

        observe : function (subject, topic, data) {
            let self = this._self;
            if (topic == "nsPref:changed") {
                let prefName = data;
                self.log("got change to " + prefName + " preference");

                switch (prefName) {
                    case "agedWeight":
                        self._agedWeight = self._prefBranch.getIntPref(prefName);
                        break;
                    case "debug":
                        self._debug = self._prefBranch.getBoolPref(prefName);
                        break;
                    case "enable":
                        self._enabled = self._prefBranch.getBoolPref(prefName);
                        break;
                    case "maxTimeGroupings":
                        self._maxTimeGroupings = self._prefBranch.getIntPref(prefName);
                        break;
                    case "timeGroupingSize":
                        self._timeGroupingSize = self._prefBranch.getIntPref(prefName) * 1000 * 1000;
                        break;
                    case "bucketSize":
                        self._bucketSize = self._prefBranch.getIntPref(prefName);
                        break;
                    default:
                        self.log("Oops! Pref not handled, change ignored.");
                }
            } else if (topic == "xpcom-shutdown") {
                self._dbStmts = null;
            }
        }
    },


    /*
     * log
     *
     * Internal function for logging debug messages to the Error Console
     * window
     */
    log : function (message) {
        if (!this._debug)
            return;
        dump("FormAutoComplete: " + message + "\n");
        this._logService.logStringMessage("FormAutoComplete: " + message);
    },


    /*
     * autoCompleteSearch
     *
     * aInputName    -- |name| attribute from the form input being autocompleted.
     * aSearchString -- current value of the input
     * aPreviousResult -- previous search result, if any.
     *
     * Returns: an nsIAutoCompleteResult
     */
    autoCompleteSearch : function (aInputName, aSearchString, aPreviousResult) {
        if (!this._enabled)
            return null;

        this.log("AutoCompleteSearch invoked. Search is: " + aSearchString);

        let result = null;

        if (aPreviousResult &&
                aSearchString.substr(0, aPreviousResult.searchString.length) == aPreviousResult.searchString) {
            this.log("Using previous autocomplete result");
            result = aPreviousResult;
            result.wrappedJSObject.searchString = aSearchString;

            // We have a list of results for a shorter search string, so just
            // filter them further based on the new search string.
            // Count backwards, because result.matchCount is decremented
            // when we remove an entry.
            for (let i = result.matchCount - 1; i >= 0; i--) {
                let match = result.getValueAt(i);

                // Remove results that are too short, or have different prefix.
                // XXX bug 394604 -- .toLowerCase can be wrong for some intl chars
                if (aSearchString.length > match.length ||
                    aSearchString.toLowerCase() !=
                        match.substr(0, aSearchString.length).toLowerCase())
                {
                    this.log("Removing autocomplete entry '" + match + "'");
                    result.removeValueAt(i, false);
                }
            }
        } else {
            this.log("Creating new autocomplete search result.");
            let entries = this.getAutoCompleteValues(aInputName, aSearchString);
            result = new FormAutoCompleteResult(this._formHistory, entries, aInputName, aSearchString);
        }

        return result;
    },

    getAutoCompleteValues : function (fieldName, searchString) {
        let values = [];
        /* Three factors in the frecency calculation for an entry (in order of use in calculation):
         * 1) average number of times used - items used more are ranked higher
         * 2) how recently it was last used - items used recently are ranked higher
         * 3) additional weight for aged entries surviving expiry - these entries are relevant
         *    since they have been used multiple times over a large time span so rank them higher
         * The score is then divided by the bucket size and we round the result so that entries
         * with a very similar frecency are bucketed together with an alphabetical sort. This is
         * to reduce the amount of moving around by entries while typing.
         */
        let query = "SELECT value, " +
                    "ROUND( " +
                        "timesUsed / MAX(1.0, (lastUsed - firstUsed) / :timeGroupingSize) * " +
                        "MAX(1.0, :maxTimeGroupings - (:now - lastUsed) / :timeGroupingSize) * "+
                        "MAX(1.0, :agedWeight * (firstUsed < :expiryDate)) / " +
                        ":bucketSize "+
                    ") AS frecency " +
                    "FROM moz_formhistory " +
                    "WHERE fieldname=:fieldname AND value LIKE :valuePrefix ESCAPE '/' " +
                    "ORDER BY frecency DESC, UPPER(value) ASC";

        let params = {
            agedWeight:         this._agedWeight,
            bucketSize:         this._bucketSize,
            expiryDate:         1000 * (Date.now() - this._expireDays * 24 * 60 * 60 * 1000),
            fieldname:          fieldName,
            maxTimeGroupings:   this._maxTimeGroupings,
            now:                Date.now() * 1000,          // convert from ms to microseconds
            timeGroupingSize:   this._timeGroupingSize,
            valuePrefix:        null                        // set below...
        }

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);

            // Chicken and egg problem: Need the statement to escape the params we
            // pass to the function that gives us the statement. So, fix it up now.
            stmt.params.valuePrefix = stmt.escapeStringForLIKE(searchString, "/") + "%";

            while (stmt.step())
                values.push(stmt.row.value);

        } catch (e) {
            this.log("getValues failed: " + e.name + " : " + e.message);
            throw "DB failed getting form autocomplete values";
        } finally {
            stmt.reset();
        }

        return values;
    },


    _dbStmts      : null,

    _dbCreateStatement : function (query, params) {
        let stmt = this._dbStmts[query];
        // Memoize the statements
        if (!stmt) {
            this.log("Creating new statement for query: " + query);
            stmt = this._formHistory.DBConnection.createStatement(query);
            this._dbStmts[query] = stmt;
        }
        // Replace parameters, must be done 1 at a time
        if (params) {
            let stmtparams = stmt.params;
            for (let i in params)
                stmtparams[i] = params[i];
        }
        return stmt;
    },

    _getFormExpiryDays : function () {
        let prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                          getService(Ci.nsIPrefBranch);
        if (prefsBranch.prefHasUserValue("browser.formfill.expire_days"))
            return prefsBranch.getIntPref("browser.formfill.expire_days");
        else
            return prefsBranch.getIntPref("browser.history_expire_days");
    }

}; // end of FormAutoComplete implementation




// nsIAutoCompleteResult implementation
function FormAutoCompleteResult (formHistory, entries, fieldName, searchString) {
    this.formHistory = formHistory;
    this.entries = entries;
    this.fieldName = fieldName;
    this.searchString = searchString;

    if (entries.length > 0) {
        this.searchResult = Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
        this.defaultIndex = 0;
    }
}

FormAutoCompleteResult.prototype = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIAutoCompleteResult,
                                            Ci.nsISupportsWeakReference]),

    // private
    formHistory : null,
    entries : null,
    fieldName : null,

    _checkIndexBounds : function (index) {
        if (index < 0 || index >= this.entries.length)
            throw Components.Exception("Index out of range.", Cr.NS_ERROR_ILLEGAL_VALUE);
    },

    // Allow autoCompleteSearch to get at the JS object so it can
    // modify some readonly properties for internal use.
    get wrappedJSObject() {
        return this;
    },

    // Interfaces from idl...
    searchString : null,
    searchResult : Ci.nsIAutoCompleteResult.RESULT_NOMATCH,
    defaultIndex : -1,
    errorDescription : "",
    get matchCount() {
        return this.entries.length;
    },

    getValueAt : function (index) {
        this._checkIndexBounds(index);
        return this.entries[index];
    },

    getCommentAt : function (index) {
        this._checkIndexBounds(index);
        return "";
    },

    getStyleAt : function (index) {
        this._checkIndexBounds(index);
        return "";
    },

    getImageAt : function (index) {
        this._checkIndexBounds(index);
        return "";
    },

    removeValueAt : function (index, removeFromDB) {
        this._checkIndexBounds(index);

        let [removedEntry] = this.entries.splice(index, 1);

        if (this.defaultIndex > this.entries.length)
            this.defaultIndex--;

        if (removeFromDB)
            this.formHistory.removeEntry(this.fieldName, removedEntry);
    }
};

let component = [FormAutoComplete];
function NSGetModule (compMgr, fileSpec) {
    return XPCOMUtils.generateModule(component);
}
