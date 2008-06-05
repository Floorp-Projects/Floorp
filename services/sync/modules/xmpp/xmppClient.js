const EXPORTED_SYMBOLS = ['XmppClient', 'HTTPPollingTransport', 'PlainAuthenticator', 'Md5DigestAuthenticator'];

// See www.xulplanet.com/tutorials/mozsdk/sockets.php
// http://www.xmpp.org/specs/rfc3920.html
// http://www.process-one.net/docs/ejabberd/guide_en.html
// http://www.xulplanet.com/tutorials/mozsdk/xmlparse.php
// http://developer.mozilla.org/en/docs/xpcshell
// http://developer.mozilla.org/en/docs/Writing_xpcshell-based_unit_tests

// IM level protocol stuff: presence announcements, conversations, etc.
// ftp://ftp.isi.edu/in-notes/rfc3921.txt

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

function LOG(aMsg) {
  dump("Weave::XMPPClient: " + aMsg + "\n");
}

Cu.import("resource://weave/xmpp/transportLayer.js");
Cu.import("resource://weave/xmpp/authenticationLayer.js");

function XmppClient( clientName, realm, clientPassword, transport, authenticator ) {
  this._init( clientName, realm, clientPassword, transport, authenticator );
}
XmppClient.prototype = {
  //connection status codes:
  NOT_CONNECTED: 0,
  CALLED_SERVER: 1,
  AUTHENTICATING: 2,
  CONNECTED: 3,
  FAILED: -1,

  // IQ stanza status codes:
  IQ_WAIT: 0,
  IQ_OK: 1,
  IQ_ERROR: -1,

  _init: function( clientName, realm, clientPassword, transport, authenticator ) {
    this._myName = clientName;
    this._realm = realm;
    this._fullName = clientName + "@" + realm;
    this._myPassword = clientPassword;
    this._connectionStatus = this.NOT_CONNECTED;
    this._error = null;
    this._streamOpen = false;
    this._transportLayer = transport;
    this._authenticationLayer = authenticator;
    LOG("initialized auth with clientName=" + clientName + ", realm=" + realm + ", pw=" + clientPassword);
    this._authenticationLayer.initialize( clientName, realm, clientPassword );
    this._messageHandlers = [];
    this._iqResponders = [];
    this._nextIqId = 0;
    this._pendingIqs = {};
  },

  __parser: null,
  get _parser() {
    if (!this.__parser)
      this.__parser = Cc["@mozilla.org/xmlextras/domparser;1"].createInstance( Ci.nsIDOMParser );
    return this.__parser;
  },

  __threadManager: null,
  get _threadManager() {
    if (!this.__threadManager)
      this.__threadManager = Cc["@mozilla.org/thread-manager;1"].getService();
    return this.__threadManager;
  },

  parseError: function( streamErrorNode ) {
    LOG( "Uh-oh, there was an error!" );
    var error = streamErrorNode.childNodes[0];
    LOG( "Name: " + error.nodeName + " Value: " + error.nodeValue );
    this._error = error.nodeName;
    this.disconnect();
    /* Note there can be an optional <text>bla bla </text> node inside
       stream: error giving additional info; there can also optionally
       be an app-specific condition element qualified by an app-defined
       namespace */
  },

  setError: function( errorText ) {
    LOG( "Error: " + errorText );
    this._error = errorText;
    this._connectionStatus = this.FAILED;
  },

  onIncomingData: function( messageText ) {
    LOG("onIncomingData(): rcvd: " + messageText);
    var responseDOM = this._parser.parseFromString( messageText, "text/xml" );
    
    if (responseDOM.documentElement.nodeName == "parsererror" ) {
      // handle server disconnection
      if (messageText.match("^</stream:stream>$")) {
        this._handleServerDisconnection();
        return;
      }

      /*
      Before giving up, remember that XMPP doesn't close the top-level
      <stream:stream> element until the communication is done; this means
      that what we get from the server is often technically only an
      xml fragment.  Try manually appending the closing tag to simulate
      a complete xml document and then parsing that. */

      var response = messageText + this._makeClosingXml();
      responseDOM = this._parser.parseFromString( response, "text/xml" );
    }

    if ( responseDOM.documentElement.nodeName == "parsererror" ) {
      /* If that still doesn't work, it might be that we're getting a fragment
      with multiple top-level tags, which is a no-no.  Try wrapping it
      all inside one proper top-level stream element and parsing. */
      response = this._makeHeaderXml( this._fullName ) + messageText + this._makeClosingXml();
      responseDOM = this._parser.parseFromString( response, "text/xml" );
    }

    if ( responseDOM.documentElement.nodeName == "parsererror" ) {
      /* Still can't parse it, give up. */
      this.setError( "Can't parse incoming XML." );
      return;
    }

    var rootElem = responseDOM.documentElement;

    if ( this._connectionStatus == this.CALLED_SERVER ) {
      // skip TLS, go straight to SALS. (encryption should be negotiated
      // at the HTTP layer, i.e. use HTTPS)

      //dispatch whatever the next stage of the connection protocol is.
      response = this._authenticationLayer.generateResponse( rootElem );
      if ( response == false ) {
        this.setError( this._authenticationLayer.getError() );
      } else if ( response == this._authenticationLayer.COMPLETION_CODE ){
        this._connectionStatus = this.CONNECTED;
        LOG( "We be connected!!" );
      } else {
        this._transportLayer.send( response );
      }
      return;
    }

    if ( this._connectionStatus == this.CONNECTED ) {
      /* Check incoming xml to see if it contains errors, presence info,
      or a message: */
      var errors = rootElem.getElementsByTagName( "stream:error" );
      if ( errors.length > 0 ) {
        this.setError( errors[0].firstChild.nodeName );
        return;
      }
      var presences = rootElem.getElementsByTagName( "presence" );
      if (presences.length > 0 ) {
        var from = presences[0].getAttribute( "from" );
        if ( from != undefined ) {
          LOG( "I see that " + from + " is online." );
        }
      }
      if ( rootElem.nodeName == "message" ) {
        this.processIncomingMessage( rootElem );
      } else {
        var messages = rootElem.getElementsByTagName( "message" );
        if (messages.length > 0 ) {
          for ( var message in messages ) {
            this.processIncomingMessage( messages[ message ] );
          }
        }
      }
      if ( rootElem.nodeName == "iq" ) {
        this.processIncomingIq( rootElem );
      } else {
        var iqs = rootElem.getElementsByTagName( "iq" );
        if ( iqs.length > 0 ) {
          for ( var iq in iqs ) {
            this.processIncomingIq( iqs[ iq ] );
          }
        }
      }
    }
  },

  processIncomingMessage: function( messageElem ) {
    LOG( "in processIncomingMessage: messageElem is a " + messageElem );
    var from = messageElem.getAttribute( "from" );
    var contentElem = messageElem.firstChild;
    // Go down till we find the element with nodeType = 3 (TEXT_NODE)
    while ( contentElem.nodeType != 3 ) {
      contentElem = contentElem.firstChild;
    }
    LOG( "Incoming message to you from " + from + ":" + contentElem.nodeValue );
    for ( var x in this._messageHandlers ) {
      // TODO do messages have standard place for metadata?
      // will want to have handlers that trigger only on certain metadata.
      this._messageHandlers[x].handle( contentElem.nodeValue, from );
    }
  },

  processIncomingIq: function( iqElem ) {
    /* This processes both kinds of incoming IQ stanzas --
       ones that are new (initated by another jabber client) and those that
       are responses to ones we sent out previously.  We can tell the
       difference by the type attribute. */
    var buddy = iqElem.getAttribute( "from " );
    var id = iqElem.getAttribute( id );

    switch( iqElem.getAttribute( "type" ) ) {
    case "get":
      /* Someone is asking us for the value of a variable.
      Delegate this to the registered iqResponder; package the answer
      up in an IQ stanza of the same ID and send it back to the asker. */
      var variable = iqElem.firstChild.firstChild.getAttribute( "var" );
      // TODO what to do here if there's more than one registered
      // iqResponder?
      var value = this._iqResponders[0].get( variable );
      var query = "<query><getresult value='" + value + "'/></query>";
      var xml = _makeIqXml( this._fullName, buddy, "result", id, query );
      this._transportLayer.send( xml );
    break;
    case "set":
      /* Someone is telling us to set the value of a variable.
      Delegate this to the registered iqResponder; we can reply
      either with an empty iq type="result" stanza, or else an 
      iq type="error" stanza */
      var variable = iqElem.firstChild.firstChild.getAttribute( "var" );
      var newValue = iqElem.firstChild.firstChildgetAttribute( "value" );
      // TODO what happens when there's more than one reigistered
      // responder?
      // TODO give the responder a chance to say "no" and give an error.
      this._iqResponders[0].set( variable, value );
      var xml = _makeIqXml( this._fullName, buddy, "result", id, "<query/>" );
      this._transportLayer.send( xml );
    break;
    case "result":
      /* If all is right with the universe, then the id of this iq stanza
      corresponds to a set or get stanza that we sent out, so it should
      be in our pending dictionary.
      */
      if ( this._pendingIqs[ id ] == undefined ) {
        this.setError( "Unexpected IQ reply id" + id );
        return;
      }
      /* The result stanza may have a query with a value in it, in
      which case this is the value of the variable we requested.
      If there's no value, it was probably a set query, and should
      just be considred a success. */
      var newValue = iqElem.firstChild.firstChild.getAttribute( "value" );
      if ( newValue != undefined ) {
        this._pendingIqs[ id ].value = newValue;
      } else {
        this._pendingIqs[ id ].value = true;
      }
      this._pendingIqs[ id ].status = this.IQ_OK;
      break;
    case "error":
      /* Dig down through the element tree till we find the one with
      the error text... */
      var elems = iqElem.getElementsByTagName( "error" );
      var errorNode = elems[0].firstChild;
      if ( errorNode.nodeValue != null ) {
        this.setError( errorNode.nodeValue );
      } else {
        this.setError( errorNode.nodeName );
      }
      if ( this._pendingIqs[ id ] != undefined ) {
        this._pendingIqs[ id ].status = this.IQ_ERROR;
      }
      break;
    }
  },

  registerMessageHandler: function( handlerObject ) {
    /* messageHandler object must have 
       handle( messageText, from ) method.
     */
    this._messageHandlers.push( handlerObject );
  },

  registerIQResponder: function( handlerObject ) {
    /* IQResponder object must have 
       .get( variable ) and
       .set( variable, newvalue ) methods. */
    this._iqResponders.push( handlerObject );
  },

  onTransportError: function( errorText ) {
    this.setError( errorText );
  },

  connect: function( host ) {
    // Do the handshake to connect with the server and authenticate.
    this._transportLayer.connect();
    this._transportLayer.setCallbackObject( this );
    this._transportLayer.send( this._makeHeaderXml( host ) );    

    this._connectionStatus = this.CALLED_SERVER;
    // Now we wait... the rest of the protocol will be driven by
    // onIncomingData.
  },

  _makeHeaderXml: function( recipient ) {
    return "<?xml version='1.0'?><stream:stream to='" +
           recipient +
           "' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>";
  },

  _makeMessageXml: function( messageText, fullName, recipient ) {
    /* a "message stanza".  Note the message element must have the
    full namespace info or it will be rejected. */
    var msgXml = "<message xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' from='" +
                 fullName + "' to='" + recipient + "' xml:lang='en'><body>" +
                 messageText + "</body></message>";
    LOG( "Message xml: " );
    LOG( msgXml );
    return msgXml;
  },

  _makePresenceXml: function( fullName ) {
    // a "presence stanza", sent to announce my presence to the server;
    // the server is supposed to multiplex this to anyone subscribed to
    // presence notifications.
    return "<presence from ='" + fullName + "'><show/></presence>";
  },

  _makeIqXml: function( fullName, recipient, type, id, query ) {
    /* an "iq (info/query) stanza".  This can be used for structured data
    exchange:  I send an <iq type='get' id='1'> containing a query,
    and get back an <iq type='result' id='1'> containing the answer to my
    query.  I can also send an <iq type='set' id='2'> to set a value
    remotely.  The recipient answers with either <iq type='result'> or 
    <iq type='error'>, with an id matching the id of my set or get. */

    //Useful!!
    return "<iq xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' from='" + fullName + "' to='" + recipient + "' type='" + type + "' id='" + id + "'>" + query + "</iq>";
  },

  _makeClosingXml: function () {
    return "</stream:stream>";
  },

  _generateIqId: function() {
    // Each time this is called, it returns an ID that has not
    // previously been used this session.
    var id = "client_" + this._nextIqId;
    this._nextIqId = this._nextIqId + 1;
    return id;
  },

  _sendIq: function( recipient, query, type ) {
    var id = this._generateIqId();
    this._pendingIqs[ id ] = { status: this.IQ_WAIT };
    this._transportLayer.send( this._makeIqXml( this._fullName,
            recipient,
            type,
            id,
            query ) );
    /* And then wait for a response with the same ID to come back...
       When we get a reply, the pendingIq dictionary entry will have
       its status set to IQ_OK or IQ_ERROR and, if it's IQ_OK and
       this was a query that's supposed to return a value, the value
       will be in the value field of the entry. */
    var thread = this._threadManager.currentThread;
    while( this._pendingIqs[ id ].status == this.IQ_WAIT ) {
      thread.processNextEvent( true );
    }
    if ( this._pendingIqs[ id ].status == this.IQ_OK ) {
      return this._pendingIqs[ id ].value;
    } else if ( this._pendingIqs[ id ].status == this.IQ_ERROR ) {
      return false;
    }
    // Can't happen?
  },

  iqGet: function( recipient, variable ) {
    var query = "<query><getvar var='" + variable + "'/></query>";
    return this._sendIq( recipient, query, "get" );
  },

  iqSet: function( recipient, variable, value ) {
    var query = "<query><setvar var='" + variable + "' value='" + value + "'/></query>";
    return this._sendIq( recipient, query, "set" );
  },

  sendMessage: function( recipient, messageText ) {
    // OK so now I'm doing that part, but what am I supposed to do with the
    // new JID that I'm bound to??
    var body = this._makeMessageXml( messageText, this._fullName, recipient );
    this._transportLayer.send( body );
  },

  announcePresence: function() {
    this._transportLayer.send( this._makePresenceXml(this._myName) );
  },

  subscribeForPresence: function( buddyId ) {
    // OK, there are 'subscriptions' and also 'rosters'...?
    //this._transportLayer.send( "<presence to='" + buddyId + "' type='subscribe'/>" );
    // TODO
    // other side must then approve this by sending back a presence to
    // me with type ='subscribed'.
  },

  disconnect: function() {
    // todo: only send closing xml if the stream has not already been
    // closed (if there was an error, the server will have closed the stream.)
    this._transportLayer.send( this._makeClosingXml() );

    this.waitForDisconnect();
  },

  _handleServerDisconnection: function() {
    this._transportLayer.disconnect();
    this._connectionStatus = this.NOT_CONNECTED;
  },

  waitForConnection: function( ) {
    var thread = this._threadManager.currentThread;
    while ( this._connectionStatus != this.CONNECTED &&
      this._connectionStatus != this.FAILED ) {
      thread.processNextEvent( true );
    }
  },

  waitForDisconnect: function() {
    var thread = this._threadManager.currentThread;
    while ( this._connectionStatus == this.CONNECTED ) {
      thread.processNextEvent( true );
    }
  }
};
