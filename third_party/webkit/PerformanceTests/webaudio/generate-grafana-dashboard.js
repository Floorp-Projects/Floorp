var OUTPUT_PATH="webaudio.json";

var benchmarks = require("./benchmarks.js");
var template = require("./template-row.json");
var dashboard = require("./webaudio-dashboard.json");
var fs = require('fs');

var str = JSON.stringify(template);

var names = benchmarks.benchmarks;

for (var i in benchmarks.benchmarks) {
  ["win", "linux", "mac", "android"].forEach(function(platform) {
    var out = str.replace(/{{benchmark}}/g, names[i]).replace(/{{platform}}/g, platform);
    dashboard.rows.push(JSON.parse(out));
  });
}

fs.writeFile(OUTPUT_PATH, JSON.stringify(dashboard, " ", 2), function(err) {
  if(err) {
    return console.log(err);
  }

  console.log("grafana dashboard saved as " + OUTPUT_PATH);
});
