Cu.import( "resource://weave/xmpp/xmppClient.js" );

function LOG(aMsg) {
  dump("TEST_XMPP: " + aMsg + "\n");
}

var serverUrl = "http://localhost:5280/http-poll";
var jabberDomain = "localhost";

var timer = Cc["@mozilla.org/timer;1"].createInstance( Ci.nsITimer );
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService();

function run_test() {
  // FIXME: this test hangs when you don't have a server, disabling for now
  return;

  /* First, just see if we can connect: */
  var transport = new HTTPPollingTransport(serverUrl, false, 4000);
  var auth = new PlainAuthenticator();
  var alice = new XmppClient("alice", jabberDomain, "iamalice",
                             transport, auth);

  // test connection
  LOG("connecting");
  alice.connect( jabberDomain );
  alice.waitForConnection();
  do_check_eq( alice._connectionStatus, alice.CONNECTED);
  LOG("connected");

  // test disconnection
  LOG("disconnecting");
  alice.disconnect();
  do_check_eq( alice._connectionStatus, alice.NOT_CONNECTED);
  LOG("disconnected");

  // test re-connection
  LOG("reconnecting");
  alice.connect( jabberDomain );
  alice.waitForConnection();
  LOG("reconnected");
  do_check_eq( alice._connectionStatus, alice.CONNECTED);
  alice.disconnect();

  // test connection failure - bad domain
  alice.connect( "bad domain" );
  alice.waitForConnection();
  do_check_eq( alice._connectionStatus, alice.FAILED );

  /*
  // re-connect and move on
  alice.connect( jabberDomain );
  alice.waitForConnection();
  do_check_eq( alice._connectionStatus, alice.CONNECTED);

  // The talking-to-myself test:
  var testIsOver = false;
  var sometext = "bla bla how you doin bla";
  var transport2 = new HTTPPollingTransport( serverUrl, false, 4000 );
  var auth2 = new PlainAuthenticator();
  var bob = new XmppClient( "bob", jabberDomain, "iambob", transport2, auth2 );

  // Timer that will make the test fail if message is not received after
  // a certain amount of time
  var timerResponder = {
  notify: function( timer ) {
      testIsOver = true;
      do_throw( "Timed out waiting for message." );
    }
  };
  timer.initWithCallback( timerResponder, 20000, timer.TYPE_ONE_SHOT );


  // Handler that listens for the incoming message:
  var aliceMessageHandler = {
  handle: function( msgText, from ) {
      dump( "Alice got a message.\n" );
      do_check_eq( msgText, sometext );
      do_check_eq( from, "bob@" + jabberDomain );
      timer.cancel();
      testIsOver = true;
    }
  };
  alice.registerMessageHandler( aliceMessageHandler );

  // Start both clients
  bob.connect( jabberDomain );
  bob.waitForConnection();
  do_check_neq( bob._connectionStatus, bob.FAILED );
  alice.connect( jabberDomain );
  alice.waitForConnection();
  do_check_neq( alice._connectionStatus, alice.FAILED );

  // Send the message
  bob.sendMessage( "alice@" + jabberDomain, sometext );
  // Wait until either the message is received, or the timeout expires.
  var currentThread = threadManager.currentThread;
  while( !testIsOver ) {
    currentThread.processNextEvent( true );
  }

  alice.disconnect();
  bob.disconnect();
  */
};
