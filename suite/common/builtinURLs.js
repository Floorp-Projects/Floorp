// the rdf service
var gRDFService;
var gTitleArc;
var gContentArc;
var gBuiltinUrlsDataSource;
var gDataSourceState;
var gDataSourceLoaded;
var gSinkObserverRegistered;

var SinkObserver = 
{
	onBeginLoad: function( aSink) {
		gDataSourceState = (gDataSourceState | 1); 
		debug_dump("\n-> SinkObserver:onBeginLoad: " + aSink + ", gDataSourceState=" + gDataSourceState + "\n");
	},
	
	onInterrupt: function( aSink) {
		gDataSourceState = (gDataSourceState | 2);
		debug_dump("\n-> SinkObserver:onInterrupt: " + aSink + ", gDataSourceState=" + gDataSourceState + "\n");
	},
	
	onResume: function( aSink) {
		gDataSourceState = (gDataSourceState & ~2);
		debug_dump("\n-> SinkObserver:onResume: " + aSink + ", gDataSourceState=" + gDataSourceState + "\n");
	},
	
	onEndLoad: function( aSink) {
		gDataSourceState = (gDataSourceState | 4);
		gDataSourceLoaded = (gDataSourceState  == 5);
		
		debug_dump("\n-> onEndLoad: " + aSink + ", gDataSourceState=" + gDataSourceState + ", gDataSourceLoaded=" + gDataSourceLoaded + "\n");

		if (!gDataSourceLoaded) {
			debug_dump("\n-> builtin URLs not loaded!\n");
			return;
		}

		gBuiltinUrlsDataSource = aSink.QueryInterface(Components.interfaces.nsIRDFDataSource);
		
		debug_dump("Got gBuiltinUrlsDataSource " + gBuiltinUrlsDataSource + " with gTitleArc " + gTitleArc + " and gContentArc " + gContentArc + "\n");
	},

	onError: function( aSink, aStatus, aErrMsg) {
		gDataSourceState = (gDataSourceState | 8);
		debug_dump("\n-> SinkObserver:onError: " + aSink +  ", status=" + aStatus + 
			", errMsg=" + aErrMsg + ", gDataSourceState=" + gDataSourceState + "\n");
	}
};

function debug_dump(msg)
{
}

/*
function debug_dump(msg)
{
    dump(msg);
}
*/

function loadDS()
{
	debug_dump("\n-->loadDS() called for " + window.document + " <--\n");
	if (gBuiltinUrlsDataSource && gDataSourceLoaded) {
		debug_dump("\n-->loadDS(): gBuiltinUrlsDataSource=" + gBuiltinUrlsDataSource + ", gDataSourceLoaded=" + gDataSourceLoaded + ", returning! <--\n");
		return;
    }
    
    if (gSinkObserverRegistered)
    {
        debug_dump("Already registered SinkObserver in loadDS()\n");
        return;
    }
    
	// initialize
	gRDFService = Components.classes['@mozilla.org/rdf/rdf-service;1'].getService();
    gRDFService = gRDFService.QueryInterface(Components.interfaces.nsIRDFService);

	if (!gRDFService) {
		debug_dump("\n-->loadDS(): gRDFService service is null!\n");
		return;
	}
	
    gTitleArc   = gRDFService.GetResource("http://home.netscape.com/NC-rdf#title");
    gContentArc = gRDFService.GetResource("http://home.netscape.com/NC-rdf#content");

    var ds_uri = "chrome://global-region/locale/builtinURLs.rdf";
    var url_ds = gRDFService.GetDataSource(ds_uri); // return nsIRDFDataSource
	if (!url_ds) {
		debug_dump("\n >>Can't get " + ds_uri + "<-\n");
		return;
	}
	
	if (url_ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource).loaded)
	{
        ds_uri = "chrome://global-region/locale/builtinURLs.rdf";
        gBuiltinUrlsDataSource = gRDFService.GetDataSource(ds_uri); // return nsIRDFDataSource
	    gDataSourceLoaded = (gBuiltinUrlsDataSource != null);
	}
	else
	{
    	var rdfXMLSink = url_ds.QueryInterface( Components.interfaces.nsIRDFXMLSink );
    	if (rdfXMLSink) {
    		gBuiltinUrlsDataSource         = null;
    		gDataSourceState      = 0x0; // init.
    		gDataSourceLoaded     = false;
            gSinkObserverRegistered = true;
            
    		rdfXMLSink.addXMLSinkObserver(SinkObserver);
    	}
    	else
    	{
    	    debug_dump("rdfXMLSink is null\n");
    	}
	}
}

function xlateURL(key)
{
	debug_dump("\n>> xlateURL(" + key + "): gBuiltinUrlsDataSource=" + gBuiltinUrlsDataSource + ", gDataSourceLoaded=" + gDataSourceLoaded + "\n");
	
	if (!gBuiltinUrlsDataSource || !gDataSourceLoaded) {
		throw("urn translation data source not loaded");
	}
	// get data
	var srcNode = gRDFService.GetResource(key);
	var titleTarget = gBuiltinUrlsDataSource.GetTarget(srcNode, gTitleArc, true);
	if (titleTarget) {
		titleTarget = 
			titleTarget.QueryInterface(Components.interfaces.nsIRDFLiteral);
		debug_dump("\n-> " + key + "::title=" + titleTarget.Value);
	}
	else {
		debug_dump("\n title target=" + titleTarget + "\n");
	}

	var contentTarget = gBuiltinUrlsDataSource.GetTarget(srcNode, gContentArc, true);
	if (contentTarget) {
		contentTarget = 
			contentTarget.QueryInterface(Components.interfaces.nsIRDFLiteral);
		debug_dump("\n-> " + key + "::content=" + contentTarget.Value + "\n");
		return contentTarget.Value;
	}
	else {
		debug_dump("\n content target=" + contentTarget + "\n");
		throw("urn not found in datasource");
	}
	
	// not reached
	return "";
}

function loadXURL(key)
{
  debug_dump("loadXURL call with " + key + "\n");
    
  var url = xlateURL(key);
  //check to see if this is a browser window before opening.
  var winType = document.documentElement.getAttribute("windowtype");

  if (window.content && winType == "navigator:browser")
    window.content.location.href = url;
  else
    window.open(url); // on mac, there maybe no open windows: see bug 83329
}

loadDS();
