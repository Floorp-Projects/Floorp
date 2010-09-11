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
function displayDuration(id, µs) document.getElementById(id).nextSibling.textContent = formatµs(µs);

function formatstamp(µs) new Date(µs/1000) +" ("+ formatµs(µs) +")";
function formatµs(µs) µs + " µs";
function formatms(ms) ms + " ms";

function point(stamp, µs, v, b) [stamp / 1000, µs / 1000, { appVersion: v, appBuild: b }];
function range(a, b) ({ from: a, to: b || a });
function mark(x, y) ({ xaxis: x, yaxis: y });
function major(r) { r.color = "black"; r.lineWidth = "3"; return r; }
function minor(r) { r.color = "lightgrey"; r.lineWidth = "1"; return r; }
function label(r, l) { r.label = l; return r; }
function majorMark(x, l) mark(label(major(range(x)), l));
function minorMark(x, l) mark(label(minor(range(x)), l));
function extensionMark(x, l) mark(label(range(x), l));

var graph, overview;
var options = { legend: { show: false, position: "ne", margin: 10, labelBoxBorderColor: "transparent" },
                xaxis: { mode: "time", min: Date.now() - 259200000 },
                yaxis: { min: 0, tickFormatter: formatms },
                selection: { mode: "xy", color: "#00A" },
                grid: { show: true, borderWidth: 0, markings: [], aboveData: true, tickColor: "white" },
                series: { lines: { show: true, fill: true },
                          points: { show: true, fill: true },
                        }
              };
var overviewOpts = $.extend(true, {}, options,
                            { xaxis: { ticks: [], mode: "time" },
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
query.executeAsync({
  handleResult: function(results)
  {
    var lastver, lastbuild;
    for (let row = results.getNextRow(); row; row = results.getNextRow())
    {
      var stamp = row.getResultByName("timestamp");
      var version = row.getResultByName("appVersion");
      var build = row.getResultByName("appBuild");
      if (lastver != version)
        options.grid.markings.push(majorMark(stamp));
      else
        if (lastbuild != build)
          options.grid.markings.push(minorMark(stamp));
      lastver = version;
      lastbuild = build;
      var l, s;
      series[1].data.push(point(stamp, l = row.getResultByName("launch"), version, build));
      series[0].data.push(point(stamp, l + (s = row.getResultByName("startup")), version, build));
      table.appendChild(tr(td(formatstamp(stamp)),
                           td(formatµs(l)),
                           td(formatµs(s)),
                           td(formatµs(l + s)),
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
    graph = $.plot($("#graph"), series, options);
    overview = $.plot($("#overview"), series, overviewOpts);
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
