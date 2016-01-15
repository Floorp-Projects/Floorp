//
// Tests if a response with an Expires-header in the past
// and Cache-Control: max-age  in the future works as
// specified in RFC 2616 section 14.9.3 by letting max-age
// take precedence

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
const BUGID = "203271";

var httpserver = new HttpServer();
var index = 0;
var tests = [
    // original problem described in bug#203271
    {url: "/precedence", server: "0", expected: "0",
     responseheader: [ "Expires: " + getDateString(-1),
                       "Cache-Control: max-age=3600"]},

    {url: "/precedence?0", server: "0", expected: "0",
     responseheader: [ "Cache-Control: max-age=3600",
                       "Expires: " + getDateString(-1)]},

    // max-age=1s, expires=1 year from now
    {url: "/precedence?1", server: "0", expected: "0",
     responseheader: [ "Expires: " + getDateString(1),
                       "Cache-Control: max-age=1"]},

    // expires=now
    {url: "/precedence?2", server: "0", expected: "0",
     responseheader: [ "Expires: " + getDateString(0)]},

    // max-age=1s
    {url: "/precedence?3", server: "0", expected: "0",
     responseheader: ["Cache-Control: max-age=1"]},

    // The test below is the example from
    //
    //         https://bugzilla.mozilla.org/show_bug.cgi?id=203271#c27
    //
    //  max-age=2592000s (1 month), expires=1 year from now, date=1 year ago
    {url: "/precedence?4", server: "0", expected: "0",
     responseheader: [ "Cache-Control: private, max-age=2592000",
                       "Expires: " + getDateString(+1)],
     explicitDate: getDateString(-1)},

    // The two tests below are also examples of clocks really out of synch
    // max-age=1s, date=1 year from now
    {url: "/precedence?5", server: "0", expected: "0",
     responseheader: [ "Cache-Control: max-age=1"],
     explicitDate: getDateString(1)},

    // max-age=60s, date=1 year from now
    {url: "/precedence?6", server: "0", expected: "0",
     responseheader: [ "Cache-Control: max-age=60"],
     explicitDate: getDateString(1)},

    // this is just to get a pause of 3s to allow cache-entries to expire
    {url: "/precedence?999", server: "0", expected: "0", delay: "3000"},

    // Below are the cases which actually matters
    {url: "/precedence", server: "1", expected: "0"}, // should be cached
     
    {url: "/precedence?0", server: "1", expected: "0"}, // should be cached

    {url: "/precedence?1", server: "1", expected: "1"}, // should have expired

    {url: "/precedence?2", server: "1", expected: "1"}, // should have expired

    {url: "/precedence?3", server: "1", expected: "1"}, // should have expired

    {url: "/precedence?4", server: "1", expected: "1"}, // should have expired

    {url: "/precedence?5", server: "1", expected: "1"}, // should have expired
    
    {url: "/precedence?6", server: "1", expected: "0"}, // should be cached

];

function logit(i, data, ctx) {
    dump("requested [" + tests[i].server + "] " +
         "got [" + data + "] " +
         "expected [" + tests[i].expected + "]");

    if (tests[i].responseheader)
        dump("\t[" + tests[i].responseheader + "]");
    dump("\n");
    // Dump all response-headers
    dump("\n===================================\n")
    ctx.visitResponseHeaders({
        visitHeader: function(key, val) {
            dump("\t" + key + ":"+val + "\n");
        }}
    );
    dump("===================================\n")
}

function setupChannel(suffix, value) {
    var chan = NetUtil.newChannel({
        uri: "http://localhost:" + httpserver.identity.primaryPort + suffix,
        loadUsingSystemPrincipal: true
    });
    var httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
    httpChan.requestMethod = "GET"; // default value, just being paranoid...
    httpChan.setRequestHeader("x-request", value, false);
    return httpChan;
}

function triggerNextTest() {
    var channel = setupChannel(tests[index].url, tests[index].server);
    channel.asyncOpen2(new ChannelListener(checkValueAndTrigger, channel));
}

function checkValueAndTrigger(request, data, ctx) {
    logit(index, data, ctx);
    do_check_eq(tests[index].expected, data);

    if (index < tests.length - 1) {
        var delay = tests[index++].delay;
        if (delay) {
            do_timeout(delay, triggerNextTest);
        } else {
            triggerNextTest();
        }
    } else {
        httpserver.stop(do_test_finished);
    }
}

function run_test() {
    httpserver.registerPathHandler("/precedence", handler);
    httpserver.start(-1);

    // clear cache
    evict_cache_entries();

    triggerNextTest();
    do_test_pending();
}

function handler(metadata, response) {
    var body = metadata.getHeader("x-request");
    response.setHeader("Content-Type", "text/plain", false);

    var date = tests[index].explicitDate;
    if (date == undefined) {
        response.setHeader("Date", getDateString(0), false);
    } else {
        response.setHeader("Date", date, false);
    }

    var header = tests[index].responseheader;
    if (header == undefined) {
        response.setHeader("Last-Modified", getDateString(-1), false);
    } else {
        for (var i = 0; i < header.length; i++) {
            var splitHdr = header[i].split(": ");
            response.setHeader(splitHdr[0], splitHdr[1], false);
        }
    }
    
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
}
 
function getDateString(yearDelta) {
    var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug',
                  'Sep', 'Oct', 'Nov', 'Dec'];
    var days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

    var d = new Date();
    return days[d.getUTCDay()] + ", " +
            d.getUTCDate() + " " +
            months[d.getUTCMonth()] + " " +
            (d.getUTCFullYear() + yearDelta) + " " +
            d.getUTCHours() + ":" + d.getUTCMinutes() + ":" +
            d.getUTCSeconds() + " UTC";
}
