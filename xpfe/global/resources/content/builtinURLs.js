// the rdf service
var rdf_uri    = 'component://netscape/rdf/rdf-service';
var RDF        = Components.classes[rdf_uri].getService();
    RDF        = RDF.QueryInterface(Components.interfaces.nsIRDFService);
//var ds_uri     = "http://h-208-12-37-237/client-urls/en-us/builtinURLS.rdf";
var ds_uri     = "chrome://global/locale/builtinURLs.rdf";
var titleArc   = RDF.GetResource("http://home.netscape.com/NC-rdf#title");
var contentArc = RDF.GetResource("http://home.netscape.com/NC-rdf#content");
var url_ds     = null;
var rdfXMLSink = null;
var ds         = null;
var state      = 0x0; // init.
var loaded     = false;

var SinkObserver = 
{
	onBeginLoad: function( aSink) {
		state = (state | 1); 
		dump("\n-> SinkObserver:onBeginLoad: " + aSink + ", state=" + state + "\n");
	},
	
	onInterrupt: function( aSink) {
		state = (state | 2);
		dump("\n-> SinkObserver:onInterrupt: " + aSink + ", state=" + state + "\n");
	},
	
	onResume: function( aSink) {
		state = (state & ~2);
		dump("\n-> SinkObserver:onResume: " + aSink + ", state=" + state + "\n");
	},
	
	onEndLoad: function( aSink) {
		state = (state | 4);
		loaded = (state  == 5);
		dump("\n-> onEndLoad: " + aSink + ", state=" + state + ", loaded=" + loaded + "\n");

		if (!loaded) {
			dump("\n->" + ds_uri + " not loaded!\n");
			return;
		}

		ds = aSink.QueryInterface(Components.interfaces.nsIRDFDataSource);
	},

	onError: function( aSink, aStatus, aErrMsg) {
		state = (state | 8);
		dump("\n-> SinkObserver:onError: " + aSink +  ", status=" + aStatus + 
			", errMsg=" + aErrMsg + ", state=" + state + "\n");
	}
};

function loadDS()
{
	if (!RDF) {
		dump("\n-->loadDS(): RDF is null!\n");
		return;
	}
	
	url_ds = RDF.GetDataSource(ds_uri); // return nsIRDFDataSource
	if (!url_ds) {
		dump("\n >>Can't get " + ds_uri + "<-\n");
		return;
	} 	
	rdfXMLSink = url_ds.QueryInterface( Components.interfaces.nsIRDFXMLSink );
	if (rdfXMLSink) {
		rdfXMLSink.addXMLSinkObserver(SinkObserver);
	}
}

function xlateURL(key)
{
	if (!ds || !loaded) {
		dump("\n data source " + ds_uri + "is not loaded! Try again later! \n");
		return;
	}
	// get data
	var srcNode = RDF.GetResource(key);

	var titleTarget = ds.GetTarget(srcNode, titleArc, true);
	if (titleTarget) {
		titleTarget = 
			titleTarget.QueryInterface(Components.interfaces.nsIRDFLiteral);
		dump("\n-> " + key + "::title=" + titleTarget.Value + "\n");
	}
	else {
		dump("\n title target=" + titleTarget + "\n");
	}

	var contentTarget = ds.GetTarget(srcNode, contentArc, true);
	if (contentTarget) {
		contentTarget = 
			contentTarget.QueryInterface(Components.interfaces.nsIRDFLiteral);
		dump("\n-> " + key + "::content=" + contentTarget.Value + "\n");
		return contentTarget.Value;

	}
	else {
		dump("\n content target=" + contentTarget + "\n");
	}
}

function loadXURL(key)
{
	window.content.location.href = xlateURL(key);
}

loadDS();
