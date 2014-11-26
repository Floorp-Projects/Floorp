#! /usr/bin/env node
var fs = require('fs');
var url = require('url');
var data = fs.readFileSync('/home/worker/mozilla-central/source/b2g/config/gaia.json');

var repo_path = JSON.parse(data).repo_path;
console.log(url.resolve('https://hg.mozilla.org', repo_path));
