ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

// needs to be rooted
var cacheFlushObserver = { observe: function() {
  cacheFlushObserver = null;
  do_send_remote_message('flushed');
}};

// We get this from the child a bit later
var URL = null;

// needs to be rooted
var cacheFlushObserver2 = { observe: function() {
  cacheFlushObserver2 = null;
  openAltChannel();
}};

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
  cc.preferAlternativeDataType("text/binary");
  chan.asyncOpen2(new ChannelListener(readTextData, null));
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
    var os = cc.openAlternativeOutputStream("text/parent-binary", altContent.length);
    os.write(altContent, altContent.length);
    os.close();

    executeSoon(() => {
      Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver2);
    });
  });
}

function openAltChannel() {
  var chan = make_channel(URL);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType("text/parent-binary");
  chan.asyncOpen2(new ChannelListener(readAltData, null));
}

function readAltData(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  // This was generated in the parent, so it's OK to get it.
  Assert.equal(buffer, "altContentParentGenerated");
  Assert.equal(cc.alternativeDataType, "text/parent-binary");

  // FINISH
  do_send_remote_message('finish');
}
