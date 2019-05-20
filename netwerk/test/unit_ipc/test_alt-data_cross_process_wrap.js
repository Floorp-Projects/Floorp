const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

// needs to be rooted
var cacheFlushObserver = { observe: function() {
  cacheFlushObserver = null;
  do_send_remote_message('flushed');
}};

// We get this from the child a bit later
var URL = null;

function run_test() {
  do_get_profile();
  do_await_remote_message('flush').then(() => {
    Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver);
  });

  do_await_remote_message('done').then(() => { sendCommand("URL;", load_channel); });

  run_test_in_child("../unit/test_alt-data_cross_process.js");
}

function load_channel(url) {
  ok(url);
  URL = url; // save this to open the alt data channel later
  var chan = make_channel(url);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType("text/binary", "", true);
  chan.asyncOpen(new ChannelListener(readTextData, null));
}

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

function readTextData(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);
  // Since we are in a different process from what that generated the alt-data,
  // we should receive the original data, not processed content.
  Assert.equal(cc.alternativeDataType, "");
  Assert.equal(buffer, "response body");

  // Now let's generate some alt-data in the parent, and make sure we can get it
  var altContent = "altContentParentGenerated";
  executeSoon(() => {
    Assert.throws(() => cc.openAlternativeOutputStream("text/parent-binary", altContent.length), /NS_ERROR_NOT_AVAILABLE/);
    do_send_remote_message('finish');
  });
}
