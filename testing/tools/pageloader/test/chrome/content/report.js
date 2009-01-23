#!/usr/bin/js
load(['chrome/content/report.js']);

pages = [
  'http://www.google.com',
  'http://www.yahoo.com',
  'http://www.msn.com',
];

cycle_time = 5;
report = new Report(pages);

for (var c=0; c < cycle_time; c++) {
  for (var p=0; p < pages.length; p++) {
    report.recordTime(p, c+1);
  }
}

print(report.getReport());
