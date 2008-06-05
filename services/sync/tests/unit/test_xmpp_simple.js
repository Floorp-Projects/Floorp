function LOG(aMsg) {
  dump("TEST_XMPP_SIMPLE: " + aMsg + "\n");
}

Components.utils.import( "resource://weave/xmpp/xmppClient.js" );

var serverUrl = "http://localhost:5280/http-poll";
var jabberDomain = "localhost";

function run_test() {
  // FIXME: this test hangs when you don't have a server, disabling for now
  return;

  // async test
  do_test_pending();

  var testMessage = "Hello Bob.";

  var aliceHandler = {
    handle: function(msgText, from) {
      LOG("ALICE RCVD from " + from + ": " + msgText);
    }
  };
  var aliceClient = getClientForUser("alice", "iamalice", aliceHandler);

  var bobHandler = {
    handle: function(msgText, from) {
      LOG("BOB RCVD from " + from + ": " + msgText);
      do_check_eq(from.split("/")[0], "alice@" + jabberDomain);
      do_check_eq(msgText, testMessage);
      LOG("messages checked out");

      aliceClient.disconnect();
      bobClient.disconnect();
      LOG("disconnected");

      do_test_finished();
    }
  };
  var bobClient = getClientForUser("bob", "iambob", bobHandler);
  bobClient.announcePresence();


  // Send a message
  aliceClient.sendMessage("bob@" + jabberDomain, testMessage);
}

function getClientForUser(aName, aPassword, aHandler) {
  // "false" tells the transport not to use session keys.  4000 is the number of
  // milliseconds to wait between attempts to poll the server.
  var transport = new HTTPPollingTransport(serverUrl, false, 4000);

  var auth = new PlainAuthenticator();

  var client = new XmppClient(aName, jabberDomain, aPassword,
                             transport, auth);

  client.registerMessageHandler(aHandler);

  // Connect
  client.connect(jabberDomain);
  client.waitForConnection();

  // this will block until our connection attempt has either succeeded or failed.
  // Check if connection succeeded:
  if ( client._connectionStatus == client.FAILED ) {
    do_throw("connection failed");
  }

  return client;
}
