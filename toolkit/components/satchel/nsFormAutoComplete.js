/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function FormAutoComplete() {
    this.init();
}

FormAutoComplete.prototype = {
    classID          : Components.ID("{c11c21b2-71c9-4f87-a0f8-5e13f50495fd}"),
    QueryInterface   : XPCOMUtils.generateQI([Ci.nsIFormAutoComplete, Ci.nsISupportsWeakReference]),

    __formHistory : null,
    get _formHistory() {
        if (!this.__formHistory)
            this.__formHistory = Cc["@mozilla.org/satchel/form-history;1"].
                                 getService(Ci.nsIFormHistory2);
        return this.__formHistory;
    },

    _prefBranch         : null,
    _debug              : true, // mirrors browser.formfill.debug
    _enabled            : true, // mirrors browser.formfill.enable preference
    _agedWeight         : 2,
    _bucketSize         : 1,
    _maxTimeGroupings   : 25,
    _timeGroupingSize   : 7 * 24 * 60 * 60 * 1000 * 1000,
    _expireDays         : null,
    _boundaryWeight     : 25,
    _prefixWeight       : 5,

    init : function() {
        // Preferences. Add observer so we get notified of changes.
        this._prefBranch = Services.prefs.getBranch("browser.formfill.");
        this._prefBranch.addObserver("", this.observer, true);
        this.observer._self = this;

        this._debug            = this._prefBranch.getBoolPref("debug");
        this._enabled          = this._prefBranch.getBoolPref("enable");
        this._agedWeight       = this._prefBranch.getIntPref("agedWeight");
        this._bucketSize       = this._prefBranch.getIntPref("bucketSize");
        this._maxTimeGroupings = this._prefBranch.getIntPref("maxTimeGroupings");
        this._timeGroupingSize = this._prefBranch.getIntPref("timeGroupingSize") * 1000 * 1000;
        this._expireDays       = this._prefBranch.getIntPref("expire_days");

        this._dbStmts = {};

        Services.obs.addObserver(this.observer, "profile-before-change", true);
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
                    case "boundaryWeight":
                        self._boundaryWeight = self._prefBranch.getIntPref(prefName);
                        break;
                    case "prefixWeight":
                        self._prefixWeight = self._prefBranch.getIntPref(prefName);
                        break;
                    default:
                        self.log("Oops! Pref not handled, change ignored.");
                }
            } else if (topic == "profile-before-change") {
                for each (let stmt in self._dbStmts) {
                    stmt.finalize();
                }
                self._dbStmts = {};
                self.__formHistory = null;
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
        Services.console.logStringMessage("FormAutoComplete: " + message);
    },


    /*
     * autoCompleteSearch
     *
     * aInputName    -- |name| attribute from the form input being autocompleted.
     * aUntrimmedSearchString -- current value of the input
     * aField -- nsIDOMHTMLInputElement being autocompleted (may be null if from chrome)
     * aPreviousResult -- previous search result, if any.
     *
     * Returns: an nsIAutoCompleteResult
     */
    autoCompleteSearch : function (aInputName, aUntrimmedSearchString, aField, aPreviousResult) {
        function sortBytotalScore (a, b) {
            return b.totalScore - a.totalScore;
        }

        if (!this._enabled)
            return null;

        // don't allow form inputs (aField != null) to get results from search bar history
        if (aInputName == 'searchbar-history' && aField) {
            this.log('autoCompleteSearch for input name "' + aInputName + '" is denied');
            return null;
        }

        this.log("AutoCompleteSearch invoked. Search is: " + aUntrimmedSearchString);
        let searchString = aUntrimmedSearchString.trim().toLowerCase();
        let result = null;

        // reuse previous results if:
        // a) length greater than one character (others searches are special cases) AND
        // b) the the new results will be a subset of the previous results
        if (aPreviousResult && aPreviousResult.searchString.trim().length > 1 &&
            searchString.indexOf(aPreviousResult.searchString.trim().toLowerCase()) >= 0) {
            this.log("Using previous autocomplete result");
            result = aPreviousResult;
            result.wrappedJSObject.searchString = aUntrimmedSearchString;

            let searchTokens = searchString.split(/\s+/);
            // We have a list of results for a shorter search string, so just
            // filter them further based on the new search string and add to a new array.
            let entries = result.wrappedJSObject.entries;
            let filteredEntries = [];
            for (let i = 0; i < entries.length; i++) {
                let entry = entries[i];
                // Remove results that do not contain the token
                // XXX bug 394604 -- .toLowerCase can be wrong for some intl chars
                if(searchTokens.some(function (tok) entry.textLowerCase.indexOf(tok) < 0))
                    continue;
                this._calculateScore(entry, searchString, searchTokens);
                this.log("Reusing autocomplete entry '" + entry.text +
                         "' (" + entry.frecency +" / " + entry.totalScore + ")");
                filteredEntries.push(entry);
            }
            filteredEntries.sort(sortBytotalScore);
            result.wrappedJSObject.entries = filteredEntries;
        } else {
            this.log("Creating new autocomplete search result.");
            let entries = this.getAutoCompleteValues(aInputName, searchString);
            result = new FormAutoCompleteResult(this._formHistory, entries, aInputName, aUntrimmedSearchString);
            if (aField && aField.maxLength > -1) {
                let original = result.wrappedJSObject.entries;
                let filtered = original.filter(function (el) el.text.length <= this.maxLength, aField);
                result.wrappedJSObject.entries = filtered;
            }
        }

        return result;
    },

    getAutoCompleteValues : function (fieldName, searchString) {
        let values = [];
        let searchTokens;

        let params = {
            agedWeight:         this._agedWeight,
            bucketSize:         this._bucketSize,
            expiryDate:         1000 * (Date.now() - this._expireDays * 24 * 60 * 60 * 1000),
            fieldname:          fieldName,
            maxTimeGroupings:   this._maxTimeGroupings,
            now:                Date.now() * 1000,          // convert from ms to microseconds
            timeGroupingSize:   this._timeGroupingSize
        }

        // only do substring matching when more than one character is typed
        let where = ""
        let boundaryCalc = "";
        if (searchString.length > 1) {
            searchTokens = searchString.split(/\s+/);

            // build up the word boundary and prefix match bonus calculation
            boundaryCalc = "MAX(1, :prefixWeight * (value LIKE :valuePrefix ESCAPE '/') + (";
            // for each word, calculate word boundary weights for the SELECT clause and
            // add word to the WHERE clause of the query
            let tokenCalc = [];
            for (let i = 0; i < searchTokens.length; i++) {
                tokenCalc.push("(value LIKE :tokenBegin" + i + " ESCAPE '/') + " +
                                "(value LIKE :tokenBoundary" + i + " ESCAPE '/')");
                where += "AND (value LIKE :tokenContains" + i + " ESCAPE '/') ";
            }
            // add more weight if we have a traditional prefix match and
            // multiply boundary bonuses by boundary weight
            boundaryCalc += tokenCalc.join(" + ") + ") * :boundaryWeight)";
            params.prefixWeight = this._prefixWeight;
            params.boundaryWeight = this._boundaryWeight;
        } else if (searchString.length == 1) {
            where = "AND (value LIKE :valuePrefix ESCAPE '/') ";
            boundaryCalc = "1";
        } else {
            where = "";
            boundaryCalc = "1";
        }
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
                    ", 3) AS frecency, " +
                    boundaryCalc + " AS boundaryBonuses " +
                    "FROM moz_formhistory " +
                    "WHERE fieldname=:fieldname " + where +
                    "ORDER BY ROUND(frecency * boundaryBonuses) DESC, UPPER(value) ASC";

        let stmt;
        try {
            stmt = this._dbCreateStatement(query, params);

            // Chicken and egg problem: Need the statement to escape the params we
            // pass to the function that gives us the statement. So, fix it up now.
            if (searchString.length >= 1)
                stmt.params.valuePrefix = stmt.escapeStringForLIKE(searchString, "/") + "%";
            if (searchString.length > 1) {
                for (let i = 0; i < searchTokens.length; i++) {
                    let escapedToken = stmt.escapeStringForLIKE(searchTokens[i], "/");
                    stmt.params["tokenBegin" + i] = escapedToken + "%";
                    stmt.params["tokenBoundary" + i] =  "% " + escapedToken + "%";
                    stmt.params["tokenContains" + i] = "%" + escapedToken + "%";
                }
            } else {
                // no addional params need to be substituted into the query when the
                // length is zero or one
            }

            while (stmt.executeStep()) {
                let entry = {
                    text:           stmt.row.value,
                    textLowerCase:  stmt.row.value.toLowerCase(),
                    frecency:       stmt.row.frecency,
                    totalScore:     Math.round(stmt.row.frecency * stmt.row.boundaryBonuses)
                }
                values.push(entry);
            }

        } catch (e) {
            this.log("getValues failed: " + e.name + " : " + e.message);
            throw "DB failed getting form autocomplete values";
        } finally {
            if (stmt) {
                stmt.reset();
            }
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

    /*
     * _calculateScore
     *
     * entry    -- an nsIAutoCompleteResult entry
     * aSearchString -- current value of the input (lowercase)
     * searchTokens -- array of tokens of the search string
     *
     * Returns: an int
     */
    _calculateScore : function (entry, aSearchString, searchTokens) {
        let boundaryCalc = 0;
        // for each word, calculate word boundary weights
        for each (let token in searchTokens) {
            boundaryCalc += (entry.textLowerCase.indexOf(token) == 0);
            boundaryCalc += (entry.textLowerCase.indexOf(" " + token) >= 0);
        }
        boundaryCalc = boundaryCalc * this._boundaryWeight;
        // now add more weight if we have a traditional prefix match and
        // multiply boundary bonuses by boundary weight
        boundaryCalc += this._prefixWeight *
                        (entry.textLowerCase.
                         indexOf(aSearchString) == 0);
        entry.totalScore = Math.round(entry.frecency * Math.max(1, boundaryCalc));
    }

}; // end of FormAutoComplete implementation




// nsIAutoCompleteResult implementation
function FormAutoCompleteResult (formHistory, entries, fieldName, searchString) {
    this.formHistory = formHistory;
    this.entries = entries;
    this.fieldName = fieldName;
    this.searchString = searchString;
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
    errorDescription : "",
    get defaultIndex() {
        if (entries.length == 0)
            return -1;
        else
            return 0;
    },
    get searchResult() {
        if (this.entries.length == 0)
            return Ci.nsIAutoCompleteResult.RESULT_NOMATCH;
        return Ci.nsIAutoCompleteResult.RESULT_SUCCESS;
    },
    get matchCount() {
        return this.entries.length;
    },

    getValueAt : function (index) {
        this._checkIndexBounds(index);
        return this.entries[index].text;
    },

    getLabelAt: function(index) {
        return getValueAt(index);
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

        if (removeFromDB)
            this.formHistory.removeEntry(this.fieldName, removedEntry.text);
    }
};

let component = [FormAutoComplete];
var NSGetFactory = XPCOMUtils.generateNSGetFactory(component);
