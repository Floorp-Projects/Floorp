#! /usr/bin/env node
var fs = require('fs');
var data = fs.readFileSync('/home/worker/mozilla-central/source/b2g/config/gaia.json');
console.log(JSON.parse(data).revision);
