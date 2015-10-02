// Test that URLs containing characters in the IDN blacklist are
// always displayed as punycode
const testcases = [
    //  Original  Punycode or
    //            normalized form
    //
    ["\u00BC", "xn--14-c6t"],
    ["\u00BD", "xn--12-c6t"],
    ["\u00BE", "xn--34-c6t"],
    ["\u01C3", "xn--ija"],
    ["\u02D0", "xn--6qa"],
    ["\u0337", "xn--4ta"],
    ["\u0338", "xn--5ta"],
    ["\u0589", "xn--3bb"],
    ["\u05C3", "xn--rdb"],
    ["\u05F4", "xn--5eb"],
    ["\u0609", "xn--rfb"],
    ["\u060A", "xn--sfb"],
    ["\u066A", "xn--jib"],
    ["\u06D4", "xn--klb"],
    ["\u0701", "xn--umb"],
    ["\u0702", "xn--vmb"],
    ["\u0703", "xn--wmb"],
    ["\u0704", "xn--xmb"],
    ["\u115F", "xn--osd"],
    ["\u1160", "xn--psd"],
    ["\u1735", "xn--d0e"],
    ["\u2027", "xn--svg"],
    ["\u2028", "xn--tvg"],
    ["\u2029", "xn--uvg"],
    ["\u2039", "xn--bwg"],
    ["\u203A", "xn--cwg"],
    ["\u2041", "xn--jwg"],
    ["\u2044", "xn--mwg"],
    ["\u2052", "xn--0wg"],
    ["\u2153", "xn--13-c6t"],
    ["\u2154", "xn--23-c6t"],
    ["\u2155", "xn--15-c6t"],
    ["\u2156", "xn--25-c6t"],
    ["\u2157", "xn--35-c6t"],
    ["\u2158", "xn--45-c6t"],
    ["\u2159", "xn--16-c6t"],
    ["\u215A", "xn--56-c6t"],
    ["\u215B", "xn--18-c6t"],
    ["\u215C", "xn--38-c6t"],
    ["\u215D", "xn--58-c6t"],
    ["\u215E", "xn--78-c6t"],
    ["\u215F", "xn--1-zjn"],
    ["\u2215", "xn--w9g"],
    ["\u2236", "xn--ubh"],
    ["\u23AE", "xn--lmh"],
    ["\u2571", "xn--hzh"],
    ["\u29F6", "xn--jxi"],
    ["\u29F8", "xn--lxi"],
    ["\u2AFB", "xn--z4i"],
    ["\u2AFD", "xn--14i"],
    ["\u2FF0", "xn--85j"],
    ["\u2FF1", "xn--95j"],
    ["\u2FF2", "xn--b6j"],
    ["\u2FF3", "xn--c6j"],
    ["\u2FF4", "xn--d6j"],
    ["\u2FF5", "xn--e6j"],
    ["\u2FF6", "xn--f6j"],
    ["\u2FF7", "xn--g6j"],
    ["\u2FF8", "xn--h6j"],
    ["\u2FF9", "xn--i6j"],
    ["\u2FFA", "xn--j6j"],
    ["\u2FFB", "xn--k6j"],
    ["\u3014", "xn--96j"],
    ["\u3015", "xn--b7j"],
    ["\u3033", "xn--57j"],
    ["\u3164", "xn--psd"],
    ["\u321D", "xn--()-357j35d"],
    ["\u321E", "xn--()-357jf36c"],
    ["\u33AE", "xn--rads-id9a"],
    ["\u33AF", "xn--rads2-4d6b"],
    ["\u33C6", "xn--ckg-tc2a"],
    ["\u33DF", "xn--am-6bv"],
    ["\uA789", "xn--058a"],
    ["\uFE3F", "xn--x6j"],
    ["\uFE5D", "xn--96j"],
    ["\uFE5E", "xn--b7j"],
    ["\uFFA0", "xn--psd"],
    ["\uFFF9", "xn--vn7c"],
    ["\uFFFA", "xn--wn7c"],
    ["\uFFFB", "xn--xn7c"],
    ["\uFFFC", "xn--yn7c"],
    ["\uFFFD", "xn--zn7c"],

    // Characters from the IDN blacklist that normalize to ASCII
    // If we start using STD3ASCIIRules these will be blocked (bug 316444)
    ["\u00A0", " "],
    ["\u2000", " "],
    ["\u2001", " "],
    ["\u2002", " "],
    ["\u2003", " "],
    ["\u2004", " "],
    ["\u2005", " "],
    ["\u2006", " "],
    ["\u2007", " "],
    ["\u2008", " "],
    ["\u2009", " "],
    ["\u200A", " "],
    ["\u2024", "."],
    ["\u202F", " "],
    ["\u205F", " "],
    ["\u3000", " "],
    ["\u3002", "."],
    ["\uFE14", ";"],
    ["\uFE15", "!"],
    ["\uFF0E", "."],
    ["\uFF0F", "/"],
    ["\uFF61", "."],

    // Characters from the IDN blacklist that are stripped by Nameprep
    ["\u200B", ""],
    ["\uFEFF", ""],
];


function run_test() {
    var pbi = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
    var oldProfile = pbi.getCharPref("network.IDN.restriction_profile", "moderate");
    var oldWhiteListCom;
    try {
        oldWhitelistCom = pbi.getBoolPref("network.IDN.whitelist.com");
    } catch(e) {
        oldWhitelistCom = false;
    }
    var idnService = Cc["@mozilla.org/network/idn-service;1"].getService(Ci.nsIIDNService);

    pbi.setCharPref("network.IDN.restriction_profile", "moderate");
    pbi.setBoolPref("network.IDN.whitelist.com", false);

    for (var j = 0; j < testcases.length; ++j) {
        var test = testcases[j];
        var URL = test[0] + ".com";
        var punycodeURL = test[1] + ".com";
        var isASCII = {};

	var result;
	try {
	    result = idnService.convertToDisplayIDN(URL, isASCII);
	} catch(e) {
	    result = ".com";
	}
        if (punycodeURL.substr(0, 4) == "xn--") {
            // test convertToDisplayIDN with a Unicode URL and with a
            //  Punycode URL if we have one
            do_check_eq(escape(result), escape(punycodeURL));

            result = idnService.convertToDisplayIDN(punycodeURL, isASCII);
            do_check_eq(escape(result), escape(punycodeURL));
        } else {
            // The "punycode" URL isn't punycode. This happens in testcases
            // where the Unicode URL has become normalized to an ASCII URL,
            // so, even though expectedUnicode is true, the expected result
            // is equal to punycodeURL
            do_check_eq(escape(result), escape(punycodeURL));
        }
    }
    pbi.setBoolPref("network.IDN.whitelist.com", oldWhitelistCom);
    pbi.setCharPref("network.IDN.restriction_profile", oldProfile);
}
