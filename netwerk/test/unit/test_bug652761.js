// This is just a crashtest for a url that is rejected at parse time (port 80,000)

Cu.import("resource://gre/modules/NetUtil.jsm");

function completeTest(request, data, ctx)
{
    do_test_finished();
}

function run_test()
{
    var chan = NetUtil.newChannel({
      uri: "http://localhost:80000/",
      loadUsingSystemPrincipal: true
    });
    var httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
    httpChan.asyncOpen2(new ChannelListener(completeTest,httpChan, CL_EXPECT_FAILURE));
    do_test_pending();
}

