if(typeof(atob) == 'undefined') {
// This code was written by Tyler Akins and has been placed in the
// public domain.  It would be nice if you left this header intact.
// Base64 code from Tyler Akins -- http://rumkin.com

var keyStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

function btoa(input) {
   var output = "";
   var chr1, chr2, chr3;
   var enc1, enc2, enc3, enc4;
   var i = 0;

   do {
      chr1 = input.charCodeAt(i++);
      chr2 = input.charCodeAt(i++);
      chr3 = input.charCodeAt(i++);

      enc1 = chr1 >> 2;
      enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
      enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
      enc4 = chr3 & 63;

      if (isNaN(chr2)) {
         enc3 = enc4 = 64;
      } else if (isNaN(chr3)) {
         enc4 = 64;
      }

      output = output + keyStr.charAt(enc1) + keyStr.charAt(enc2) + 
         keyStr.charAt(enc3) + keyStr.charAt(enc4);
   } while (i < input.length);
   
   return output;
}

function atob(input) {
   var output = "";
   var chr1, chr2, chr3;
   var enc1, enc2, enc3, enc4;
   var i = 0;

   // remove all characters that are not A-Z, a-z, 0-9, +, /, or =
   input = input.replace(/[^A-Za-z0-9\+\/\=]/g, "");

   do {
      enc1 = keyStr.indexOf(input.charAt(i++));
      enc2 = keyStr.indexOf(input.charAt(i++));
      enc3 = keyStr.indexOf(input.charAt(i++));
      enc4 = keyStr.indexOf(input.charAt(i++));

      chr1 = (enc1 << 2) | (enc2 >> 4);
      chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
      chr3 = ((enc3 & 3) << 6) | enc4;

      output = output + String.fromCharCode(chr1);

      if (enc3 != 64) {
         output = output + String.fromCharCode(chr2);
      }
      if (enc4 != 64) {
         output = output + String.fromCharCode(chr3);
      }
   } while (i < input.length);

   return output;
}
}


/* Two implementations of SASL authentication:
   one using MD5-DIGEST, the other using PLAIN. 


Here's the interface that each implementation must obey:
initialize( clientName, clientRealm, clientPassword );

generateResponse( rootElem );

getError();
 returns text of error message
*/

function BaseAuthenticator() {
}
BaseAuthenticator.prototype = {
 COMPLETION_CODE: "success!",

 initialize: function( userName, realm, password  ) {
    this._name = userName;
    this._realm = realm;
    this._password = password;
    this._stepNumber = 0;
    this._errorMsg = "";
  },

 getError: function () {
    /* Returns text of most recent error message.
       Client code should call this if generateResponse() returns false
       to see what the problem was. */
    return this._errorMsg;
  },

 generateResponse: function( rootElem ) {
    /* Subclasses must override this.  rootElem is a DOM node which is
       the root element of the XML the server has sent to us as part
       of the authentication protocol.  return value: the string that
       should be sent back to the server in response.  'false' if
       there's a failure, or COMPLETION_CODE if nothing else needs to
       be sent because authentication is a success. */

    this._errorMsg = "generateResponse() should be overridden by subclass.";
    return false;
  },
 
 verifyProtocolSupport: function( rootElem, protocolName ) {
    /* Parses the incoming stream from the server to check whether the
       server supports the type of authentication we want to do
       (specified in the protocolName argument).
       Returns false if there is any problem.
     */
    if ( rootElem.nodeName != "stream:stream" ) {
      this._errorMsg = "Expected stream:stream but got " + rootElem.nodeName;
      return false;
    }
      
    dump( "Got response from server...\n" );
    dump( "ID is " + rootElem.getAttribute( "id" ) + "\n" );
    // TODO: Do I need to do anything with this ID value???
    dump( "From: " + rootElem.getAttribute( "from" ) + "\n" );
    if (rootElem.childNodes.length == 0) {
      // No child nodes is unexpected, but allowed by the protocol.
      // this shouldn't be an error.
      this._errorMsg = "Expected child nodes but got none.";
      return false;
    }

    var child = rootElem.childNodes[0];
    if (child.nodeName == "stream:error" ) {
      this._errorMsg = this.parseError( child );
      return false;
    }

    if ( child.nodeName != "stream:features" ) {
      this._errorMsg = "Expected stream:features but got " + child.nodeName;
      return false;
    }
      
    var protocolSupported = false;
    var mechanisms = child.getElementsByTagName( "mechanism" );
    for ( var x = 0; x < mechanisms.length; x++ ) {
      if ( mechanisms[x].firstChild.nodeValue == protocolName ) {
	protocolSupported = true;
      }
    }
      
    if ( !protocolSupported ) {
      this._errorMsg = protocolName + " not supported by server!";
      return false;
    }
    return true;
  }

};

function Md5DigestAuthenticator( ) {
  /* SASL using DIGEST-MD5 authentication.
     Uses complicated hash of password
     with nonce and cnonce to obscure password while preventing replay
     attacks.
     
     See http://www.faqs.org/rfcs/rfc2831.html
     "Using Digest Authentication as a SASL mechanism"

     TODO: currently, this is getting rejected by my server.
     What's wrong?
  */
}
Md5DigestAuthenticator.prototype = {

 _makeCNonce: function( ) {
    return "\"" + Math.floor( 10000000000 * Math.random() ) + "\"";
  },
 
 generateResponse: function Md5__generateResponse( rootElem ) {
    if ( this._stepNumber == 0 ) {

      if ( this.verifyProtocolSupport( rootElem, "DIGEST-MD5" ) == false ) {
	return false;
      }
      // SASL step 1: request that we use DIGEST-MD5 authentication.
      this._stepNumber = 1;
      return "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='DIGEST-MD5'/>";

    } else if ( this._stepNumber == 1 ) {
     
      // proceed to SASL step 2: are you asking for a CHALLENGE?!?      
      var challenge = this._unpackChallenge( rootElem.firstChild.nodeValue );
      dump( "Nonce is " + challenge.nonce + "\n" );
      // eg:
      // nonce="3816627940",qop="auth",charset=utf-8,algorithm=md5-sess

      // Now i have the nonce: make a digest-response out of
      /* username: required
	 realm: only needed if realm is in challenge
	 nonce: required, just as recieved
	 cnonce: required, opaque quoted string, 64 bits entropy
	 nonce-count: optional
	 qop: (quality of protection) optional
	 serv-type: optional?
	 host: optional?
	 serv-name: optional?
	 digest-uri: "service/host/serv-name" (replaces those three?)
	 response: required (32 lowercase hex),
	 maxbuf: optional,
	 charset,
	 LHEX (32 hex digits = ??),
	 cipher: required if auth-conf is negotiatedd??
	 authzid: optional
      */


      // TODO: Are these acceptable values for realm, nonceCount, and
      // digestUri??
      var nonceCount = "00000001";
      var digestUri = "xmpp/" + this.realm;
      var cNonce = this._makeCNonce();
    // Section 2.1.2.1 of RFC2831
      var A1 = str_md5( this.name + ":" + this.realm + ":" + this.password ) + ":" + challenge.nonce + ":" + cNonce;
      var A2 = "AUTHENTICATE:" + digestUri;
      var myResponse = hex_md5( hex_md5( A1 ) + ":" + challenge.nonce + ":" + nonceCount + ":" + cNonce + ":auth" + hex_md5( A2 ) );

      var responseDict = {
      username: "\"" + this.name + "\"",
      nonce: challenge.nonce,
      nc: nonceCount,
      cnonce: cNonce,
      qop: "\"auth\"",
      algorithm: "md5-sess",
      charset: "utf-8",
      response: myResponse
      };
      responseDict[ "digest-uri" ] = "\"" + digestUri + "\"";
      var responseStr = this._packChallengeResponse( responseDict );
      this._stepNumber = 2;
      return "<response xmlns='urn:ietf:params:xml:ns:xmpp-sasl'>" + responseStr + "</response>";

    } else if ( this._stepNumber = 2 ) {
      dump( "Got to step 3!" );
      // At this point the server might reject us with a 
      // <failure><not-authorized/></failure>
      if ( rootElem.nodeName == "failure" ) {
	this._errorMsg = rootElem.firstChild.nodeName;
	return false;
      }
      //this._connectionStatus = this.REQUESTED_SASL_3;
    }
    this._errorMsg = "Can't happen.";
    return false;
  },

 _unpackChallenge: function( challengeString ) {
    var challenge = atob( challengeString );
    dump( "After b64 decoding: " + challenge + "\n" );
    var challengeItemStrings = challenge.split( "," );
    var challengeItems = {};
    for ( var x in challengeItemStrings ) {
      var stuff = challengeItemStrings[x].split( "=" );
      challengeItems[ stuff[0] ] = stuff[1];
    }
    return challengeItems;
  },

 _packChallengeResponse: function( responseDict ) {
    var responseArray = []
    for( var x in responseDict ) {
      responseArray.push( x + "=" + responseDict[x] );
    }
    var responseString = responseArray.join( "," );
    dump( "Here's my response string: \n" );
    dump( responseString + "\n" );
    return btoa( responseString );
  }
};
Md5DigestAuthenticator.prototype.__proto__ = new BaseAuthenticator();


function PlainAuthenticator( ) {
  /* SASL using PLAIN authentication, which sends password in the clear. */
}
PlainAuthenticator.prototype = {
 
 generateResponse: function( rootElem ) {
    if ( this._stepNumber == 0 ) {
      if ( this.verifyProtocolSupport( rootElem, "PLAIN" ) == false ) {
	return false;
      }
      var authString = btoa( this._realm + '\0' + this._name + '\0' + this._password );
      this._stepNumber = 1;
      return "<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>" + authString + "</auth>";
    } else if ( this._stepNumber == 1 ) {
      if ( rootElem.nodeName == "failure" ) {
	// Authentication rejected: username or password may be wrong.
	this._errorMsg = rootElem.firstChild.nodeName;
	return false;
      } else if ( rootElem.nodeName == "success" ) {
	// Authentication accepted: now we start a new stream for
	// resource binding.
	/* RFC3920 part 7 says: upon receiving a success indication  within the
	   SASL negotiation, the client MUST send a new stream header to the
	   server, to which the serer MUST respond with a stream header
	   as well as a list of available stream features. */
	// TODO: resource binding happens in any authentication mechanism
	// so should be moved to base class.
	this._stepNumber = 2;
	return "<?xml version='1.0'?><stream:stream to='jonathan-dicarlos-macbook-pro.local' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>";
      }
    } else if ( this._stepNumber == 2 ) {
      // See if the server is asking us to bind a resource, and if it's
      // asking us to start a session:
      var bindNodes = rootElem.getElementsByTagName( "bind" );
      if ( bindNodes.length > 0 ) {
	this._needBinding = true;
      }
      var sessionNodes = rootElem.getElementsByTagName( "session" );
      if ( sessionNodes.length > 0 ) {
	this._needSession = true;
      }

      if ( !this._needBinding && !this._needSession ) {
	// Server hasn't requested either: we're done.
	return this.COMPLETION_CODE;
      }
      
      if ( this._needBinding ) {
	// Do resource binding:
	// Tell the server to generate the resource ID for us.
	this._stepNumber = 3;
	return "<iq type='set' id='bind_1'><bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'/></iq>";
      } 
      
      this._errorMsg = "Server requested session not binding: can't happen?";
      return false;
    } else if ( this._stepNumber == 3 ) {
      // Pull the JID out of the stuff the server sends us.
      var jidNodes = rootElem.getElementsByTagName( "jid" );
      if ( jidNodes.length == 0 ) {
	this._errorMsg = "Expected JID node from server, got none.";
	return false;
      }
      this._jid = jidNodes[0].firstChild.nodeValue;
      // TODO: Does the client need to do anything special with its new
      // "client@host.com/resourceID"  full JID?
      dump( "JID set to " + this._jid );

      // If we still need to do session, then we're not done yet:
      if ( this._needSession ) {
	this._stepNumber = 4;
	return "<iq to='" + this._realm + "' type='set' id='sess_1'><session xmlns='urn:ietf:params:xml:ns:xmpp-session'/></iq>";
      } else {
	return this.COMPLETION_CODE;
      }
    } else if ( this._stepNumber == 4 ) {
      // OK, now we're done.
      return this.COMPLETION_CODE;
    }
  }

};
PlainAuthenticator.prototype.__proto__ = new BaseAuthenticator();
