"use strict";

/* global NetUtil, ChannelListener, CL_ALLOW_UNKNOWN_CL */

add_task(async function check_proxy() {
  do_send_remote_message("start-test");
  let URL = await do_await_remote_message("start-test-done");
  let chan = NetUtil.newChannel({
    uri: URL,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);

  let { req, buff } = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  equal(buff, "content");
  equal(req.QueryInterface(Ci.nsIHttpChannelInternal).isProxyUsed, true);
});

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req, buff) => resolve({ req, buff }), null, flags)
    );
  });
}
