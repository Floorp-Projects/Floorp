
function do_info(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  dump("TEST-INFO | " + stack.filename + " | [" + stack.name + " : " +
       stack.lineNumber + "] " + text + "\n");
}
function run_test()
{
    var tests = [
	[ "http://mozilla.org/", "http://mozilla.org/somewhere/there", true ],
	[ "http://mozilla.org/", "http://www.mozilla.org/", false ],
	[ "http://mozilla.org/", "http://mozilla.org:80", true ],
	[ "http://mozilla.org/", "http://mozilla.org:90", false ],
	[ "http://mozilla.org", "https://mozilla.org", false ],
	[ "http://mozilla.org", "https://mozilla.org:80", false ],	
	[ "http://mozilla.org:443", "https://mozilla.org", false ],
	[ "https://mozilla.org:443", "https://mozilla.org", true ],
	[ "https://mozilla.org:443", "https://mozilla.org/somewhere/", true ],
	[ "about:", "about:", false ],
	[ "data:text/plain,text", "data:text/plain,text", false ],
	[ "about:blank", "about:blank", false ],
	[ "about:", "http://mozilla.org/", false ],
	[ "about:", "about:config", false ],
	[ "about:text/plain,text", "data:text/plain,text", false ],
	[ "jar:http://mozilla.org/!/", "http://mozilla.org/", true ],
	[ "view-source:http://mozilla.org/", "http://mozilla.org/", true ]
    ];

    var secman = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(Ci.nsIScriptSecurityManager);

    tests.forEach(function(aTest) {
        do_info("Comparing " + aTest[0] + " to " + aTest[1]);

	var uri1 = NetUtil.newURI(aTest[0]);
	var uri2 = NetUtil.newURI(aTest[1]);

	var equal;
	try {
	    secman.checkSameOriginURI(uri1, uri2, false, false);
	    equal = true;
	} catch (e) {
	    equal = false
	}
	Assert.equal(equal, aTest[2]);
    });
}
