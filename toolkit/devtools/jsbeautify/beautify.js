var { cssBeautify } = require("devtools/jsbeautify/beautify-css");
var { htmlBeautify } = require("devtools/jsbeautify/beautify-html");
var { jsBeautify } = require("devtools/jsbeautify/beautify-js");

exports.css = cssBeautify;
exports.html = htmlBeautify;
exports.js = jsBeautify;
