// the rdf service
var RDF;
var titleArc;
var contentArc;
var ds;
var state;
var loaded;

var SinkObserver = 
{
	onBeginLoad: function( aSink) {
		state = (state | 1); 
		//dump("\n-> SinkObserver:onBeginLoad: " + aSink + ", state=" + state + "\n");
	},
	
	onInterrupt: function( aSink) {
		state = (state | 2);
		//dump("\n-> SinkObserver:onInterrupt: " + aSink + ", state=" + state + "\n");
	},
	
	onResume: function( aSink) {
		state = (state & ~2);
		//dump("\n-> SinkObserver:onResume: " + aSink + ", state=" + state + "\n");
	},
	
	onEndLoad: function( aSink) {
		state = (state | 4);
		loaded = (state  == 5);
		//dump("\n-> onEndLoad: " + aSink + ", state=" + state + ", loaded=" + loaded + "\n");

		if (!loaded) {
			dump("\n-> builtin URLs not loaded!\n");
			return;
		}

		ds = aSink.QueryInterface(Components.interfaces.nsIRDFDataSource);

		titleArc   = RDF.GetResource("http://home.netscape.com/NC-rdf#title");
		contentArc = RDF.GetResource("http://home.netscape.com/NC-rdf#content");
	},

	onError: function( aSink, aStatus, aErrMsg) {
		state = (state | 8);
		dump("\n-> SinkObserver:onError: " + aSink +  ", status=" + aStatus + 
			", errMsg=" + aErrMsg + ", state=" + state + "\n");
	}
};

function loadDS()
{
	//dump("\n-->loadDS() called <--\n");
	if (ds && loaded) {
		//dump("\n-->loadDS(): ds=" + ds + ", loaded=" + loaded + ", returning! <--\n");
		return;
    }

	// initialize
	RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

	if (!RDF) {
		dump("\n-->loadDS(): RDF service is null!\n");
		return;
	}
	
    var ds_uri = "chrome://global/locale/builtinURLs.rdf";
    var url_ds = RDF.GetDataSource(ds_uri); // return nsIRDFDataSource
	if (!url_ds) {
		dump("\n >>Can't get " + ds_uri + "<-\n");
		return;
	} 	
	var rdfXMLSink = url_ds.QueryInterface( Components.interfaces.nsIRDFXMLSink );
	if (rdfXMLSink) {
		ds         = null;
		state      = 0x0; // init.
		loaded     = false;

		rdfXMLSink.addXMLSinkObserver(SinkObserver);
	}
}

function xlateURL(key)
{
	//dump("\n>> xlateURL(" + key + "): ds=" + ds + ", loaded=" + loaded);
	if (!ds || !loaded) {
		dump("\n xlateURL(): data source is not loaded! Try again later! \n");
		return "";
	}
	// get data
	var srcNode = RDF.GetResource(key);
	var titleTarget = ds.GetTarget(srcNode, titleArc, true);
	if (titleTarget) {
		titleTarget = 
			titleTarget.QueryInterface(Components.interfaces.nsIRDFLiteral);
		dump("\n-> " + key + "::title=" + titleTarget.Value);
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
	
	return "";
}

function loadXURL(key)
{
	window._content.location.href = xlateURL(key);
}

loadDS();
