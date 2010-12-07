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
 * The Original Code is the about:startup page.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Brooks <db48x@db48x.net>
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

var Cc = Components.classes;
var Ci = Components.interfaces;
Components.utils.import("resource://gre/modules/Services.jsm");
let dateService = Cc["@mozilla.org/intl/scriptabledateformat;1"]
                    .getService(Ci.nsIScriptableDateFormat);

let strings = Services.strings.createBundle("chrome://global/locale/aboutStartup.properties");
let branding = Services.strings.createBundle("chrome://branding/locale/brand.properties");
let brandShortName = branding.GetStringFromName("brandShortName");

function displayTimestamp(id, µs) document.getElementById(id).textContent = formatstamp(µs);
function displayDuration(id, µs) document.getElementById(id).nextSibling.textContent = formatms(msFromµs(µs));

function getStr(str)
{
  try {
    return strings.getStringFromName("about.startup."+ str);
  } catch (x) {
    return str;
  }
}
function formatStr(str, args)
{
  try {
    return strings.formatStringFromName("about.startup."+ str, args, args.length);
  } catch (x) {
    return str +" "+ args.toSource();
  }
}
function appVersion(version, build) formatStr("appVersion", [brandShortName, version, build]);
function formatExtension(str, name, version) formatStr("extension"+(str.replace(/^on/, "")
                                                                       .replace(/ing$/, "ed")),
                                                       [name, version]);
function formatDate(date) dateService.FormatDateTime("", dateService.dateFormatLong,
                                                     dateService.timeFormatSeconds,
                                                     date.getFullYear(), date.getMonth()+1,
                                                     date.getDate(), date.getHours(),
                                                     date.getMinutes(), date.getSeconds());

function msFromµs(µs) µs / 1000;
function formatstamp(µs) formatDate(new Date(msFromµs(µs)));
function formatµs(µs) µs + " µs";
function formatms(ms) formatStr("milliseconds", [ms]);

function point(stamp, µs, v, b) [msFromµs(stamp), msFromµs(µs), { appVersion: v, appBuild: b }];
function range(a, b) ({ from: msFromµs(a), to: msFromµs(b || a) });
function mark(x, y) { var r = {}; x && (r.xaxis = x); y && (r.yaxis = y); return r };
function label(r, l) $.extend(r, { label: l });
function color(r, c) $.extend(r, { color: c });
function major(r) color(r, "#444");
function minor(r) color(r, "#AAA");
function green(r) color(r, "#00F");
function majorMark(x, l) label(major(mark(range(x))), l);
function minorMark(x, l) label(minor(mark(range(x))), l);
function extensionMark(x, l) label(green(mark(range(x))), l);

function clamp(min, value, max) Math.max(min, (Math.min(value, max)));

///// First, display the timings from the current startup
let launched, startup, restored;

let runtime = Services.appinfo;
runtime.QueryInterface(Ci.nsIXULRuntime_MOZILLA_2_0);
try {
  displayTimestamp("launched", launched = runtime.launchTimestamp);
} catch(x) { }

displayTimestamp("started", startup = runtime.startupTimestamp);
if (launched)
  displayDuration("started", startup - launched);

let app = Cc["@mozilla.org/toolkit/app-startup;1"]
            .getService(Ci.nsIAppStartup_MOZILLA_2_0);
displayTimestamp("restored", restored = app.restoredTimestamp);
displayDuration("restored", restored - startup);

///// Next, load the database
var file = Services.dirsvc.get("ProfD", Components.interfaces.nsIFile);
file.append("startup.sqlite");

var db = Services.storage.openDatabase(file);

///// set up the graph options
var graph, overview;
var options = { legend: { show: true, position: "ne", margin: 10,
                          labelBoxBorderColor: "transparent" },
                xaxis: { mode: "time" },
                yaxis: { min: 0, tickFormatter: formatms },
                selection: { mode: "xy", color: "#00A" },
                grid: { show: true, borderWidth: 0, markings: [],
                        aboveData: true, tickColor: "white" },
                series: { lines: { show: true, fill: true },
                          points: { show: true, fill: true },
                        },
                colors: ["#67A9D8", "#FFC170"],
              };
var overviewOpts = $.extend(true, {}, options,
                            { legend: { show: false },
                              xaxis: { ticks: [], mode: "time" },
                              yaxis: { ticks: [], min: 0,
                                       autoscaleMargin: 0.1 },
                              grid: { show: false },
                              series: { lines: { show: true, fill: true,
                                                 lineWidth: 1 },
                                        points: { show: false },
                                        shadowSize: 0
                                      },
                            });

var series = [{ label: getStr("duration.launch"),
                data: []
              },
              { label: getStr("duration.startup"),
                data: []
              }
             ];

$("#graph").bind("plotselected", function (event, ranges)
{
  // do the zooming
  graph = $.plot($("#graph"),
                 series,
                 $.extend(true, {}, options,
                          { xaxis: { min: ranges.xaxis.from,
                                     max: ranges.xaxis.to
                                   },
                            yaxis: { min: ranges.yaxis.from,
                                     max: ranges.yaxis.to
                                   },
                          }));

  // don't fire event on the overview to prevent eternal loop
  overview.setSelection(ranges, true);
});

$("#overview").bind("plotselected", function (event, ranges) {
  graph.setSelection(ranges);
});

///// read everything from the database and graph it
let work = (function()
{
  populateMeasurements();
  yield;
  populateEvents();
  yield;
})();

function go()
{
  try {
    work.next();
  } catch (x if x instanceof StopIteration) {
    // harmless
  }
}

go();

function populateMeasurements()
{
  let s = "SELECT timestamp, launch, startup, appVersion, appBuild FROM duration";
  var query = db.createStatement(s);
  var lastver, lastbuild;
  query.executeAsync({
    handleResult: function(results)
    {
      let hasresults = false;
      let table = document.getElementById("duration-table");
      for (let row = results.getNextRow(); row; row = results.getNextRow())
      {
        hasresults = true;
        let stamp = row.getResultByName("timestamp");
        let version = row.getResultByName("appVersion");
        let build = row.getResultByName("appBuild");
        if (lastver != version)
        {
          options.grid.markings.push(majorMark(stamp, appVersion(version,
                                                                 build)));
        }
        else
        {
          if (lastbuild != build)
          {
            options.grid.markings.push(minorMark(stamp, appVersion(version,
                                                                   build)));
          }
        }

        lastver = version;
        lastbuild = build;
        let l = row.getResultByName("launch"),
            s = row.getResultByName("startup");
        series[1].data.push(point(stamp, l, version, build));
        series[0].data.push(point(stamp, l + s, version, build));
        table.appendChild(tr(td(formatstamp(stamp)),
                             td(formatms(msFromµs(l))),
                             td(formatms(msFromµs(s))),
                             td(formatms(msFromµs((l + s)))),
                             td(version),
                             td(build)));
      }
      if (hasresults)
        $("#duration-table > .empty").hide();
    },
    handleError: function(error)
    {
      $("#duration-table").appendChild(tr(td("Error: "+ error.message +" ("+
                                             error.result +")")));
    },
    handleCompletion: function()
    {
      var table = $("table");
      var height = $(window).height() - (table.offset().top +
                                         table.outerHeight(true))
                                      - 110;
      $("#graph").height(Math.max(350, height));

      options.xaxis.min = Date.now() - clamp(3600000,    // 1 hour in milliseconds
                                             Date.now() - series[0].data[0][0],
                                             604800000); // 7 days in milliseconds

      var max = 0;
      for each (let [stamp, d] in series[0].data)
        if (stamp >= options.xaxis.min && d > max)
          max = d;
      options.yaxis.max = max;

      graph = $.plot($("#graph"), series, options);

      var offset = graph.getPlotOffset().left;
      $("#overview").width($("#overview").width() - offset);
      $("#overview").css("margin-left", offset);
      overview = $.plot($("#overview"), series, overviewOpts);
      go();
    },
  });
}

function populateEvents()
{
  let s = "SELECT timestamp, extid, name, version, action FROM events";
  var query = db.createStatement(s);
  let lastver, lastbuild;
  let hasresults;

  query.executeAsync({
    handleResult: function(results)
    {
      Application.getExtensions(function (extensions)
      {
        let table = document.getElementById("events-table");
        for (let row = results.getNextRow(); row; row = results.getNextRow())
        {
          hasresults = true;
          let stamp = row.getResultByName("timestamp"),
              id = row.getResultByName("extid"),
              extension = extensions.get(id),
              name = extension ? extension.name : row.getResultByName("name"),
              version = row.getResultByName("version"),
              action = row.getResultByName("action");

          options.grid.markings.push(extensionMark(stamp,
                                                   formatExtension(action,
                                                                   name,
                                                                   version)));
          table.appendChild(tr(td(formatstamp(stamp)),
                               td(action),
                               td(name),
                               td(id),
                               td(version)));
        }
        if (hasresults)
          $("#events-table > .empty").hide();
      });
    },
    handleError: function(error)
    {
      $("#events-table").appendChild(tr(td("Error: "+ error.message +" ("+
                                           error.result +")")));
    },
    handleCompletion: function()
    {
      graph = $.plot($("#graph"), series, options);
      go();
    },
  });
}

function td(str)
{
  let cell = document.createElement("td");
  cell.innerHTML = str;
  return cell;
}

function tr()
{
  let row = document.createElement("tr");
  Array.forEach(arguments, function(cell) { row.appendChild(cell); });
  return row;
}

function showTable()
{
  $("#showtable").hide();
  $("#showgraph").show();
  $("#graphs").hide();
  $("#tables").show();
}

function showGraph()
{
  $("#showgraph").hide();
  $("#showtable").show();
  $("#tables").hide();
  $("#graphs").show();
}
