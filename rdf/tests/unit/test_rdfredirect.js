do_load_httpd_js();

function getRDFService()
{
  return Components.classes["@mozilla.org/rdf/rdf-service;1"].
    getService(Components.interfaces.nsIRDFService);
}

var server1, server2;

function run_test()
{
  var samplefile = do_get_file('sample.rdf');
  
  server1 = new nsHttpServer();
  server1.registerPathHandler("/sample-xs.rdf", xsRedirect);
  server1.registerPathHandler("/sample-local.rdf", localRedirect);
  server1.registerFile('/sample.rdf', samplefile);
  server1.start(4444);

  server2 = new nsHttpServer();
  server2.registerFile('/sample.rdf', samplefile);
  server2.start(4445);

  do_test_pending();

  new rdfLoadObserver('http://localhost:4444/sample.rdf', true);
  new rdfLoadObserver('http://localhost:4445/sample.rdf', true);
  new rdfLoadObserver('http://localhost:4444/sample-xs.rdf', false);
  new rdfLoadObserver('http://localhost:4444/sample-local.rdf', true);
}

var gPending = 0;

function rdfLoadObserver(uri, shouldPass)
{
  this.shouldPass = shouldPass;
  this.uri = uri;
  
  ++gPending;
  
  var rdfService = getRDFService();
  this.ds = rdfService.GetDataSource(uri).
    QueryInterface(Components.interfaces.nsIRDFXMLSink);
  this.ds.addXMLSinkObserver(this);
}

rdfLoadObserver.prototype =
{
  onBeginLoad : function() { },
  onInterrupt : function() { },
  onResume : function() { },
  onEndLoad : function() {
    print("Testing results of loading " + this.uri);
    
    var rdfs = getRDFService();
    var res = rdfs.GetResource("urn:mozilla:sample-data");
    var arc = rdfs.GetResource("http://purl.org/dc/elements/1.1/title");
    var answer = this.ds.GetTarget(res, arc, true);
    if (answer !== null) {
      do_check_true(this.shouldPass);
      do_check_true(answer instanceof Components.interfaces.nsIRDFLiteral);
      do_check_eq(answer.Value, "Sample");
    }
    else {
      do_check_false(this.shouldPass);
    }
      
    gPending -= 1;
      
    this.ds.removeXMLSinkObserver(this);

    if (gPending == 0) {
      do_test_pending();
      server1.stop(do_test_finished);
      server2.stop(do_test_finished);
    }
  },
  onError : function() { }
}

function xsRedirect(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", "http://localhost:4445/sample.rdf", false);
}

function localRedirect(metadata, response)
{
  response.setStatusLine(metadata.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", "http://localhost:4444/sample.rdf", false);
}
