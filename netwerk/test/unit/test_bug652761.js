// This is just a crashtest for a url that is rejected at parse time (port 80,000)


function run_test()
{
    // Bug 1301621 makes invalid ports throw
    Assert.throws(() => {
        var chan = NetUtil.newChannel({
          uri: "http://localhost:80000/",
          loadUsingSystemPrincipal: true
        });
    }, /NS_ERROR_MALFORMED_URI/, "invalid port");

    do_test_finished();
}
