var Cc = Components.classes;
var Ci = Components.interfaces;

let stringsvc = Components.classes["@mozilla.org/intl/stringbundle;1"]
                          .getService(Components.interfaces.nsIStringBundleService);
let strings = stringsvc.createBundle("chrome://global/locale/aboutStartup.properties");
let branding = stringsvc.createBundle("chrome://branding/locale/brand.properties");
let brandShortName = branding.GetStringFromName("brandShortName");

function displayTimestamp(id, µs) document.getElementById(id).textContent = formatstamp(µs);
function displayDuration(id, µs) document.getElementById(id).nextSibling.textContent = formatms(msFromµs(µs));

function formatStr(str, args) strings.formatStringFromName("about.startup."+ str, args, args.length);
function appVersion(version, build) formatStr("appVersion", [brandShortName, version, build]);
function formatExtension(str, name, version) formatStr("extension"+str, [name, version]);

function msFromµs(µs) µs / 1000;
function formatstamp(µs) new Date(msFromµs(µs));
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

///// First, display the timings from the current startup
let launched, startup, restored;
let runtime = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime);

try {
  displayTimestamp("launched", launched = runtime.launchTimestamp);
} catch(x) { }

displayTimestamp("started", startup = runtime.startupTimestamp);
if (launched)
  displayDuration("started", startup - launched);

let ss = Cc["@mozilla.org/browser/sessionstartup;1"]
           .getService(Ci.nsISessionStartup);
displayTimestamp("restored", restored = ss.restoredTimestamp);
displayDuration("restored", restored - startup);

///// Next, load the database
var file = Components.classes["@mozilla.org/file/directory_service;1"]
                     .getService(Components.interfaces.nsIProperties)
                     .get("ProfD", Components.interfaces.nsIFile);
file.append("startup.sqlite");

var svc = Components.classes["@mozilla.org/storage/service;1"]
                    .getService(Components.interfaces.mozIStorageService);
var db = svc.openDatabase(file);

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

var series = [{ label: "Launch Time",
                data: []
              },
              { label: "Startup Time",
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
          if (lastbuild != build)
            options.grid.markings.push(minorMark(stamp, appVersion(version,
                                                                   build)));

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

      options.xaxis.min = Date.now() - 604800000; // 7 days in milliseconds
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
  let s = "SELECT timestamp, id, name, version, action FROM events";
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
              id = row.getResultByName("id"),
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
