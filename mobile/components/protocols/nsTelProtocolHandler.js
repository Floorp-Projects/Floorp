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

#ifdef WINCE
    try {
      //try tel:
      let channel = new nsWinceTelChannel (aURI, phoneNumber);
      return channel;
    } catch(e){
      //tel not registered, try the next one
    }
#endif
    
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

#ifdef WINCE
function nsWinceTelChannel(URI, phoneNumber)
{
    this.URI = URI;
    this.originalURI = URI;
    this.phoneNumber = phoneNumber
}

nsWinceTelChannel.prototype.QueryInterface =
function bc_QueryInterface(iid)
{
    if (!iid.equals(Ci.nsIChannel) && !iid.equals(Ci.nsIRequest) &&
        !iid.equals(Ci.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
}

/* nsIChannel */
nsWinceTelChannel.prototype.loadAttributes = null;
nsWinceTelChannel.prototype.contentLength = 0;
nsWinceTelChannel.prototype.owner = null;
nsWinceTelChannel.prototype.loadGroup = null;
nsWinceTelChannel.prototype.notificationCallbacks = null;
nsWinceTelChannel.prototype.securityInfo = null;

nsWinceTelChannel.prototype.open =
nsWinceTelChannel.prototype.asyncOpen =
function bc_open(observer, ctxt)
{    
    var phoneInterface= Components.classes["@mozilla.org/phone/support;1"].createInstance(Ci.nsIPhoneSupport);
    phoneInterface.makeCall(this.phoneNumber,"",false);
    
    // We don't throw this (a number, not a real 'resultcode') because it
    // upsets xpconnect if we do (error in the js console).
    Components.returnCode = NS_ERROR_NO_CONTENT;
}

nsWinceTelChannel.prototype.asyncRead =
function bc_asyncRead(listener, ctxt)
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}

/* nsIRequest */
nsWinceTelChannel.prototype.isPending =
function bc_isPending()
{
    return true;
}

nsWinceTelChannel.prototype.status = Components.results.NS_OK;

nsWinceTelChannel.prototype.cancel =
function bc_cancel(status)
{
    this.status = status;
}

nsWinceTelChannel.prototype.suspend =
nsWinceTelChannel.prototype.resume =
function bc_suspres()
{
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
}
#endif
