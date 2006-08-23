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
 * The Original Code is new-graph code.
 *
 * The Initial Developer of the Original Code is
 *    Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (Original Author)
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

//const getdatacgi = "getdata-fake.cgi?";
//const getdatacgi = "http://localhost:9050/getdata.cgi?";
const getdatacgi = "getdata.cgi?";


function checkErrorReturn(obj) {
    if (!obj || obj.resultcode != 0) {
        alert ("Error: " + (obj ? (obj.error + "(" + obj.resultcode + ")") : "(nil)"));
        return false;
    }
    return true;
}

function TinderboxData() {
    this.onTestListAvailable = new YAHOO.util.CustomEvent("testlistavailable");
    this.onDataSetAvailable = new YAHOO.util.CustomEvent("datasetavailable");

    this.testList = null;
    this.testData = {};
}

TinderboxData.prototype = {
    testList: null,
    testData: null,

    onTestListAvailable: null,
    onDataSetAvailable: null,

    defaultLoadRange: null,

    init: function () {
        var self = this;
        //netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect")

        loadJSONDoc(getdatacgi)
        .addCallbacks(
            function (obj) {
                if (!checkErrorReturn(obj)) return;
                self.testList = obj.results;
                self.onTestListAvailable.fire(self.testList);
            },
            function () {alert ("Error talking to getdata.cgi"); });
    },

    requestTestList: function (callback) {
        var self = this;

        if (this.testList != null) {
            callback.call (window, this.testList);
        } else {
            var cb = 
            function (type, args, obj) {
                self.onTestListAvailable.unsubscribe(cb, obj);
                obj.call (window, args[0]);
            };

            this.onTestListAvailable.subscribe (cb, callback);
        }
    },

    // arg1 = startTime, arg2 = endTime, arg3 = callback
    // arg1 = callback, arg2/arg3 == null
    requestDataSetFor: function (testId, arg1, arg2, arg3) {
        var self = this;

        var startTime = arg1;
        var endTime = arg2;
        var callback = arg3;

        if (arg1 && arg2 == null && arg3 == null) {
            callback = arg1;
            if (this.defaultLoadRange) {
                startTime = this.defaultLoadRange[0];
                endTime = this.defaultLoadRange[1];
                //log ("load range using default", startTime, endTime);
            } else {
                startTime = null;
                endTime = null;
            }
        }

        if (testId in this.testData) {
            var ds = this.testData[testId];
            //log ("Can maybe use cached?");
            if ((ds.requestedFirstTime == null && ds.requestedLastTime == null) ||
                (ds.requestedFirstTime <= startTime &&
                 ds.requestedLastTime >= endTime))
            {
                //log ("Using cached ds");
                callback.call (window, testId, ds);
                return;
            }

            // this can be optimized, if we request just the bookend bits,
            // but that's overkill
            if (ds.firstTime < startTime)
                startTime = ds.firstTime;
            if (ds.lastTime > endTime)
                endTime = ds.lastTime;
        }

        var cb = 
        function (type, args, obj) {
            if (args[0] != testId ||
                args[2] > startTime ||
                args[3] < endTime)
            {
                // not useful for us; there's another
                // outstanding request for our time range, so wait for that
                return;
            }

            self.onDataSetAvailable.unsubscribe(cb, obj);
            obj.call (window, args[0], args[1]);
        };
        this.onDataSetAvailable.subscribe (cb, callback);

        //netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect")

        var reqstr = getdatacgi + "setid=" + testId;
        if (startTime)
            reqstr += "&starttime=" + startTime;
        if (endTime)
            reqstr += "&endtime=" + endTime;
        log (reqstr);
        loadJSONDoc(reqstr)
        .addCallbacks(
            function (obj) {
                if (!checkErrorReturn(obj)) return;

                var ds = new TimeValueDataSet(obj.results);
                ds.requestedFirstTime = startTime;
                ds.requestedLastTime = endTime;
                self.testData[testId] = ds;
                if (obj.annotations)
                    ds.annotations = new TimeStringDataSet(obj.annotations);

                self.onDataSetAvailable.fire(testId, ds, startTime, endTime);
            },
            function (obj) {alert ("Error talking to getdata.cgi (" + obj + ")"); log (obj.stack); });
    },

    clearValueDataSets: function () {
        //log ("clearvalueDatasets");
        this.tinderboxTestData = {};
    }
};

