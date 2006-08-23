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

// all times are in seconds

const ONE_HOUR_SECONDS = 60*60;
const ONE_DAY_SECONDS = 24*ONE_HOUR_SECONDS;
const ONE_WEEK_SECONDS = 7*ONE_DAY_SECONDS;
const ONE_YEAR_SECONDS = 365*ONE_DAY_SECONDS; // leap years whatever.

const MONTH_ABBREV = [ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" ];

const bonsaicgi = "bonsaibouncer.cgi";

// more days than this and we'll force user confirmation for the bonsai query
const bonsaiNoForceDays = 90;

// the default average interval
var gAverageInterval = 3*ONE_HOUR_SECONDS;
var gCurrentLoadRange = null;
var gForceBonsai = false;

var Tinderbox;
var BigPerfGraph;
var SmallPerfGraph;
var Bonsai;

function loadingDone() {
    //createLoggingPane(true);

    Tinderbox = new TinderboxData();
    Tinderbox.init();

    Bonsai = new BonsaiService();

    SmallPerfGraph = new CalendarTimeGraph("smallgraph");
    SmallPerfGraph.yLabelHeight = 20;
    SmallPerfGraph.setSelectionType("range");
    BigPerfGraph = new CalendarTimeGraph("graph");
    BigPerfGraph.setSelectionType("cursor");
    BigPerfGraph.setCursorType("free");

    onDataLoadChanged();

    SmallPerfGraph.onSelectionChanged.
        subscribe (function (type, args, obj) {
                       log ("selchanged");

                       if (args[0] == "range") {
                           if (args[1] && args[2]) {
                               var t1 = args[1];
                               var t2 = args[2];

                               var foundIndexes = [];

                               // make sure that there are at least two points
                               // on at least one graph for this
                               var foundPoints = false;
                               var dss = BigPerfGraph.dataSets;
                               for (var i = 0; i < dss.length; i++) {
                                   var idcs = dss[i].indicesForTimeRange(t1, t2);
                                   if (idcs[1] - idcs[0] > 1) {
                                       foundPoints = true;
                                       break;
                                   }
                                   foundIndexes.push(idcs);
                               }

                               if (!foundPoints) {
                                   // we didn't find at least two points in at least
                                   // one graph; so munge the time numbers until we do.
                                   log("Orig t1 " + t1 + " t2 " + t2);

                                   for (var i = 0; i < dss.length; i++) {
                                       if (foundIndexes[i][0] > 0) {
                                           t1 = Math.min(dss[i].data[(foundIndexes[i][0] - 1) * 2], t1);
                                       } else if (foundIndexes[i][1]+1 < (ds.data.length/2)) {
                                           t2 = Math.max(dss[i].data[(foundIndexes[i][1] + 1) * 2], t2);
                                       }
                                   }

                                   log("Fixed t1 " + t1 + " t2 " + t2);
                               }

                               BigPerfGraph.setTimeRange (t1, t2);
                           } else {
                               BigPerfGraph.setTimeRange (SmallPerfGraph.startTime, SmallPerfGraph.endTime);
                           }
                           BigPerfGraph.autoScale();
                           BigPerfGraph.redraw();
                       }
                       
                       updateLinkToThis();
                   });

    BigPerfGraph.onCursorMoved.
        subscribe (function (type, args, obj) {
                       var time = args[0];
                       var val = args[1];
                       if (time != null && val != null) {
                           // cheat
                           showStatus("Date: " + formatTime(time) + " Value: " + val.toFixed(2));
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
    m.setColor(randomColor());
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
        Tinderbox.requestDataSetFor (baselineModule.testId,
                                     function (testid, ds) {
                                         try {
                                             log ("Got results for baseline: '" + testid + "' ds: " + ds);
                                             ds.color = baselineModule.color;
                                             onGraphLoadRemainder(ds);
                                         } catch(e) { log(e); }
                                     });
    } else {
        onGraphLoadRemainder();
    }
}

function onGraphLoadRemainder(baselineDataSet) {
    for each (var graphModule in GraphFormModules) {
        log ("onGraphLoadRemainder: ", graphModule.id, graphModule.testId, "color:", graphModule.color);

        // this would have been loaded earlier
        if (graphModule.baseline)
            continue;

        var autoExpand = true;
        if (SmallPerfGraph.selectionType == "range" &&
            SmallPerfGraph.selectionStartTime &&
            SmallPerfGraph.selectionEndTime)
        {
            if (SmallPerfGraph.selectionStartTime < gCurrentLoadRange[0] ||
                SmallPerfGraph.selectionEndTime > gCurrentLoadRange[1])
            {
                SmallPerfGraph.selectionStartTime = Math.max (SmallPerfGraph.selectionStartTime, gCurrentLoadRange[0]);
                SmallPerfGraph.selectionEndTime = Math.min (SmallPerfGraph.selectionEndTime, gCurrentLoadRange[1]);
            }

            BigPerfGraph.setTimeRange (SmallPerfGraph.selectionStartTime, SmallPerfGraph.selectionEndTime);
            autoExpand = false;
        }

        // we need a new closure here so that we can get the right value
        // of graphModule in our closure
        var makeCallback = function (module) {
            return function (testid, ds) {
                try {
                    ds.color = module.color;

                    if (baselineDataSet)
                        ds = ds.createRelativeTo(baselineDataSet);

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
                        if (g == SmallPerfGraph || autoExpand) {
                            g.expandTimeRange(Math.max(ds.firstTime, gCurrentLoadRange ? gCurrentLoadRange[0] : ds.firstTime),
                                              Math.min(ds.lastTime, gCurrentLoadRange ? gCurrentLoadRange[1] : ds.lastTime));
                        }

                        g.autoScale();

                        g.redraw();
                        gReadyForRedraw = true;
                    }

                    updateLinkToThis();
                } catch(e) { log(e); }
            };
        };

        Tinderbox.requestDataSetFor (graphModule.testId, makeCallback(graphModule));
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

function findGraphModule(testId) {
    for each (var gm in GraphFormModules) {
        if (gm.testId == testId)
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
