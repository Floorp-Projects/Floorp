Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");



const kSCHEME = "tel";
const kPROTOCOL_NAME = "Tel Protocol";
const kPROTOCOL_CONTRACTID = "@mozilla.org/network/protocol;1?name=" + kSCHEME;
const kPROTOCOL_CID = Components.ID("d4bc06cc-fa9f-48ce-98e4-5326ca96ba28");

// Mozilla defined
const kSIMPLEURI_CONTRACTID = "@mozilla.org/network/simple-uri;1";
const kIOSERVICE_CONTRACTID = "@mozilla.org/network/io-service;1";

const Ci = Components.interfaces;



function TelProtocol()
{
}

TelProtocol.prototype =
{
    
  classDescription: "Handler for tel URIs",
  classID:          Components.ID(kPROTOCOL_CID),
  contractID:       kPROTOCOL_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler]),
  
  
  scheme: kSCHEME,
  defaultPort: -1,
  protocolFlags: Ci.nsIProtocolHandler.URI_NORELATIVE | 
                 Ci.nsIProtocolHandler.URI_NOAUTH,
  
  allowPort: function(port, scheme)
  {
    return false;
  },
  
  newURI: function(spec, charset, baseURI)
  {
    var uri = Components.classes[kSIMPLEURI_CONTRACTID].createInstance(Ci.nsIURI);
    uri.spec = spec;
    return uri;
  },
  
  
  newChannel: function(aURI)
  {
    // aURI is a nsIUri, so get a string from it using .spec
    let phoneNumber = aURI.spec;
    
    // strip away the kSCHEME: part
    phoneNumber = phoneNumber.substring(phoneNumber.indexOf(":") + 1, phoneNumber.length);    
    phoneNumber = encodeURI(phoneNumber);
    
    var ios = Components.classes[kIOSERVICE_CONTRACTID].getService(Ci.nsIIOService);
    
    try {
      //try voipto:
      let channel = ios.newChannel("voipto:"+phoneNumber, null, null);
      return channel;
    } catch(e){
      //voipto not registered, try the next one
    }
    try {
      //try callto:
      let channel = ios.newChannel("callto:"+phoneNumber, null, null);
      return channel;
    } catch(e){
      //callto not registered, try the next one
    }
    //try wtai:  
    //this is our last equivalent protocol, so if it fails pass the exception on
    let channel = ios.newChannel("wtai:"+phoneNumber, null, null);
    return channel;
  }
};

var components = [TelProtocol];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}

