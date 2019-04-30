const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = new HttpServer();
var index = 0;
var tests = [
    // Initial request. Cached variant will have no cookie
    { url : "/bug468426", server : "0", expected : "0", cookie: null},

    // Cache now contains a variant with no value for cookie. If we don't
    // set cookie we expect to receive the cached variant
    { url : "/bug468426", server : "1", expected : "0", cookie: null},

    // Cache still contains a variant with no value for cookie. If we
    // set a value for cookie we expect a fresh value
    { url : "/bug468426", server : "2", expected : "2", cookie: "c=2"},

    // Cache now contains a variant with cookie "c=2". If the request
    // also set cookie "c=2", we expect to receive the cached variant.
    { url : "/bug468426", server : "3", expected : "2", cookie: "c=2"},

    // Cache still contains a variant with cookie "c=2". When setting
    // cookie "c=4" in the request we expect a fresh value
    { url : "/bug468426", server : "4", expected : "4", cookie: "c=4"},

    // Cache now contains a variant with cookie "c=4". When setting
    // cookie "c=4" in the request we expect the cached variant
    { url : "/bug468426", server : "5", expected : "4", cookie: "c=4"},
    
    // Cache still contains a variant with cookie "c=4". When setting
    // no cookie in the request we expect a fresh value
    { url : "/bug468426", server : "6", expected : "6", cookie: null},

];

function setupChannel(suffix, value, cookie) {
    var chan = NetUtil.newChannel({
        uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
        loadUsingSystemPrincipal: true
    })
    var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
    httpChan.requestMethod = "GET";
    httpChan.setRequestHeader("x-request", value, false);
    if (cookie != null)
        httpChan.setRequestHeader("Cookie", cookie, false);
    return httpChan;
}

function triggerNextTest() {
    var channel = setupChannel(tests[index].url, tests[index].server, tests[index].cookie);
    channel.asyncOpen(new ChannelListener(checkValueAndTrigger, null));
}

function checkValueAndTrigger(request, data, ctx) {
    Assert.equal(tests[index].expected, data);

    if (index < tests.length - 1) {
        index++;
        // This call happens in onStopRequest from the channel. Opening a new
        // channel to the same url here is no good idea!  Post it instead...
        do_timeout(1, triggerNextTest);
    } else {
        httpserver.stop(do_test_finished);
    }
}

function run_test() {
    httpserver.registerPathHandler("/bug468426", handler);
    httpserver.start(-1);

    // Clear cache and trigger the first test
    evict_cache_entries();
    triggerNextTest();

    do_test_pending();
}

function handler(metadata, response) {
    var body = "unset";
    try {
        body = metadata.getHeader("x-request");
    } catch(e) { }
    response.setStatusLine(metadata.httpVersion, 200, "Ok");
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Last-Modified", getDateString(-1), false);
    response.setHeader("Vary", "Cookie", false);
    response.bodyOutputStream.write(body, body.length);
}

function getDateString(yearDelta) {
    var months = [ 'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug',
            'Sep', 'Oct', 'Nov', 'Dec' ];
    var days = [ 'Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat' ];

    var d = new Date();
    return days[d.getUTCDay()] + ", " + d.getUTCDate() + " "
            + months[d.getUTCMonth()] + " " + (d.getUTCFullYear() + yearDelta)
            + " " + d.getUTCHours() + ":" + d.getUTCMinutes() + ":"
            + d.getUTCSeconds() + " UTC";
}
