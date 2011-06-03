do_load_httpd_js();

var httpserver = new nsHttpServer();
var index = 0;

function setupChannel(url)
{
    var ios = Components.classes["@mozilla.org/network/io-service;1"].
                         getService(Ci.nsIIOService);
    var chan = ios.newChannel("http://localhost:4444" + url, "", null);
    var httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
    return httpChan;
}

function completeTest1(request, data, ctx)
{
    httpserver.stop(do_test_finished);
}

function run_test()
{
    httpserver.registerPathHandler("/2xcl", handler);
    httpserver.start(4444);

    var channel = setupChannel("/2xcl");
    channel.asyncOpen(new ChannelListener(completeTest1,
                                          channel, CL_EXPECT_FAILURE), null);
    do_test_pending();
}

function handler(metadata, response)
{
    var body = "012345678901234567890123456789";
    response.seizePower();
    response.write("HTTP/1.0 200 OK\r\n");
    response.write("Content-Type: text/plain\r\n");
    response.write("Content-Length: 20\r\n");
    response.write("Content-Length: 30\r\n");
    response.write("\r\n");
    response.write(body);
    response.finish();
}

