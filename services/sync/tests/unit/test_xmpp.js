var Cu = Components.utils;

Cu.import( "resource://weave/xmpp/xmppClient.js" );

var serverUrl = "http://127.0.0.1:5280/http-poll";
var jabberName = "alice";
var jabberDomain = "jonathan-dicarlos-macbook-pro.local";
var jabberPassword = "iamalice";

function run_test() {
  /* First, just see if we can connect: */
  var transport = new HTTPPollingTransport( serverUrl,
					    false,
					    10000 );
  var auth = new PlainAuthenticator();
  var client = new XmppClient( jabberName, jabberDomain, jabberPassword,
			       transport, auth );
		
  client.connect( jabberDomain );
  client.waitForConnection();
  do_check_neq( client._connectionStatus, client.FAILED );
  if ( client._connectionStatus != client.FAILED ) {
    client.disconnect();
  };
};
