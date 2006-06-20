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
 * The Original Code is new graph server code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

// all times are in seconds

const ONE_HOUR_SECONDS = 60*60;
const ONE_DAY_SECONDS = 24*ONE_HOUR_SECONDS;
const ONE_WEEK_SECONDS = 7*ONE_DAY_SECONDS;
const ONE_YEAR_SECONDS = 365*ONE_DAY_SECONDS; // leap years whatever.

const MONTH_ABBREV = [ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" ];

//const getdatacgi = "getdata-fake.cgi?";
//const getdatacgi = "http://localhost:9050/getdata.cgi?";
const getdatacgi = "getdata.cgi?";

const bonsaicgi = "bonsaibouncer.cgi";

// more days than this and we'll force user confirmation for the bonsai query
const bonsaiNoForceDays = 90;

// the default average interval
var gAverageInterval = 6*ONE_HOUR_SECONDS;
var gCurrentLoadRange = null;
var gForceBonsai = false;

function checkErrorReturn(obj) {
    if (!obj || obj.resultcode != 0) {
        alert ("Error: " + (obj ? (obj.error + "(" + obj.resultcode + ")") : "(nil)"));
        return false;
    }
    return true;
}

function TinderboxData() {
    this.onTinderboxListAvailable = new YAHOO.util.CustomEvent("tinderboxlistavailable");
    this.onTinderboxTestListAvailable = new YAHOO.util.CustomEvent("tinderboxtestlistavailable");
    this.onDataSetAvailable = new YAHOO.util.CustomEvent("datasetavailable");

    this.tinderboxTests = {};
    this.tinderboxTestData = {};
}

TinderboxData.prototype = {
    tinderboxes: null,
    tinderboxTests: null,
    tinderboxTestData: null,

    onTinderboxListAvailable: null,
    onTinderboxTestListAvailable: null,
    onDataSetAvailable: null,

    defaultLoadRange: null,

    init: function () {
        var self = this;
        //netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect")

        loadJSONDoc(getdatacgi)
        .addCallbacks(
            function (obj) {
                if (!checkErrorReturn(obj)) return;
                self.tinderboxes = obj.results;
                self.onTinderboxListAvailable.fire(self.tinderboxes);
            },
            function () {alert ("Error talking to getdata.cgi"); });
    },

    requestTinderboxList: function (callback) {
        var self = this;

        if (this.tinderboxes != null) {
            callback.call (window, this.tinderboxes);
        } else {
            var cb = 
            function (type, args, obj) {
                self.onTinderboxListAvailable.unsubscribe(cb, obj);
                obj.call (window, args[0]);
            };

            this.onTinderboxListAvailable.subscribe (cb, callback);
        }
    },

    requestTestListFor: function (tbox, callback) {
        var self = this;

        if ((tbox in this.tinderboxTests) &&
            (this.tinderboxTests[tbox] != -1))
        {
            callback.call (window, tbox, this.tinderboxTests[tbox]);
        } else {
            // we do this this way so that we only need one
            // outstanding request per tinderbox, and we have a way to
            // wait for that data to be available
            var cb = 
            function (type, args, obj) {
                if (args[0] != tbox)
                    return;
                self.onTinderboxTestListAvailable.unsubscribe(cb, obj);
                obj.call (window, args[0], args[1]);
            };
            this.onTinderboxTestListAvailable.subscribe (cb, callback);

            if (!(tbox in this.tinderboxTests)) {
                this.tinderboxTests[tbox] = -1;

                //netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect")

                loadJSONDoc(getdatacgi + "tbox=" + tbox)
                    .addCallbacks(
                        function (obj) {
                            if (!checkErrorReturn(obj)) return;

                            self.tinderboxTests[tbox] = obj.results;
                            self.onTinderboxTestListAvailable.fire(tbox, self.tinderboxTests[tbox]);
                        },
                        function () {alert ("Error talking to getdata.cgi"); });
            }
        }
    },

    // arg1 = startTime, arg2 = endTime, arg3 = callback
    // arg1 = callback, arg2/arg3 == null
    requestValueDataSetFor: function (tbox, testname, arg1, arg2, arg3) {
        var self = this;

        var startTime = arg1;
        var endTime = arg2;
        var callback = arg3;

        if (arg1 && arg2 == null && arg3 == null) {
            callback = arg1;
            if (this.defaultLoadRange) {
                startTime = this.defaultLoadRange[0];
                endTime = this.defaultLoadRange[1];
                log ("load range using default", startTime, endTime);
            } else {
                startTime = null;
                endTime = null;
            }
        }

        if ((tbox in this.tinderboxTestData) &&
            (testname in this.tinderboxTestData[tbox]))
        {
            var ds = this.tinderboxTestData[tbox][testname];
            log ("Can maybe use cached?");
            if ((ds.requestedFirstTime == null && ds.requestedLastTime == null) ||
                (ds.requestedFirstTime <= startTime &&
                 ds.requestedLastTime >= endTime))
            {
                log ("Using cached ds");
                callback.call (window, tbox, testname, ds);
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
            if (args[0] != tbox ||
                args[1] != testname ||
                args[3] > startTime ||
                args[4] < endTime)
            {
                // not useful for us; there's another
                // outstanding request for our time range, so wait for that
                return;
            }

            self.onDataSetAvailable.unsubscribe(cb, obj);
            obj.call (window, args[0], args[1], args[2]);
        };
        this.onDataSetAvailable.subscribe (cb, callback);

        //netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect")

        var reqstr = getdatacgi + "tbox=" + tbox + "&test=" + testname;
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
                if (!(tbox in self.tinderboxTestData))
                    self.tinderboxTestData[tbox] = {};
                self.tinderboxTestData[tbox][testname] = ds;

                self.onDataSetAvailable.fire(tbox, testname, ds, startTime, endTime);
            },
            function (obj) {alert ("Error talking to getdata.cgi (" + obj + ")"); log (obj.stack); });
    },

    clearValueDataSets: function () {
        log ("clearvalueDatasets");
        this.tinderboxTestData = {};
    }
};

function TimeDataSet(data) {
    this.data = data;
}

TimeDataSet.prototype = {
    data: null,

    indicesForTimeRange: function (startTime, endTime) {
        var startIndex = -1;
        var endIndex = -1;

        if (this.data[0] > endTime ||
            this.data[this.data.length-2] < startTime)
            return null;

        for (var i = 0; i < this.data.length/2; i++) {
            if (startIndex == -1 && this.data[i*2] >= startTime) {
                startIndex = i;
            } else if (startIndex != -1 && this.data[i*2] > endTime) {
                endIndex = i;
                return [startIndex, endIndex];
            }
        }

        endIndex = (this.data.length/2) - 1;
        return [startIndex, endIndex];
    },
};

function TimeValueDataSet(data, color) {
    this.data = data;

    this.firstTime = data[0];
    if (data.length > 2)
        this.lastTime = data[data.length-2];
    else
        this.lastTime = data[0];

    if (color) {
        this.color = color;
    } else {
        this.color = randomColor();
    }

    log ("new tds:", this.firstTime, this.lastTime);

    this.relativeToSets = new Array();
}

TimeValueDataSet.prototype = {
    __proto__: new TimeDataSet(),

    firstTime: 0,
    lastTime: 0,
    data: null,    // array: [time0, value0, time1, value1, ...]
    relativeTo: null,

    color: "black",

    minMaxValueForTimeRange: function (startTime, endTime) {
        var minValue = Number.POSITIVE_INFINITY;
        var maxValue = Number.NEGATIVE_INFINITY;
        for (var i = 0; i < this.data.length/2; i++) {
            var t = this.data[i*2];
            if (t >= startTime && t <= endTime) {
                var v = this.data[i*2+1];
                if (v < minValue)
                    minValue = v;
                if (v > maxValue)
                    maxValue = v;
            }
        }

        return [minValue, maxValue];
    },

    // create a new ds that's the average of this ds's values,
    // with the average sampled over the given interval,
    // at every avginterval/2
    createAverage: function (avginterval) {
        if (avginterval <= 0)
            throw "avginterval <= 0";

        if (this.averageDataSet != null &&
            this.averageInterval == avginterval)
        {
            return this.averageDataSet;
        }

        var newdata = [];

        var time0 = this.data[0];
        var val0 = 0;
        var count0 = 0;

        var time1 = time0 + avginterval/2;
        var val1 = 0;
        var count1 = 0;

        var ns = this.data.length/2;
        for (var i = 0; i < ns; i++) {
            var t = this.data[i*2];
            var v = this.data[i*2+1];
            if (t > time0+avginterval) {
                newdata.push(time0 + avginterval/2);
                newdata.push(count0 ? (val0 / count0) : 0);

                // catch up
                while (time1 < t) {
                    time0 += avginterval/2;
                    time1 = time0;
                }

                time0 = time1;
                val0 = val1;
                count0 = count1;

                time1 = time0 + avginterval/2;
                val1 = 0;
                count1 = 0;
            }

            val0 += v;
            count0++;

            if (t > time1) {
                val1 += v;
                count1++;
            }
        }

        if (count0 > 0) {
            newdata.push(time0 + avginterval/2);
            newdata.push(val0 / count0);
        }

        var newds = new TimeValueDataSet(newdata, lighterColor(this.color));
        newds.averageOf = this;

        this.averageDataSet = newds;
        this.averageInterval = avginterval;

        return newds;
    },

    // create a new dataset with this ds's data,
    // relative to otherds
    createRelativeTo: function (otherds, absval) {
        if (otherds == this) {
            log("error, same ds");
            return null;
        }

        for each (var s in this.relativeToSets) {
            if (s.relativeTo == otherds)
                return s;
        }

        var firstTime = this.firstTime;
        var lastTime = this.lastTime;

        if (otherds.firstTime > firstTime)
            firstTime = otherds.firstTime;
        if (otherds.lastTime < lastTime)
            lastTime = otherds.lastTime;

        var newdata = [];

        var thisidx = this.indicesForTimeRange (firstTime, lastTime);
        var otheridx = this.indicesForTimeRange (firstTime, lastTime);

        var o = otheridx[0];
        var ov, ov1, ov2, ot1, ot2;
        for (var i = thisidx[0]; i < thisidx[1]; i++) {
            var t = this.data[i*2];
            var tv = this.data[i*2+1];
            while (otherds.data[o*2] < t)
                o++;

            ot1 = otherds.data[o*2];
            ov1 = otherds.data[o*2+1];
            if (o < otheridx[1]) {
                ot2 = otherds.data[o*2+2];
                ov2 = otherds.data[o*2+3];
            } else {
                ot2 = ot1;
                ov2 = ov1;
            }


            var d = (t-ot1)/(ot2-ot1);
            ov = (1-d) * ov1 + d * ov2;

            newdata.push(t);
            //log ("i", i, "tv", tv, "ov", ov, "t", t, "ot1", ot1, "ot2", ot2, "ov1", ov1, "ov2", ov2);
            //log ("i", i, "tv", tv, "ov", ov, "tv/ov", tv/ov, "ov/tv", ov/tv);
            if (absval) {
                newdata.push(tv-ov);
            } else {
                if (tv > ov)
                    newdata.push((tv/ov) - 1);
                else
                    newdata.push(-((ov/tv) - 1));
            }
        }

        var newds = new TimeValueDataSet(newdata, this.color);
        newds.relativeTo = otherds;

        this.relativeToSets.push(newds);

        return newds;
    },
};

function TimeStringDataSet(data) {
    this.data = data;
}

TimeStringDataSet.prototype = {
    __proto__: new TimeDataSet(),
    data: null,

    onDataSetChanged: null,

    init: function () {
    },

    addString: function (time, string) {
    },

    removeStringAt: function (index) {
    },
};

function BonsaiService() {
}

BonsaiService.prototype = {
    // this could cache stuff, so that we only have to request the bookend data
    // if we want a wider range, but it's probably not worth it for now.
    //
    // The callback is called with an object argument which contains:
    // {
    //   times: [ t1, t2, t3, .. ],
    //   who:   [ w1, w2, w3, .. ],
    //   log:   [ l1, l2, l3, .. ],
    //   files: [ [ r11, f11, r12, f12, r13, f13, .. ], [ r21, f21, r22, f22, r23, f23, .. ], .. ]
    // }
    //
    // r = revision number, as a string, e.g. "1.15"
    // f = file, e.g. "mozilla/widget/foo.cpp"
    //
    // arg1 = callback, arg2 = null
    // arg1 = includeFiles, arg2 = callback
    requestCheckinsBetween: function (startDate, endDate, arg1, arg2) {
        var includeFiles = arg1;
        var callback = arg2;

        if (arg2 == null) {
            callback = arg1;
            includeFiles = null;
        }

        var queryargs = {
            treeid: "default",
            module: "SeaMonkeyAll",
            branch: "HEAD",
            mindate: startDate,
            maxdate: endDate
        };

        if (!includeFiles)
            queryargs.xml_nofiles = "1";

        log ("bonsai request: ", queryString(queryargs));

        doSimpleXMLHttpRequest (bonsaicgi, queryargs)
            .addCallbacks(
                function (obj) {
                    var result = { times: [], who: [], comment: [], files: null };
                    if (includeFiles)
                        result.files = [];

                    // strip out the xml declaration
                    var s = obj.responseText.replace(/<\?xml version="1.0"\?>/, "");
                    var bq = new XML(s);
                    for (var i = 0; i < bq.ci.length(); i++) {
                        var ci = bq.ci[i];
                        result.times.push(ci.@date);
                        result.who.push(ci.@who);
                        result.comment.push(ci.log.text().toString());
                        if (includeFiles) {
                            var files = [];
                            for (var j = 0; j < ci.files.f.length(); j++) {
                                var f = ci.files.f[j];
                                files.push(f.@rev);
                                files.push(f.text().toString());
                            }
                            result.files.push(files);
                        }
                    }

                    callback.call (window, result);
                },
                function () { alert ("Error talking to bonsai"); });
    },
};

function Graph() {
}

Graph.prototype = {
    startTime: null,
    endTime: null,

    borderTop: 1,
    borderLeft: 1,

    yScale: 1,
    yOffset: 0,

    backBuffer: null,
    frontBuffer: null,
    yAxisDiv: null,
    xAxisDiv: null,

    dataSets: null,
    dataSetIndices: null,
    dataSetMinMaxes: null,

    dataSetMinMinVal: 0,
    dataSetMaxMaxVal: 0,

    xLabelContainer: null,
    xLabelWidth: 100,
    xLabelHeight: 50,

    yLabelContainer: null,
    yLabelWidth: 50,
    yLabelHeight: 50,

    selectionType: "none",
    selectionColor: "rgba(0,0,255,0.5)",
    selectionCursorTime: null,
    selectionStartTime: null,
    selectionEndTime: null,

    onSelectionChanged: null,
    onCursorMoved: null,

    cursorType: "none",
    cursorColor: "rgba(200,200,0,0.7)",
    cursorTime: null,
    cursorValue: null,

    markerColor: "rgba(200,0,0,0.4)",
    markersVisible: true,
    markers: null,

    dirty: true,
    valid: false,

    init: function (canvasElement) {
        this.frontBuffer = getElement(canvasElement);
        this.xLabelContainer = getElement(canvasElement + "-labels-x");
        this.yLabelContainer = getElement(canvasElement + "-labels-y");

        this.backBuffer = new CANVAS();
        this.backBuffer.width = this.frontBuffer.width;
        this.backBuffer.height = this.frontBuffer.height;

        this.overlayBuffer = new CANVAS();
        this.overlayBuffer.width = this.frontBuffer.width;
        this.overlayBuffer.height = this.frontBuffer.height;

        this.dataSets = new Array();
        this.dataSetMinMaxes = new Array();
        this.dataSetIndices = new Array();

        this.markers = new Array();

        this.onSelectionChanged = new YAHOO.util.CustomEvent("graphselectionchanged");
        this.onCursorMoved = new YAHOO.util.CustomEvent("graphcursormoved");
    },

    getQueryString: function (prefix) {
        var qs = "";

        qs += prefix + "st=" + this.selectionType;
        if (this.selectionType == "range") {
            if (this.selectionStartTime != null && this.selectionEndTime != null)
                qs += "&" + prefix + "ss=" + this.selectionStartTime + "&" + prefix + "se=" + this.selectionEndTime;
        } else if (this.selectionType == "cursor") {
            if (this.selectionCursorTime != null)
                qs += "&" + prefix + "sc=" + this.selectionCursorTime;
        }

        qs += "&" + prefix + "start=" + this.startTime + "&" + prefix + "end=" + this.endTime;

        return qs;
    },

    handleQueryStringData: function (prefix, qsdata) {
        // XX should do some more verification that
        // qsdata has the members we care about
        this.startTime = new Number(qsdata[prefix + "start"]);
        this.endTime = new Number(qsdata[prefix + "end"]);

        var st = qsdata[prefix + "st"];
        if (st == "range") {
            this.selectionType = "range";
            if (((prefix+"ss") in qsdata) && ((prefix+"se") in qsdata)) {
                this.selectionStartTime = new Number(qsdata[prefix + "ss"]);
                this.selectionEndTime = new Number(qsdata[prefix + "se"]);
            } else {
                this.selectionStartTime = null;
                this.selectionEndTime = null;
            }                
        } else if (st == "cursor") {
            this.selectionType = "Cursor";
            if ((prefix+"sc") in qsdata)
                this.selectionCursorTime = new Number(qsdata[prefix + "sc"]);
            else
                this.selectionCursorTime = null;
        }

        this.dirty = true;
    },

    addDataSet: function (ds, color) {
        if (this.dataSets.some(function(d) { return (d==ds); }))
            return;

        if (color == null) {
            if (ds.color != null) {
                color = ds.color;
            } else {
                color = randomColor();
            }
        }

        this.dataSets.push(ds);
        this.dataSetIndices.push(null);
        this.dataSetMinMaxes.push(null);

        this.dirty = true;
    },

    removeDataSet: function (ds) {
        for (var i = 0; i < this.dataSets.length; i++) {
            if (this.dataSets[i] == ds) {
                this.dataSets = Array.splice(this.dataSets, i, 1);
                this.dataSetIndices = Array.splice(this.dataSetIndices, i, 1);
                this.dataSetMinMaxes = Array.splice(this.dataSetMinMaxes, i, 1);
                return;
            }
        }
    },

    clearDataSets: function () {
        this.dataSets = new Array();
        this.dataSetMinMaxes = new Array();
        this.dataSetIndices = new Array();

        this.dirty = true;
    },

    setTimeRange: function (start, end) {
        this.startTime = start;
        this.endTime = end;

        this.dirty = true;
    },

    expandTimeRange: function (start, end) {
        if (this.startTime == null || start < this.startTime)
            this.startTime = start;
        if (this.endTime == null || end > this.endTime)
            this.endTime = end;

        this.dirty = true;
    },

    setSelectionType: function (stype) {
        if (this.selectionType == stype)
            return;

        // clear out old listeners
        if (this.selectionType == "range") {
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousedown", this.selectionMouseDown);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousemove", this.selectionMouseMove);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mouseup", this.selectionMouseUp);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mouseout", this.selectionMouseOut);
        } else if (this.selectionType == "cursor") {
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousedown", this.selectionMouseDown);
        }

        this.selectionStartTime = null;
        this.selectionEndTime = null;
        this.selectionCursorTime = null;

        if (stype == "range") {
            YAHOO.util.Event.addListener (this.frontBuffer, "mousedown", this.selectionMouseDown, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mousemove", this.selectionMouseMove, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mouseup", this.selectionMouseUp, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mouseout", this.selectionMouseOut, this, true);

            this.selectionType = "range";
        } else if (stype == "cursor") {
            YAHOO.util.Event.addListener (this.frontBuffer, "mousedown", this.selectionMouseDown, this, true);
            this.selectionType = "cursor";
        }

        this.redrawOverlayOnly();
    },

    setSelectionColor: function (scolor) {
        this.selectionColor = scolor;
        this.redrawOverlayOnly();
    },

    setCursorType: function (type) {
        if (this.cursorType == type)
            return;

        if (this.cursorType == "free") {
            YAHOO.util.Event.removeListener (this.frontBuffer, "mousemove", this.cursorMouseMove);
            YAHOO.util.Event.removeListener (this.frontBuffer, "mouseout", this.cursorMouseOut);
        }

        if (type == "free") {
            YAHOO.util.Event.addListener (this.frontBuffer, "mousemove", this.cursorMouseMove, this, true);
            YAHOO.util.Event.addListener (this.frontBuffer, "mouseout", this.cursorMouseOut, this, true);
            this.cursorType = "free";
        } else {
            this.cursorType = "none";
        }
    },

    recompute: function () {
        this.dataSetIndices = [];
        this.dataSetMinMaxes = [];

        this.hasRelative = false;
        var nonRelative = 0;

        for (var i = 0; i < this.dataSets.length; i++) {
            this.dataSetIndices.push (this.dataSets[i].indicesForTimeRange (this.startTime, this.endTime));
            this.dataSetMinMaxes.push (this.dataSets[i].minMaxValueForTimeRange (this.startTime, this.endTime));

            if (this.dataSets[i].relativeTo != null)
                this.hasRelative = true;
            else
                nonRelative++;
        }

        if (this.hasRelative && nonRelative > 1) {
            log("More than one non-relative dataset added to graph");
            throw "More than one non-relative dataset added to graph";
        }

        this.dataSetMinMinVal = Number.MAX_VALUE;
        this.dataSetMaxMaxVal = Number.MIN_VALUE;

        for each (var dsvals in this.dataSetMinMaxes) {
            if (this.dataSetMinMinVal > dsvals[0])
                this.dataSetMinMinVal = dsvals[0];
            if (this.dataSetMaxMaxVal < dsvals[1])
                this.dataSetMaxMaxVal = dsvals[1];
        }

        log ("minmin:", this.dataSetMinMinVal, "maxmax:", this.dataSetMaxMaxVal);

        this.getTimeAxisLabels();
        this.getValueAxisLabels();

        this.dirty = false;
    },

    autoScale: function () {
        if (this.dirty)
            this.recompute();

        var vmin, vmax;

        if (this.hasRelative) {
            vmin = Math.floor(this.dataSetMinMinVal);
            vmax = Math.ceil(this.dataSetMaxMaxVal);

            if ((vmax - vmin) == 1)
                vmin--;

            log ("vmin", vmin, "vmax", vmax);
            this.yOffset = vmin;
            this.yScale = this.frontBuffer.height / (vmax - vmin);
            this.dirty = true;
            return;
        }

        var delta = this.dataSetMaxMaxVal - this.dataSetMinMinVal;
        var scaled = false;
        for each (var sfactor in [1000, 500, 250, 100, 25, 10, 1]) {
            if (delta > sfactor) {
                vmin = this.dataSetMinMinVal - (this.dataSetMinMinVal % sfactor);
                vmax = (this.dataSetMaxMaxVal - (this.dataSetMaxMaxVal % sfactor)) + sfactor;

                this.yOffset = vmin;
                this.yScale = this.frontBuffer.height / (vmax - vmin);
                scaled = true;
                break;
            }
        }

        if (!scaled) {
            this.yOffset = this.dataSetMinMinVal;
            this.yScale = this.frontBuffer.height / (this.dataSetMaxMaxVal - this.dataSetMinMinVal);
        }

        // we have to dirty again, due to the labels
        this.dirty = true;
    },

    redraw: function () {
        if (this.dirty)
            this.recompute();

        var ctx = this.backBuffer.getContext("2d");
        var cw = this.backBuffer.width;
        var ch = this.backBuffer.height;

        var xoffs = this.startTime;
        var yoffs = this.yOffset;

        var xscale = cw / (this.endTime - this.startTime);

        var hasAverageDSs = false;
        for each (var ds in this.dataSets) {
            if ("averageOf" in ds) {
                hasAverageDSs = true;
                break;
            }
        }

        for (var i = 0; i < this.dataSets.length; i++) {
            // yScale = pixels-per-value
            with (ctx) {
                clearRect (0, 0, cw, ch);
                lineWidth = 1.0;

                // draw gridlines
                var yLabelValues = this.getTimeAxisLabels();
                strokeStyle = "#999999";
                for each (var label in yLabelValues) {
                    // label[1] is the actual value of that label line; we need
                    // to scale it into place, but we can't just use scale()
                    // since we want to make sure it's a single-pixel line
                    var p = Math.round((label[1] - xoffs) * xscale) + 0.5;
                    beginPath();
                    moveTo(p, -0.5);
                    lineTo(p, this.frontBuffer.height + 0.5);
                    stroke();
                }

                var xLabelValues = this.getValueAxisLabels();
                for each (var label in xLabelValues) {
                    var p = Math.round((label[1] - yoffs) * this.yScale) + 0.5;
                    beginPath();
                    moveTo(-0.5, p);
                    lineTo(this.frontBuffer.width + 0.5, p);
                    stroke();
                }

                // draw markers
                strokeStyle = this.markerColor;
                for (var i = 0; i < this.markers.length/2; i++) {
                    var mtime = this.markers[i*2];
                    //var mlabel = this.markers[i*2+1];

                    if (mtime < this.startTime || mtime > this.endTime)
                        continue;

                    var p = Math.round((mtime - xoffs) * xscale) + 0.5;
                    beginPath();
                    moveTo(p, Math.round(this.frontBuffer.height*0.8)-0.5);
                    lineTo(p, this.frontBuffer.height+0.5);
                    stroke();
                }

                // draw actual graph lines

                for (var i = 0; i < this.dataSets.length; i++) {
                    if (this.dataSetIndices[i] == null) {
                        // there isn't anything in the data set in the given time range
                        continue;
                    }

                    //log ("ds start end", this.startTime, this.endTime, "timediff:", (this.endTime - this.startTime));
                    save();
                    scale(xscale, -this.yScale);
                    translate(0, -ch/this.yScale);

                    beginPath();

                    var first = true;
                    var startIdx = this.dataSetIndices[i][0];
                    var endIdx = this.dataSetIndices[i][1];

                    // start one before and go one after if we can,
                    // so that the plot doesn't have a hole at the start
                    // and end
                    if (startIdx > 0) startIdx--;
                    if (endIdx < ((this.dataSets[i].data.length)/2)) endIdx++;

                    for (var j = startIdx; j < endIdx; j++)
                    {
                        var t = this.dataSets[i].data[j*2];
                        var v = this.dataSets[i].data[j*2+1];
                        if (first) {
                            moveTo (t-xoffs, v-yoffs);
                            first = false;
                        } else {
                            lineTo (t-xoffs, v-yoffs);
                        }
                    }

                    /* bleh. */

                    restore();

                    if (hasAverageDSs && !("averageOf" in this.dataSets[i])) {
                        lineWidth = 0.5;
                    } else {
                        lineWidth = 1.0;
                    }

                    strokeStyle = colorToRgbString(this.dataSets[i].color);
                    stroke();
                }
            }
        }

        this.redrawOverlayOnly();

        try {
            this.makeLabels();
        } catch (e) {
            log(e);
        }

        this.valid = true;
    },

    redrawOverlayOnly: function () {
        with (this.frontBuffer.getContext("2d")) {
            globalCompositeOperation = "copy";
            drawImage(this.backBuffer, 0, 0);
        }

        var doDrawOverlay = false;

        with (this.overlayBuffer.getContext("2d")) {
            clearRect(0, 0, this.overlayBuffer.width, this.overlayBuffer.height);
            if (this.selectionCursorTime || (this.selectionStartTime && this.selectionEndTime)) {
                var spixel, epixel;
                var pps = (this.frontBuffer.width / (this.endTime - this.startTime));

                if (this.selectionCursorTime) {
                    spixel = Math.round((this.selectionCursorTime-this.startTime) * pps);
                    epixel = spixel + 1;
                } else if (this.selectionStartTime && this.selectionEndTime) {
                    spixel = Math.round((this.selectionStartTime-this.startTime) * pps);
                    epixel = Math.round((this.selectionEndTime-this.startTime) * pps);
                }

                globalCompositeOperation = "over";
                fillStyle = this.selectionColor;
                fillRect(spixel, 0, epixel - spixel, this.frontBuffer.height);

                doDrawOverlay = true;
            }

            if ((this.cursorType != "none") && this.cursorTime != null && this.cursorValue != null) {
                globalCompositeOperation = "over";
                strokeStyle = this.cursorColor;

                var cw = this.frontBuffer.width;
                var ch = this.frontBuffer.height;

                var v;

                v = ch - Math.round((this.cursorValue - this.yOffset) * this.yScale);
                beginPath();
                moveTo(  -0.5, v+0.5);
                lineTo(cw+0.5, v+0.5);
                stroke();

                v = Math.round((this.cursorTime-this.startTime) * cw/(this.endTime - this.startTime));
                beginPath();
                moveTo(v+0.5,   -0.5);
                lineTo(v+0.5, ch+0.5);
                stroke();

                doDrawOverlay = true;
            }
        }

        if (doDrawOverlay) {
            with (this.frontBuffer.getContext("2d")) {
                globalCompositeOperation = "over";
                drawImage(this.overlayBuffer, 0, 0);
            }
        }

    },


    getValueAxisLabels: function () {
        if (!this.dirty)
            return this.yAxisLabels;

        // see getTimeAxisLabels for more commentary

        // y axis is either an arbitrary value or a percent
        var visibleValues = this.frontBuffer.height * this.yScale;
        var valuePerPixel = 1/this.yScale;
        var labelValue = this.yLabelHeight * valuePerPixel;

        // round to nearest integer, but that's it; we can try to get
        // fancy later on
        var fixedPrecision;
        if (this.hasRelative) {
/*
            labelValue = 1;

            var vdiff = Math.ceil(this.dataSetMaxMaxVal) - Math.floor(this.dataSetMinMinVal);
            if (vdiff <= 2) {
                labelValue = .25;
            } else if (vdiff <= 3) {
                labelValue = .5;
            } else {
                labelValue = 1;
            }
*/
        } else {
            if (visibleValues > 1000) {
                fixedPrecision = 0;
            } else if (visibleValues > 100) {
                fixedPrecision = 1;
            } else if (visibleValues > 10) {
                fixedPrecision = 2;
            } else if (visibleValues > 1) {
                fixedPrecision = 3;
            }
        }

        var numLabels = this.frontBuffer.height / this.yLabelHeight;
        var labels = [];
        var firstLabelOffsetValue = (labelValue - (this.yOffset % labelValue));

        var visibleYMax = this.yOffset + this.frontBuffer.height/this.yScale;

        //log("yoffset", this.yOffset, "ymax", visibleYMax, "labelValue", labelValue, "numLabels", numLabels, "flo", firstLabelOffsetValue);
        for (var i = 0; i < numLabels; i++) {
            // figure out the time value of this label
            var lvalue = this.yOffset + firstLabelOffsetValue + i*labelValue;
            if (lvalue > visibleYMax)
                break;

            // we want the text to correspond to the value drawn at the start of the block
            // also note that Y axis is inverted
            // XXX put back the -y/2 once we figure out how to vertically center a label's text
            var lpos = this.frontBuffer.height - ((lvalue - this.yOffset)/valuePerPixel /* - (this.yLabelHeight/2)*/);
            var l;
            //log ("lpos: ", lpos, "lvalue", lvalue, "ysc", this.yScale);
            if (this.hasRelative) {
                l = [lpos, lvalue, (lvalue * 100).toFixed(0).toString() + "%"];
            } else {
                l = [lpos, lvalue, lvalue.toFixed(fixedPrecision).toString()];
            }
            //log("lval", lvalue, "lpos", l[0]);
            labels.push(l);
        }

        this.yAxisLabels = labels;
        return labels;
    },

    getTimeAxisLabels: function () {
        if (!this.dirty)
            return this.xAxisLabels;

        // x axis is always time in seconds

        // duration is in seconds
        var duration = this.endTime - this.startTime;

        // we know the pixel size and we know the time, we can
        // compute the seconds per pixel
        var secondsPerPixel = duration / this.frontBuffer.width;

        // so what's the exact duration of one label of our desired size?
        var labelDuration = this.xLabelWidth * secondsPerPixel;

        // let's come up with a more round duration for our label.
        if (labelDuration <= 60) {
            labelDuration = 60;
        } else if (labelDuration <= 14*60) {
            labelDuration = Math.ceil(labelDuration / 60) * 60;
        } else if (labelDuration <= 15*60) {
            labelDuration = 15*60;
        } else if (labelDuration <= 59*60) {
            labelDuration = Math.ceil(labelDuration / (5*60)) * (5*60);
        } else if (labelDuration <= 23*ONE_HOUR_SECONDS) {
            labelDuration = Math.ceil(labelDuration / ONE_HOUR_SECONDS) * ONE_HOUR_SECONDS;
        } else if (labelDuration <= 6*ONE_DAY_SECONDS) {
            labelDuration = Math.ceil(labelDuration / ONE_DAY_SECONDS) * ONE_DAY_SECONDS;
        } else {
            // round to the nearest day at least
            labelDuration = labelDuration - (labelDuration%ONE_DAY_SECONDS);
        }

        // how many labels max can we fit?
        var numLabels = (this.frontBuffer.width / this.xLabelWidth);

        var labels = [];

        // we want our first label to land on a multiple of the label duration;
        // figure out where that lies.
        var firstLabelOffsetSeconds = (labelDuration - (this.startTime % labelDuration));

        //log ("sps", secondsPerPixel, "ldur", labelDuration, "nl", numLabels, "flo", firstLabelOffsetSeconds);

        for (var i = 0; i < numLabels; i++) {
            // figure out the time value of this label
            var ltime = this.startTime + firstLabelOffsetSeconds + i*labelDuration;
            if (ltime > this.endTime)
                break;

            // the first number is at what px position to place the label;
            // the second number is the actual value of the label
            // the third is an array of strings that go into the label
            var lval = [(ltime - this.startTime)/secondsPerPixel - (this.xLabelWidth/2), ltime, this.formatTimeLabel(ltime)];
            //log ("ltime", ltime, "lpos", lval[0], "end", this.endTime);
            labels.push(lval);
        }

        this.xAxisLabels = labels;
        return labels;
    },

    formatTimeLabel: function (ltime) {
        // ltime is in seconds since the epoch in, um, so
        var d = new Date (ltime*1000);
        var s1 = d.getHours() +
          (d.getMinutes() < 10 ? ":0" : ":") + d.getMinutes() +
          (d.getSeconds() < 10 ? ":0" : ":") + d.getSeconds();
        var s2 = d.getDate() + " " + MONTH_ABBREV[d.getMonth()] + " " + (d.getYear()+1900);
        return [s1, s2];
    },

    makeLabels: function () {
        //log ("makeLabels");
        if (this.xLabelContainer) {
            var labels = [];
            var xboxPos = YAHOO.util.Dom.getXY(this.xLabelContainer);
            xboxPos[0] = xboxPos[0] + this.borderLeft;
            xboxPos[1] = xboxPos[1] + this.borderTop;
            var labelValues = this.getTimeAxisLabels();

            for each (var lval in labelValues) {
                var xpos = /*xboxPos[0] +*/ lval[0];
                var div = new DIV({ class: "x-axis-label" });
                div.style.position = "absolute";
                div.style.width = this.xLabelWidth + "px";
                div.style.height = this.xLabelHeight + "px";
                div.style.left = xpos + "px";
                div.style.top = "0px"; //xboxPos[1] + this.frontBuffer.height;

                // XXX don't hardcode [2][0] etc.
                appendChildNodes(div, lval[2][0], new BR(), lval[2][1]);

                labels.push(div);
            }

            replaceChildNodes(this.xLabelContainer, labels);
        }

        if (this.yLabelContainer) {
            var labels = [];
            var yboxPos = YAHOO.util.Dom.getXY(this.yLabelContainer);
            yboxPos[0] = yboxPos[0] + this.borderLeft;
            yboxPos[1] = yboxPos[1] + this.borderTop;
            var labelValues = this.getValueAxisLabels();

            for each (var lval in labelValues) {
                var ypos = /*xboxPos[0] +*/ lval[0];
                var div = new DIV({ class: "y-axis-label" });
                div.style.position = "absolute";
                div.style.width = this.yLabelWidth + "px";
                div.style.height = this.yLabelHeight + "px";
                div.style.left = "0px"; //xboxPos[0]
                // XXX remove the -5 once we figure out how to vertically center text in this box
                div.style.top = (ypos-8) + "px";

                // XXX don't hardcode [2] etc.
                appendChildNodes(div, lval[2]);
                labels.push(div);
            }

            replaceChildNodes(this.yLabelContainer, labels);
        }

        if (0) {
            var labels = [];
            var total_sz = this.frontBuffer.height;

            // the ideal label height is 30px; 10% extra for gaps
            var sz_desired = 30;
            var nlabels = Math.floor(total_sz / (sz_desired * 1.10));
            var label_sz = Math.floor(total_sz / nlabels);

            //log ("lsz: " + label_sz + " nl: " + nlabels);

            for (var i = 0; i < nlabels; i++) {
                var pos = label_sz * i;
                var div = new DIV({class: "y-axis-label", style: "width: 50px; height: " + label_sz + "px" });
                appendChildNodes(div, "Label " + i);

                labels.push(div);
            }

            replaceChildNodes(this.yLabelContainer, labels);
        }
    },

    //
    // selection handling
    //
    selectionMouseDown: function(event) {
        if (!this.valid)
            return;

        if (this.selectionType == "range") {
            var pos = YAHOO.util.Dom.getX(this.frontBuffer) + this.borderLeft;
            this.dragState = { startX: event.pageX - pos };
            var ds = this.dragState;

            ds.curX = ds.startX + 1;
            ds.secondsPerPixel = (this.endTime - this.startTime) / this.frontBuffer.width;

            this.selectionStartTime = ds.startX * ds.secondsPerPixel + this.startTime;
            this.selectionEndTime = ds.curX * ds.secondsPerPixel + this.startTime;

            this.redrawOverlayOnly();
            
            this.selectionSweeping = true;
        } else if (this.selectionType == "cursor") {
            var pos = YAHOO.util.Dom.getX(this.frontBuffer) + this.borderLeft;
            var secondsPerPixel = (this.endTime - this.startTime) / this.frontBuffer.width;

            this.selectionCursorTime = (event.pageX - pos) * secondsPerPixel + this.startTime;

            this.redrawOverlayOnly();

            this.onSelectionChanged.fire("cursor", this.selectionCursorTime);
        }
    },

    abortSelection: function() {
        if (!this.selectionSweeping)
            return;

        this.selectionSweeping = false;
        this.redrawOverlayOnly();
    },

    clearSelection: function() {
        this.selectionSweeping = false;
        this.selectionStartTime = null;
        this.selectionEndTime = null;
        this.redrawOverlayOnly();
    },

    selectionUpdateFromEventPageCoordinate: function(pagex) {
        var pos = YAHOO.util.Dom.getX(this.frontBuffer) + this.borderLeft;
        var ds = this.dragState;
        ds.curX = pagex - pos;
        if (ds.curX > this.frontBuffer.width)
            ds.curX = this.frontBuffer.width;
        else if (ds.curX < 0)
            ds.curX = 0;

        var cxTime = (ds.curX * ds.secondsPerPixel) + this.startTime;
        var startxTime = (ds.startX * ds.secondsPerPixel) + this.startTime;
        if (ds.curX < ds.startX) {
            this.selectionEndTime = startxTime;
            this.selectionStartTime = cxTime;
        } else {
            this.selectionStartTime = startxTime;
            this.selectionEndTime = cxTime;
        }
    },

    selectionMouseMove: function(event) {
        if (!this.selectionSweeping)
            return;

        this.selectionUpdateFromEventPageCoordinate(event.pageX);

        this.redrawOverlayOnly();
    },

    selectionMouseUp: function(event) {
        if (!this.selectionSweeping)
            return;

        this.selectionSweeping = false;
        this.onSelectionChanged.fire("range", this.selectionStartTime, this.selectionEndTime);
    },

    selectionMouseOut: function(event) {
        if (!this.selectionSweeping)
            return;

        this.selectionUpdateFromEventPageCoordinate(event.pageX);

        this.selectionSweeping = false;
        this.onSelectionChanged.fire("range", this.selectionStartTime, this.selectionEndTime);
    },

    /*
     * cursor stuff
     */
    cursorMouseMove: function (event) {
        if (!this.valid)
            return;

        if (this.cursorType != "free")
            return;

        var pos = YAHOO.util.Dom.getXY(this.frontBuffer);
        pos[0] = pos[0] + this.borderLeft;
        pos[1] = pos[1] + this.borderTop;
        var secondsPerPixel = (this.endTime - this.startTime) / this.frontBuffer.width;
        var valuesPerPixel = 1.0 / this.yScale;

        this.cursorTime = (event.pageX - pos[0]) * secondsPerPixel + this.startTime;
        this.cursorValue = (this.frontBuffer.height - (event.pageY - pos[1])) * valuesPerPixel + this.yOffset;

        this.onCursorMoved.fire(this.cursorTime, this.cursorValue);

        this.redrawOverlayOnly();
    },

    cursorMouseOut: function (event) {
        if (!this.valid)
            return;

        if (this.cursorType != "free")
            return;

        this.cursorTime = null;
        this.cursorValue = null;

        this.onCursorMoved.fire(this.cursorTime, this.cursorValue);

        this.redrawOverlayOnly();
    },

    /*
     * marker stuff
     */
    deleteAllMarkers: function () {
        this.markers = new Array();
    },

    addMarker: function (mtime, mlabel) {
        this.markers.push (mtime);
        this.markers.push (mlabel);
    },

};

function BigGraph(canvasId) {
    this.__proto__.__proto__.init.call (this, canvasId);
}

BigGraph.prototype = {
    __proto__: new Graph(),

};

function SmallGraph(canvasId) {
    this.__proto__.__proto__.init.call (this, canvasId);
}

SmallGraph.prototype = {
    __proto__: new Graph(),

    yLabelHeight: 20,
};

var GraphFormModules = [];
var GraphFormModuleCount = 0;

function GraphFormModule(userConfig) {
    GraphFormModuleCount++;
    this.__proto__.__proto__.constructor.call(this, "graphForm" + GraphFormModuleCount, userConfig);
}

GraphFormModule.prototype = {
    __proto__: new YAHOO.widget.Module(),

    imageRoot: "",

    tinderbox: null,
    testname: null,
    baseline: false,
    average: false,

    init: function (el, userConfig) {
        var self = this;

        this.__proto__.__proto__.init.call(this, el/*, userConfig*/);
        
        this.cfg = new YAHOO.util.Config(this);
        this.cfg.addProperty("tinderbox", { suppressEvent: true });
        this.cfg.addProperty("testname", { suppressEvent: true });
        this.cfg.addProperty("baseline", { suppressEvent: true });

        if (userConfig)
            this.cfg.applyConfig(userConfig, true);

        //form = new FORM({ class: "graphform", action: "javascript:;"});
        form = new DIV({ class: "graphform-line" });
        el = new SPAN({ class: "graphform-first-span" });
        form.appendChild(el);

        el = new SELECT({ name: "testname",
                          class: "testname",
                          onchange: function(event) { self.onChangeTestname(); } });
        this.testSelect = el;
        form.appendChild(el);

        appendChildNodes(form, " on ");

        el = new SELECT({ name: "tinderbox",
                          class: "tinderbox",
                          onchange: function(event) { self.onChangeTinderbox(); } });
        this.tinderboxSelect = el;
        form.appendChild(el);

        appendChildNodes(form, " (average: ");
        el = new INPUT({ name: "average",
                         type: "checkbox",
                         onchange: function(event) { self.average = event.target.checked; } });
        this.averageCheckbox = el;
        form.appendChild(el);
        appendChildNodes(form, ")");

        this.colorDiv = new IMG({ style: "border: 1px solid black; vertical-align: middle; margin: 3px;",
                                   width: 15,
                                   height: 15,
                                   src: "js/img/clear.png" });
        form.appendChild(this.colorDiv);

        el = new IMG({ src: "js/img/plus.png", class: "plusminus",
                       onclick: function(event) { addGraphForm(); } });
        form.appendChild(el);
        el = new IMG({ src: "js/img/minus.png", class: "plusminus",
                       onclick: function(event) { self.remove(); } });
        form.appendChild(el);
/*
        el = new INPUT({ type: "radio",
                         name: "baseline",
                         onclick: function(event) { self.onBaseLineRadioClick(); } });
        form.appendChild(el);
*/


        this.setBody (form);

        var forceTinderbox = null, forceTestname = null;
        if (userConfig) {
            forceTinderbox = this.cfg.getProperty("tinderbox");
            forceTestname = this.cfg.getProperty("testname");
            baseline = this.cfg.getProperty("baseline");
            if (baseline)
                this.onBaseLineRadioClick();
        }

        Tinderbox.requestTinderboxList(function (tboxes) {
                                           var opts = [];
                                           for each (var tbox in tboxes) {
                                               opts.push(new OPTION({ value: tbox }, tbox));
                                           }
                                           replaceChildNodes(self.tinderboxSelect, opts);

                                           if (forceTinderbox != null) 
                                               self.tinderboxSelect.value = forceTinderbox;

                                           setTimeout(function () { self.onChangeTinderbox(forceTestname); }, 0);
                                       });

        GraphFormModules.push(this);
    },

    getQueryString: function (prefix) {
        return prefix + "tb=" + this.tinderbox + "&" + prefix + "tn=" + this.testname + "&" + prefix + "bl=" + (this.baseline ? "1" : "0");
    },

/*
    handleQueryStringData: function (prefix, qsdata) {
        var tbox = qsdata[prefix + "tb"];
        var tname = qsdata[prefix + "tn"];
        var baseline = (qsdata[prefix + "bl"] == "1");

        if (baseline)
            this.onBaseLineRadioClick();

        this.forcedTinderbox = tbox;
        this.tinderboxSelect.value = tbox;

        this.forcedTestname = tname;
        this.testSelect.value = tname;
        this.onChangeTinderbox();
    },
*/

    onChangeTinderbox: function (forceTestname) {
        var self = this;

        this.tinderbox = this.tinderboxSelect.value;

        this.testSelect.disabled = true;
        Tinderbox.requestTestListFor(this.tinderbox,
                                     function (tbox, tests) {
                                         var opts = [];
                                         for each (var test in tests) {
                                             opts.push(new OPTION({ value: test }, test));
                                         }

                                         self.testname = tests[0];
                                         replaceChildNodes(self.testSelect, opts);

                                         if (forceTestname) {
                                             self.testname = forceTestname;
                                             self.testSelect.value = forceTestname;
                                         }

                                         self.testSelect.disabled = false;
                                     });
    },

    onChangeTestname: function () {
        var self = this;

        this.testname = this.testSelect.value;
    },

    onBaseLineRadioClick: function () {
        GraphFormModules.forEach(function (g) { g.baseline = false; });
        this.baseline = true;
    },

    remove: function () {
        var nf = [];
        for each (var f in GraphFormModules) {
            if (f != this)
                nf.push(f);
        }
        GraphFormModules = nf;
        this.destroy();
    },
};

var Tinderbox;
var BigPerfGraph;
var SmallPerfGraph;
var Bonsai;

function loadingDone() {
    //createLoggingPane(true);

    Tinderbox = new TinderboxData();
    Tinderbox.init();

    Bonsai = new BonsaiService();

    SmallPerfGraph = new SmallGraph("smallgraph");
    SmallPerfGraph.setSelectionType("range");
    BigPerfGraph = new BigGraph("graph");
    BigPerfGraph.setSelectionType("cursor");
    BigPerfGraph.setCursorType("free");

    onDataLoadChanged();

    SmallPerfGraph.onSelectionChanged.subscribe (function (type, args, obj) {
                                                     log ("selchanged");
                                                     if (args[0] == "range") {
                                                         BigPerfGraph.setTimeRange (args[1], args[2]);
                                                         BigPerfGraph.autoScale();
                                                         BigPerfGraph.redraw();
                                                     }

                                                     updateLinkToThis();
                                                 });

    BigPerfGraph.onCursorMoved.subscribe (function (type, args, obj) {
                                              var time = args[0];
                                              var val = args[1];
                                              if (time != null && val != null) {
                                                  // cheat
                                                  var l = Graph.prototype.formatTimeLabel (time);
                                                  showStatus("Date: " + l[1] + " " + l[0] + " Value: " + val.toFixed(2));
                                              } else {
                                                  showStatus(null);
                                              }
                                          });
    if (document.location.hash) {
        handleHash(document.location.hash);
    } else {
        addGraphForm();
    }
}

function addGraphForm(config) {
    var m = new GraphFormModule(config);
    m.render (getElement("graphforms"));
    return m;
}

function onNoBaseLineClick() {
    GraphFormModules.forEach (function (g) { g.baseline = false; });
}

// whether the bonsai data query should redraw the graph or not
var gReadyForRedraw = true;

function onUpdateBonsai() {
    BigPerfGraph.deleteAllMarkers();

    getElement("bonsaibutton").disabled = true;

    if (gCurrentLoadRange) {
        if ((gCurrentLoadRange[1] - gCurrentLoadRange[0]) < (bonsaiNoForceDays * ONE_DAY_SECONDS) || gForceBonsai) {
            Bonsai.requestCheckinsBetween (gCurrentLoadRange[0], gCurrentLoadRange[1],
                                           function (bdata) {
                                               for (var i = 0; i < bdata.times.length; i++) {
                                                   BigPerfGraph.addMarker (bdata.times[i], bdata.who[i] + ": " + bdata.comment[i]);
                                               }
                                               if (gReadyForRedraw)
                                                   BigPerfGraph.redraw();

                                               getElement("bonsaibutton").disabled = false;
                                           });
        }
    }
}

function onGraph() {
    for each (var g in [BigPerfGraph, SmallPerfGraph]) {
        g.clearDataSets();
        g.setTimeRange(null, null);
    }

    gReadyForRedraw = false;

    // do the actual graph data request
    var baselineModule = null;
    GraphFormModules.forEach (function (g) { if (g.baseline) baselineModule = g; });
    if (baselineModule) {
        baselineModule.colorDiv.style.backgroundColor = "black";
        Tinderbox.requestValueDataSetFor (baselineModule.tinderbox, baselineModule.testname,
                                     function (tbox, test, ds) {
                                         try {
                                             log ("Got results for baseline: '" + tbox + "' '" + test + "' ds: " + ds);
                                             onGraphLoadRemainder(ds);
                                         } catch(e) { log(e); }
                                     });
    } else {
        onGraphLoadRemainder();
    }
}

function onGraphLoadRemainder(baselineDataSet) {
    for each (var graphModule in GraphFormModules) {
        // this would have been loaded earlier
        if (graphModule.baseline)
            continue;

        var autoExpand = true;
        if (SmallPerfGraph.selectionType == "range" &&
            SmallPerfGraph.selectionStartTime &&
            SmallPerfGraph.selectionEndTime)
        {
            BigPerfGraph.setTimeRange (SmallPerfGraph.selectionStartTime, SmallPerfGraph.selectionEndTime);
            autoExpand = false;
        }

        Tinderbox.requestValueDataSetFor (graphModule.tinderbox, graphModule.testname,
                                     function (tbox, test, ds) {
                                         try {
                                             if (baselineDataSet)
                                                 ds = ds.createRelativeTo(baselineDataSet);

                                             var gm = findGraphModule(tbox, test);
                                             if (gm)
                                                 gm.colorDiv.style.backgroundColor = ds.color;

                                             log ("got ds:", ds.firstTime, ds.lastTime, ds.data.length);
                                             var avgds = null;
                                             if (baselineDataSet == null &&
                                                 graphModule.average)
                                             {
                                                 avgds = ds.createAverage(gAverageInterval);
                                             }

                                             if (avgds)
                                                 log ("got avgds:", avgds.firstTime, avgds.lastTime, avgds.data.length);


                                             for each (g in [BigPerfGraph, SmallPerfGraph]) {
                                                 g.addDataSet(ds);
                                                 if (avgds)
                                                     g.addDataSet(avgds);
                                                 if (g == SmallPerfGraph || autoExpand)
                                                     g.expandTimeRange(ds.firstTime, ds.lastTime);
                                                 g.autoScale();

                                                 g.redraw();
                                                 gReadyForRedraw = true;
                                             }

                                             updateLinkToThis();

                                         } catch(e) { log(e); }
                                     });
    }
}

function onDataLoadChanged() {
    log ("loadchanged");
    if (getElement("load-days-radio").checked) {
        var dval = new Number(getElement("load-days-entry").value);
        log ("dval", dval);
        if (dval <= 0) {
            //getElement("load-days-entry").style.background-color = "red";
            return;
        } else {
            //getElement("load-days-entry").style.background-color = "inherit";
        }

        var d2 = Math.ceil(Date.now() / 1000);
        d2 = (d2 - (d2 % ONE_DAY_SECONDS)) + ONE_DAY_SECONDS;
        var d1 = Math.floor(d2 - (dval * ONE_DAY_SECONDS));
        log ("drange", d1, d2);

        Tinderbox.defaultLoadRange = [d1, d2];
        gCurrentLoadRange = [d1, d2];
    } else {
        Tinderbox.defaultLoadRange = null;
        gCurrentLoadRange = null;
    }

    Tinderbox.clearValueDataSets();

    // hack, reset colors
    randomColorBias = 0;
}

function findGraphModule(tbox, test) {
    for each (var gm in GraphFormModules) {
        if (gm.tinderbox == tbox && gm.testname == test)
            return gm;
    }
    return null;
}

function updateLinkToThis() {
    var qs = "";

    qs += SmallPerfGraph.getQueryString("sp");
    qs += "&";
    qs += BigPerfGraph.getQueryString("bp");

    var ctr = 1;
    for each (var gm in GraphFormModules) {
        qs += "&" + gm.getQueryString("m" + ctr);
        ctr++;
    }

    getElement("linktothis").href = document.location.pathname + "#" + qs;
}

function handleHash(hash) {
    var qsdata = {};
    for each (var s in hash.substring(1).split("&")) {
        var q = s.split("=");
        qsdata[q[0]] = q[1];
    }

    var ctr = 1;
    while (("m" + ctr + "tb") in qsdata) {
        var prefix = "m" + ctr;
        var tbox = qsdata[prefix + "tb"];
        var tname = qsdata[prefix + "tn"];
        var baseline = (qsdata[prefix + "bl"] == "1");

        // passing this is pretty stupid here

        var m = addGraphForm({tinderbox: tbox, testname: tname, baseline: baseline});
        //m.handleQueryStringData("m" + ctr, qsdata);

        ctr++;
    }

    SmallPerfGraph.handleQueryStringData("sp", qsdata);
    BigPerfGraph.handleQueryStringData("bp", qsdata);
}

function showStatus(s) {
    replaceChildNodes("status", s);
}

/* Get some pre-set colors in for the first 5 graphs, thens start randomly generating stuff */
var presetColorIndex = 0;
var presetColors = [
    [0.0, 0.0, 0.7, 1.0],
    [0.0, 0.5, 0.0, 1.0],
    [0.7, 0.0, 0.0, 1.0],
    [0.7, 0.0, 0.7, 1.0],
    [0.0, 0.7, 0.7, 1.0]
];

var randomColorBias = 0;
function randomColor() {
    if (presetColorIndex < presetColors.length) {
        return presetColors[presetColorIndex++];
    }

    var col = [
        (Math.random()*0.5) + ((randomColorBias==0) ? 0.5 : 0.2),
        (Math.random()*0.5) + ((randomColorBias==1) ? 0.5 : 0.2),
        (Math.random()*0.5) + ((randomColorBias==2) ? 0.5 : 0.2),
        1.0
    ];
    randomColorBias++;
    if (randomColorBias == 3)
        randomColorBias = 0;

    return col;
}

function lighterColor(col) {
    return [
        Math.min(0.85, col[0] * 1.2),
        Math.min(0.85, col[1] * 1.2),
        Math.min(0.85, col[2] * 1.2),
        col[3]
    ];
}

function colorToRgbString(col) {
    return "rgba("
        + Math.floor(col[0]*255) + ","
        + Math.floor(col[1]*255) + ","
        + Math.floor(col[2]*255) + ","
        + col[3]
        + ")";
}
