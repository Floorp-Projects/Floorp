// Test based on submitted one from Peter B Shalimoff

var test = function(s, funcName){
    function Arg(){};
    Arg.prototype.toString = function(){
	info("Testing " + funcName + " with null args");
	return this.value;
    };
    // create a generic arg lits of null, -1, and 10 nulls
    var args = [s, -1];
    for (var i = 0; i < 10; ++i) {
	args.push(new Arg());
    }
    var up = Components.classes["@mozilla.org/network/url-parser;1?auth=maybe"].getService(Components.interfaces.nsIURLParser);
    try {
	up[funcName].apply(up, args);
	return args;
	} catch (x) {
	    Assert.ok(true); // make sure it throws an exception instead of crashing
	    return x;
	}
    // should always have an exception to catch
    Assert.ok(false);
};
var s = null;
var funcs = ["parseAuthority", "parseFileName", "parseFilePath", "parsePath", "parseServerInfo", "parseURL", "parseUserInfo"];

function run_test() {
    funcs.forEach(function(f){test(s, f);});
}
