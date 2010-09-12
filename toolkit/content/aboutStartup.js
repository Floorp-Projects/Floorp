var Cc = Components.classes;
var Ci = Components.interfaces;

let launched, startup, restored;

let runtime = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime);

try {
  displayTimestamp("launched", launched = runtime.launchTimestamp);
} catch(x) { }

displayTimestamp("started", startup = runtime.startupTimestamp);
if (launched)
  displayDuration("started", startup - launched);

let ss = Cc["@mozilla.org/browser/sessionstartup;1"].getService(Ci.nsISessionStartup);
displayTimestamp("restored", restored = ss.restoredTimestamp);
displayDuration("restored", restored - startup);

function displayTimestamp(id, µs) document.getElementById(id).textContent = formatstamp(µs);
function displayDuration(id, µs) document.getElementById(id).nextSibling.textContent = formatms(msFromµs(µs));

function msFromµs(µs) µs / 1000;
function formatstamp(µs) new Date(msFromµs(µs)) +" ("+ formatms(msFromµs(µs)) +")";
function formatµs(µs) µs + " µs";
function formatms(ms) ms + " ms";

function point(stamp, µs, v, b) [msFromµs(stamp), msFromµs(µs), { appVersion: v, appBuild: b }];
function range(a, b) ({ from: msFromµs(a), to: msFromµs(b || a) });
function mark(x, y) { var r = {}; x && (r.xaxis = x); y && (r.yaxis = y); return r };
function major(r) { r.color = "#444"; return r; }
function minor(r) { r.color = "#AAA"; return r; }
function label(r, l) { r.label = l; return r; }
function majorMark(x, l) label(major(mark(range(x))), l);
function minorMark(x, l) label(minor(mark(range(x))), l);
function extensionMark(x, l) label(mark(range(x)), l);

var graph, overview;
var options = { legend: { show: true, position: "ne", margin: 10, labelBoxBorderColor: "transparent" },
                xaxis: { mode: "time" },
                yaxis: { min: 0, tickFormatter: formatms },
                selection: { mode: "xy", color: "#00A" },
                grid: { show: true, borderWidth: 0, markings: [], aboveData: true, tickColor: "white" },
                series: { lines: { show: true, fill: true },
                          points: { show: true, fill: true },
                        },
              };
var overviewOpts = $.extend(true, {}, options,
                            { legend: { show: false },
                              xaxis: { ticks: [], mode: "time" },
                              yaxis: { ticks: [], min: 0, autoscaleMargin: 0.1 },
                              grid: { show: false },
                              series: { lines: { show: true, fill: true, lineWidth: 1 },
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
var table = document.getElementsByTagName("table")[1];

var file = Components.classes["@mozilla.org/file/directory_service;1"]
                     .getService(Components.interfaces.nsIProperties)
                     .get("ProfD", Components.interfaces.nsIFile);
file.append("startup.sqlite");

var svc = Components.classes["@mozilla.org/storage/service;1"]
                    .getService(Components.interfaces.mozIStorageService);
var db = svc.openDatabase(file);
var query = db.createStatement("SELECT timestamp, launch, startup, appVersion, appBuild, platformVersion, platformBuild FROM duration");
var lastver, lastbuild;
query.executeAsync({
  handleResult: function(results)
  {
    for (let row = results.getNextRow(); row; row = results.getNextRow())
    {
      var stamp = row.getResultByName("timestamp");
      var version = row.getResultByName("appVersion");
      var build = row.getResultByName("appBuild");
      if (lastver != version)
      {
        options.grid.markings.push(majorMark(stamp, "Firefox "+ version +" ("+ build +")"));
      }
      else
        if (lastbuild != build)
          options.grid.markings.push(minorMark(stamp, "Firefox "+ version +" ("+ build +")"));

      lastver = version;
      lastbuild = build;
      var l, s;
      series[1].data.push(point(stamp, l = row.getResultByName("launch"), version, build));
      series[0].data.push(point(stamp, l + (s = row.getResultByName("startup")), version, build));
      table.appendChild(tr(td(formatstamp(stamp)),
                           td(formatms(msFromµs(l))),
                           td(formatms(msFromµs(s))),
                           td(formatms(msFromµs((l + s)))),
                           td(version),
                           td(build),
                           td(row.getResultByName("platformVersion")),
                           td(row.getResultByName("platformBuild"))));
    }
  },
  handleError: function(error)
  {
    table.appendChild(tr(td("Error: "+ error.message +" ("+ error.result +")")));
  },
  handleCompletion: function()
  {
    var table = $("table");
    var height = $(window).height() - (table.offset().top + table.outerHeight(true)) - 110;
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

    //var axes = graph.getAxes();
    //overview.setSelection({ xaxis: { min: axes.xaxis.min,
    //                                 max: axes.xaxis.max
    //                               },
    //                        yaxis: { min: axes.yaxis.min,
    //                                 max: axes.yaxis.max
    //                               }
    //                      },
    //                      true);
  },
});

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
