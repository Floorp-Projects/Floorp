var {utils: Cu, interfaces: Ci, classes: Cc} = Components;

Cu.import("resource://gre/modules/Services.jsm");

const BASE_HOSTNAMES = ["example.org", "example.co.uk"];
const SUBDOMAINS = ["", "pub.", "www.", "other."];

const cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
const cm = cs.QueryInterface(Ci.nsICookieManager2);

function run_test() {
    Services.prefs.setIntPref("network.cookie.staleThreshold", 0);
    for (var host of BASE_HOSTNAMES) {
        do_print('testing with host ' + host);
        var base = SUBDOMAINS[0] + host;
        var sub = SUBDOMAINS[1] + host;
        var other = SUBDOMAINS[2] + host;
        var another = SUBDOMAINS[3] + host;
        test_basic_eviction(base, sub, other, another);
        cm.removeAll();
        test_domain_or_path_matches_not_both(base, sub, other, another);
        cm.removeAll();
    }
    test_localdomain();
}

// Verify that subdomains of localhost are treated as separate hosts and aren't considered
// candidates for eviction.
function test_localdomain() {
    Services.prefs.setIntPref("network.cookie.maxPerHost", 2);

    const BASE_URI = Services.io.newURI("http://localhost", null, null);
    const BASE_BAR = Services.io.newURI("http://localhost/bar", null, null);
    const OTHER_URI = Services.io.newURI("http://other.localhost", null, null);
    const OTHER_BAR = Services.io.newURI("http://other.localhost/bar", null, null);
    
    setCookie("session_no_path", null, null, null, BASE_URI);
    setCookie("session_bar_path", null, "/bar", null, BASE_BAR);

    setCookie("session_no_path", null, null, null, OTHER_URI);
    setCookie("session_bar_path", null, "/bar", null, OTHER_BAR);

    verifyCookies(['session_no_path',
                   'session_bar_path'], BASE_URI);
    verifyCookies(['session_no_path',
                   'session_bar_path'], OTHER_URI);

    setCookie("session_another_no_path", null, null, null, BASE_URI);
    verifyCookies(['session_no_path',
                   'session_another_no_path'], BASE_URI);

    setCookie("session_another_no_path", null, null, null, OTHER_URI);
    verifyCookies(['session_no_path',
                   'session_another_no_path'], OTHER_URI);
}

// Ensure that cookies are still considered candidates for eviction if either the domain
// or path matches, but not both.
function test_domain_or_path_matches_not_both(base_host,
                                              subdomain_host,
                                              other_subdomain_host,
                                              another_subdomain_host) {
    Services.prefs.setIntPref("network.cookie.maxPerHost", 2);

    const BASE_URI = Services.io.newURI("http://" + base_host, null, null);
    const PUB_FOO_PATH = Services.io.newURI("http://" + subdomain_host + "/foo", null, null);
    const WWW_BAR_PATH = Services.io.newURI("http://" + other_subdomain_host + "/bar", null, null);
    const OTHER_BAR_PATH = Services.io.newURI("http://" + another_subdomain_host + "/bar", null, null);
    const PUB_BAR_PATH = Services.io.newURI("http://" + subdomain_host + "/bar", null, null);
    const WWW_FOO_PATH = Services.io.newURI("http://" + other_subdomain_host + "/foo", null, null);

    setCookie("session_pub_with_foo_path", subdomain_host, "/foo", null, PUB_FOO_PATH);
    setCookie("session_www_with_bar_path", other_subdomain_host, "/bar", null, WWW_BAR_PATH);
    verifyCookies(['session_pub_with_foo_path',
                   'session_www_with_bar_path'], BASE_URI);

    setCookie("session_pub_with_bar_path", subdomain_host, "/bar", null, PUB_BAR_PATH);
    verifyCookies(['session_www_with_bar_path',
                   'session_pub_with_bar_path'], BASE_URI);

    setCookie("session_other_with_bar_path", another_subdomain_host, "/bar", null, OTHER_BAR_PATH);
    verifyCookies(['session_pub_with_bar_path',
                   'session_other_with_bar_path'], BASE_URI);
}

function test_basic_eviction(base_host, subdomain_host, other_subdomain_host) {
    Services.prefs.setIntPref("network.cookie.maxPerHost", 5);

    const BASE_URI = Services.io.newURI("http://" + base_host, null, null);
    const SUBDOMAIN_URI = Services.io.newURI("http://" + subdomain_host, null, null);
    const OTHER_SUBDOMAIN_URI = Services.io.newURI("http://" + other_subdomain_host, null, null);
    const FOO_PATH = Services.io.newURI("http://" + base_host + "/foo", null, null);
    const BAR_PATH = Services.io.newURI("http://" + base_host + "/bar", null, null);
    const ALL_SUBDOMAINS = '.' + base_host;
    const OTHER_SUBDOMAIN = other_subdomain_host;

    // Initialize the set of cookies with a mix of non-session cookies with no path,
    // and session cookies with explicit paths. Any subsequent cookies added will cause
    // existing cookies to be evicted.
    setCookie("non_session_non_path_non_domain", null, null, 10000, BASE_URI);
    setCookie("non_session_non_path_subdomain", ALL_SUBDOMAINS, null, 10000, SUBDOMAIN_URI);
    setCookie("session_non_path_pub_domain", OTHER_SUBDOMAIN, null, null, OTHER_SUBDOMAIN_URI);
    setCookie("session_foo_path", null, "/foo", null, FOO_PATH);
    setCookie("session_bar_path", null, "/bar", null, BAR_PATH);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'session_non_path_pub_domain',
                   'session_foo_path',
                   'session_bar_path'], BASE_URI);

    // Ensure that cookies set for the / path appear more recent.
    cs.getCookieString(OTHER_SUBDOMAIN_URI, null)
    verifyCookies(['non_session_non_path_non_domain',
                   'session_foo_path',
                   'session_bar_path',
                   'non_session_non_path_subdomain',
                   'session_non_path_pub_domain'], BASE_URI);

    // Evict oldest session cookie that does not match example.org/foo (session_bar_path)
    setCookie("session_foo_path_2", null, "/foo", null, FOO_PATH);
    verifyCookies(['non_session_non_path_non_domain',
                   'session_foo_path',
                   'non_session_non_path_subdomain',
                   'session_non_path_pub_domain',
                   'session_foo_path_2'], BASE_URI);

    // Evict oldest session cookie that does not match example.org/bar (session_foo_path)
    setCookie("session_bar_path_2", null, "/bar", null, BAR_PATH);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'session_non_path_pub_domain',
                   'session_foo_path_2',
                   'session_bar_path_2'], BASE_URI);

    // Evict oldest session cookie that does not match example.org/ (session_non_path_pub_domain)
    setCookie("non_session_non_path_non_domain_2", null, null, 10000, BASE_URI);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'session_foo_path_2',
                   'session_bar_path_2',
                   'non_session_non_path_non_domain_2'], BASE_URI);

    // Evict oldest session cookie that does not match example.org/ (session_foo_path_2)
    setCookie("session_non_path_non_domain_3", null, null, null, BASE_URI);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'session_bar_path_2',
                   'non_session_non_path_non_domain_2',
                   'session_non_path_non_domain_3'], BASE_URI);

    // Evict oldest session cookie; all such cookies match example.org/bar (session_bar_path_2)
    setCookie("non_session_non_path_non_domain_3", null, null, 10000, BAR_PATH);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'non_session_non_path_non_domain_2',
                   'session_non_path_non_domain_3',
                   'non_session_non_path_non_domain_3'], BASE_URI);

    // Evict oldest session cookie, even though it matches pub.example.org (session_non_path_non_domain_3)
    setCookie("non_session_non_path_pub_domain", null, null, 10000, OTHER_SUBDOMAIN_URI);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'non_session_non_path_non_domain_2',
                   'non_session_non_path_non_domain_3',
                   'non_session_non_path_pub_domain'], BASE_URI);

    // All session cookies have been evicted.
    // Evict oldest non-session non-domain-matching cookie (non_session_non_path_pub_domain)
    setCookie("non_session_bar_path_non_domain", null, '/bar', 10000, BAR_PATH);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'non_session_non_path_non_domain_2',
                   'non_session_non_path_non_domain_3',
                   'non_session_bar_path_non_domain'], BASE_URI);

    // Evict oldest non-session non-path-matching cookie (non_session_bar_path_non_domain)
    setCookie("non_session_non_path_non_domain_4", null, null, 10000, BASE_URI);
    verifyCookies(['non_session_non_path_non_domain',
                   'non_session_non_path_subdomain',
                   'non_session_non_path_non_domain_2',
                   'non_session_non_path_non_domain_3',
                   'non_session_non_path_non_domain_4'], BASE_URI);

    // At this point all remaining cookies are non-session cookies, have a path of /,
    // and either don't have a domain or have one that matches subdomains.
    // They will therefore be evicted from oldest to newest if all new cookies added share
    // similar characteristics.
}

// Verify that the given cookie names exist, and are ordered from least to most recently accessed
function verifyCookies(names, uri) {
    do_check_eq(cm.countCookiesFromHost(uri.host), names.length);
    let cookies = cm.getCookiesFromHost(uri.host, {});
    let actual_cookies = [];
    while (cookies.hasMoreElements()) {
        let cookie = cookies.getNext().QueryInterface(Ci.nsICookie2);
        actual_cookies.push(cookie);
    }
    if (names.length != actual_cookies.length) {
        let left = names.filter(function(n) {
            return actual_cookies.findIndex(function(c) {
                return c.name == n;
            }) == -1;
        });
        let right = actual_cookies.filter(function(c) {
            return names.findIndex(function(n) {
                return c.name == n;
            }) == -1;
        }).map(function(c) { return c.name });
        if (left.length) {
            do_print("unexpected cookies: " + left);
        }
        if (right.length) {
            do_print("expected cookies: " + right);
        }
    }
    do_check_eq(names.length, actual_cookies.length);
    actual_cookies.sort(function(a, b) {
        if (a.lastAccessed < b.lastAccessed)
            return -1;
        if (a.lastAccessed > b.lastAccessed)
            return 1;
        return 0;
    });
    for (var i = 0; i < names.length; i++) {
        do_check_eq(names[i], actual_cookies[i].name);
        do_check_eq(names[i].startsWith('session'), actual_cookies[i].isSession);
    }
}

var lastValue = 0
function setCookie(name, domain, path, maxAge, url) {
    let value = name + "=" + ++lastValue;
    var s = 'setting cookie ' + value;
    if (domain) {
        value += "; Domain=" + domain;
        s += ' (d=' + domain + ')';
    }
    if (path) {
        value += "; Path=" + path;
        s += ' (p=' + path + ')';
    }
    if (maxAge) {
        value += "; Max-Age=" + maxAge;
        s += ' (non-session)';
    } else {
        s += ' (session)';
    }
    s += ' for ' + url.spec;
    do_print(s);
    cs.setCookieStringFromHttp(url, null, null, value, null, null);
}
