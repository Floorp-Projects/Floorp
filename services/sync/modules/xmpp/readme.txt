About the XMPP module

Here is sample code demonstrating how client code can use the XMPP module. It assumes that a Jabber server is running on localhost on port 5280.

        Components.utils.import( "resource://weave/xmpp/xmppClient.js" );

        var serverUrl = "http://127.0.0.1:5280/http-poll";
        var jabberDomain = "mylaptop.local";

        var transport = new HTTPPollingTransport( serverUrl,
	                                                                    false,
        								    4000 );
        // "false" tells the transport not to use session keys.  4000 is the number of
        // milliseconds to wait between attempts to poll the server.
        var auth = new PlainAuthenticator();
        var alice = new XmppClient( "alice", jabberDomain, "iamalice",
	                                             transport, auth );
        // This sets up an XMPP client for the jabber ID 
        // "alice@jonathan-dicarlos-macbook-pro.local", who has password
        // "iamalice".

        // Set up callback for incoming messages:
        var aliceMessageHandler = {
            handle: function( msgText, from ) {
 	           // Your code goes here.  It will be called whenever another XMPP client
                   // sends a message to Alice.
                   // msgText is the text of the incoming message, and "from" is the
		   // jabber ID of the sender.
            }
        };
        alice.registerMessageHandler( aliceMessageHandler );

        // Connect
        alice.connect( jabberDomain );
        alice.waitForConnection();
        // this will block until our connection attempt has either succeeded or failed.
        // Check if connection succeeded:
        if ( alice._connectionStatus == alice.FAILED ) {
            // handle error
        }

        // Send a message
        alice.sendMessage( "bob@mylaptop.local", "Hello, Bob." );

	// Disconnect
	alice.disconnect();


Installing an XMPP server to test against:

I'm using ejabberd, which works well and is open source but is implemented in Erlang.  (That's bad, because I don't know how to hack Erlang.  I'm thinking of moving to jabberd, implemented in C.)

ejabberd: http://www.ejabberd.im/
jabberd: http://jabberd.jabberstudio.org/

Installation of ejabberd was simple.  After it's installed, configure it (create an admin account, and enable http-polling) by editing the configuration file:

        ejabberd/conf/ejabberd.cfg

 Its web admin interface can be used to create accounts (so that the "alice" and "bob" accounts assumed by the testing code will actually exist).  This admin interface is at:

        http://localhost:5280/admin/

The ejabberd process is started simply by running:

        ejabberd/bin/ejabberdctl start



Outstanding Issues -- bugs and things to do.


* Occasionally (by which I mean, randomly) the server will respond to
   a request with an HTTP status 0.  Generally things will go back to
   normal with the next request and then keep working, so it's not
   actually preventing anything from working, but I don't understand
   what status 0 means and that makes me uncomfortable as it could be
   hiding other problems.

* Occasionally (randomly) a message is received that has a raw integer
  for a messageElem, which causes XML parsing of the incoming message
  to fail.  I don't see any pattern for the values of the raw
  integers, nor do I understand how we can get an integer in place of
  an XML element when requesting the root node of the incoming XML
  document.

* Duplicate messages.  Occasionally, even though one instance of the
  client sends only a single copy of a <message> stanza, the intended
  target will receive it multiple times.  Sometimes if the recipient
  signs off and then signs back on, it will recieve another copy of
  the message.  This makes me think it's probably a feature of the
  server to resend messages that it thinks the recipient might have
  missed while offline.  Duplicate messages are a problem especially
  when using xmpp to synchronize two processes, for instance.  Do I
  need to tweak the server settings, or change server implementations?
  Do I need to have the client receiving messages acknowledge them to the
  server somehow?
  Do I need to move to using <iq> stanzas for synchronization?  Or
  start putting GUIDs into <message>s and discarding duplicates
  myself?

* Whenever I send an <iq> request, I get a <bad-request/> error back
  from the server.  Documentation for the standard error messages
  indicates that bad-request is supposed to happen when the type of
  the iq element is invalid, i.e. something other than 'set', 'get',
  'result', or 'error'.  But I'm using 'get' or 'set' and still
  getting <bad-request/>.  I don't understand what's happening here.

* The authentication layer currently doesn't work with MD5-digest auth, only plain
   auth.

* The HTTPPolling transport layer gets a "key sequence error" if useKeys is turned on.
   (Everything seems to be working OK with useKeys turned off, but that's less
   secure.)

* Speaking of security, I need to try using HTTP polling transport over an HTTPS
  connection and see if everything works.  If it does, that will be great, because
  we'll have SSL/TLS for free, and it won't matter so much that we're using
  plain auth because the password will be encrypted as part of SSL.

* Need to implement the presence-notification/subscription/"buddy list" stuff
  so that clients can more easily know when other clients are online.


To anyone reading this, I'd appreciate any help in debugging these problems.
Can you duplicate these problems?  Do you have any suggestions of things to try?



For a forum post:
copy the outstanding issues list

Here's where/how I'm trying to install the jabberd server.


For email to the list:
