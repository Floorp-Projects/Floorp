Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

var httpserver = new HttpServer();
httpserver.start(-1);
var cache = null;

var base_url = "http://localhost:" + httpserver.identity.primaryPort;
var resource_age_100 = "/resource_age_100";
var resource_age_100_url = base_url + resource_age_100;
var resource_stale_100 = "/resource_stale_100";
var resource_stale_100_url = base_url + resource_stale_100;
var resource_fresh_100 = "/resource_fresh_100";
var resource_fresh_100_url = base_url + resource_fresh_100;

// Test flags
var hit_server = false;


function make_channel(url, cache_control)
{
  // Reset test global status
  hit_server = false;

  var req = NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
  req.QueryInterface(Ci.nsIHttpChannel);
  if (cache_control) {
    req.setRequestHeader("Cache-control", cache_control, false);
  }

  return req;
}

function make_uri(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  return ios.newURI(url, null, null);
}

function resource_age_100_handler(metadata, response)
{
  hit_server = true;

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Age", "100", false);
  response.setHeader("Last-Modified", date_string_from_now(-100), false);
  response.setHeader("Expires", date_string_from_now(+9999), false);

  const body = "data1";
  response.bodyOutputStream.write(body, body.length);
}

function resource_stale_100_handler(metadata, response)
{
  hit_server = true;

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Date", date_string_from_now(-200), false);
  response.setHeader("Last-Modified", date_string_from_now(-200), false);
  response.setHeader("Cache-Control", "max-age=100", false);
  response.setHeader("Expires", date_string_from_now(-100), false);

  const body = "data2";
  response.bodyOutputStream.write(body, body.length);
}

function resource_fresh_100_handler(metadata, response)
{
  hit_server = true;

  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Last-Modified", date_string_from_now(0), false);
  response.setHeader("Cache-Control", "max-age=100", false);
  response.setHeader("Expires", date_string_from_now(+100), false);

  const body = "data3";
  response.bodyOutputStream.write(body, body.length);
}


function run_test()
{
  do_get_profile();
  do_test_pending();

  httpserver.registerPathHandler(resource_age_100, resource_age_100_handler);
  httpserver.registerPathHandler(resource_stale_100, resource_stale_100_handler);
  httpserver.registerPathHandler(resource_fresh_100, resource_fresh_100_handler);
  cache = getCacheStorage("disk");

  wait_for_cache_index(run_next_test);
}

// Here starts the list of tests

// ============================================================================
// Cache-Control: no-store

add_test(() => {
  // Must not create a cache entry
  var ch = make_channel(resource_age_100_url, "no-store");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_false(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // Prepare state only, cache the entry
  var ch = make_channel(resource_age_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // Check the prepared cache entry is used when no special directives are added
  var ch = make_channel(resource_age_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // Try again, while we already keep a cache entry,
  // the channel must not use it, entry should stay in the cache
  var ch = make_channel(resource_age_100_url, "no-store");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

// ============================================================================
// Cache-Control: no-cache

add_test(() => {
  // Check the prepared cache entry is used when no special directives are added
  var ch = make_channel(resource_age_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // The existing entry should be revalidated (we expect a server hit)
  var ch = make_channel(resource_age_100_url, "no-cache");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

// ============================================================================
// Cache-Control: max-age

add_test(() => {
  // Check the prepared cache entry is used when no special directives are added
  var ch = make_channel(resource_age_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // The existing entry's age is greater than the maximum requested,
  // should hit server
  var ch = make_channel(resource_age_100_url, "max-age=10");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // The existing entry's age is greater than the maximum requested,
  // but the max-stale directive says to use it when it's fresh enough
  var ch = make_channel(resource_age_100_url, "max-age=10, max-stale=99999");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // The existing entry's age is lesser than the maximum requested,
  // should go from cache
  var ch = make_channel(resource_age_100_url, "max-age=1000");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_age_100_url), ""));

    run_next_test();
  }, null), null);
});

// ============================================================================
// Cache-Control: max-stale

add_test(() => {
  // Preprate the entry first
  var ch = make_channel(resource_stale_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_stale_100_url), ""));

    // Must shift the expiration time set on the entry to |now| be in the past
    do_timeout(1500, run_next_test);
  }, null), null);
});

add_test(() => {
  // Check it's not reused (as it's stale) when no special directives
  // are provided
  var ch = make_channel(resource_stale_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_stale_100_url), ""));

    do_timeout(1500, run_next_test);
  }, null), null);
});

add_test(() => {
  // Accept cached responses of any stale time
  var ch = make_channel(resource_stale_100_url, "max-stale");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_stale_100_url), ""));

    do_timeout(1500, run_next_test);
  }, null), null);
});

add_test(() => {
  // The entry is stale only by 100 seconds, accept it
  var ch = make_channel(resource_stale_100_url, "max-stale=1000");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_stale_100_url), ""));

    do_timeout(1500, run_next_test);
  }, null), null);
});

add_test(() => {
  // The entry is stale by 100 seconds but we only accept a 10 seconds stale
  // entry, go from server
  var ch = make_channel(resource_stale_100_url, "max-stale=10");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_stale_100_url), ""));

    run_next_test();
  }, null), null);
});

// ============================================================================
// Cache-Control: min-fresh

add_test(() => {
  // Preprate the entry first
  var ch = make_channel(resource_fresh_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_fresh_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // Check it's reused when no special directives are provided
  var ch = make_channel(resource_fresh_100_url);
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_fresh_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // Entry fresh enough to be served from the cache
  var ch = make_channel(resource_fresh_100_url, "min-fresh=10");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_false(hit_server);
    do_check_true(cache.exists(make_uri(resource_fresh_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  // The entry is not fresh enough
  var ch = make_channel(resource_fresh_100_url, "min-fresh=1000");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_fresh_100_url), ""));

    run_next_test();
  }, null), null);
});

// ============================================================================
// Parser test, if the Cache-Control header would not parse correctly, the entry
// doesn't load from the server.

add_test(() => {
  var ch = make_channel(resource_fresh_100_url, "unknown1,unknown2 = \"a,b\",  min-fresh = 1000 ");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_fresh_100_url), ""));

    run_next_test();
  }, null), null);
});

add_test(() => {
  var ch = make_channel(resource_fresh_100_url, "no-cache = , min-fresh = 10");
  ch.asyncOpen(new ChannelListener(function(request, data) {
    do_check_true(hit_server);
    do_check_true(cache.exists(make_uri(resource_fresh_100_url), ""));

    run_next_test();
  }, null), null);
});

// ============================================================================
// Done

add_test(() => {
  run_next_test();
  httpserver.stop(do_test_finished);
});

// ============================================================================
// Helpers

function date_string_from_now(delta_secs) {
    var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug',
                  'Sep', 'Oct', 'Nov', 'Dec'];
    var days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

    var d = new Date();
    d.setTime(d.getTime() + delta_secs * 1000);
    return days[d.getUTCDay()] + ", " +
           d.getUTCDate() + " " +
           months[d.getUTCMonth()] + " " +
           d.getUTCFullYear() + " " +
           d.getUTCHours() + ":" +
           d.getUTCMinutes() + ":" +
           d.getUTCSeconds() + " UTC";
}
