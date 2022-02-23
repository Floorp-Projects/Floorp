/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.ssl_tokens_cache_enabled");
  http3_clear_prefs();
});

add_task(async function setup() {
  await http3_setup_tests("h3-29");
});

let Http3Listener = function(closure, expected_priority, context) {
  this._closure = closure;
  this._expected_priority = expected_priority;
  this._context = context;
};

Http3Listener.prototype = {
  resumed: false,

  onStartRequest: function testOnStartRequest(request) {
    Assert.equal(request.status, Cr.NS_OK);
    Assert.equal(request.responseStatus, 200);

    let secinfo = request.securityInfo;
    secinfo.QueryInterface(Ci.nsITransportSecurityInfo);
    Assert.equal(secinfo.resumed, this.resumed);
    Assert.ok(secinfo.serverCert != null);

    let priority = null;
    try {
      priority = request.getResponseHeader("priority-mirror");
    } catch (e) {}
    Assert.equal(priority, this._expected_priority, this._context);
  },

  onDataAvailable: function testOnDataAvailable(request, stream, off, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function testOnStopRequest(request, status) {
    let httpVersion = "";
    try {
      httpVersion = request.protocolVersion;
    } catch (e) {}
    Assert.equal(httpVersion, "h3-29");

    try {
      this._closure();
    } catch (ex) {
      do_throw("Error in closure function: " + ex);
    }
  },
};

function make_channel(url) {
  var request = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  });
  request.QueryInterface(Ci.nsIHttpChannel);
  return request;
}

async function test_flag_priority(context, flag, expected_priority) {
  var chan = make_channel("https://foo.example.com/priority_mirror");
  var cos = chan.QueryInterface(Ci.nsIClassOfService);

  if (flag != null) {
    cos.addClassFlags(flag);
  }
  await new Promise(resolve =>
    chan.asyncOpen(new Http3Listener(resolve, expected_priority, context))
  );
}

add_task(async function test_http3_prio() {
  Services.prefs.setBoolPref("network.http.http3.priorization", false);
  await test_flag_priority("disabled (none)", null, null);
  await test_flag_priority(
    "disabled (urgent_start)",
    Ci.nsIClassOfService.UrgentStart,
    null
  );
  await test_flag_priority(
    "disabled (leader)",
    Ci.nsIClassOfService.Leader,
    null
  );
  await test_flag_priority(
    "disabled (unblocked)",
    Ci.nsIClassOfService.Unblocked,
    null
  );
  await test_flag_priority(
    "disabled (follower)",
    Ci.nsIClassOfService.Follower,
    null
  );
  await test_flag_priority(
    "disabled (speculative)",
    Ci.nsIClassOfService.Speculative,
    null
  );
  await test_flag_priority(
    "disabled (background)",
    Ci.nsIClassOfService.Background,
    null
  );
  await test_flag_priority(
    "disabled (background)",
    Ci.nsIClassOfService.Tail,
    null
  );
});

add_task(async function test_http3_prio_enabled() {
  Services.prefs.setBoolPref("network.http.http3.priorization", true);
  await test_flag_priority("enabled (none)", null, "u=4");
  await test_flag_priority(
    "enabled (urgent_start)",
    Ci.nsIClassOfService.UrgentStart,
    "u=1"
  );
  await test_flag_priority(
    "enabled (leader)",
    Ci.nsIClassOfService.Leader,
    "u=2"
  );
  await test_flag_priority(
    "enabled (unblocked)",
    Ci.nsIClassOfService.Unblocked,
    null
  );
  await test_flag_priority(
    "enabled (follower)",
    Ci.nsIClassOfService.Follower,
    "u=4"
  );
  await test_flag_priority(
    "enabled (speculative)",
    Ci.nsIClassOfService.Speculative,
    "u=6"
  );
  await test_flag_priority(
    "enabled (background)",
    Ci.nsIClassOfService.Background,
    "u=6"
  );
  await test_flag_priority(
    "enabled (background)",
    Ci.nsIClassOfService.Tail,
    "u=6"
  );
});
