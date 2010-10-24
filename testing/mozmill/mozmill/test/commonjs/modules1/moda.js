var modb = require("modb"),
    modc = require("../modules2/modc");
    
exports.add = modc.add;

exports.subtract = function(a, b) {
  return a - b;
}