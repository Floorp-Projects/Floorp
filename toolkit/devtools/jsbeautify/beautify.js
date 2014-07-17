var { cssBeautify } = require("devtools/toolkit/jsbeautify/beautify-css");
var { htmlBeautify } = require("devtools/toolkit/jsbeautify/beautify-html");
var { jsBeautify } = require("devtools/toolkit/jsbeautify/beautify-js");

exports.css = cssBeautify;
exports.html = htmlBeautify;
exports.js = jsBeautify;
