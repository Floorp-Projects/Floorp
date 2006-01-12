
function rssfetch(refTab,targetDoc, targetElement) {
	//http://rss.slashdot.org/Slashdot/slashdot
	//http://del.icio.us/rss/taboca


	var testLoad=new blenderObject();
	//testLoad.xmlSet("http://rss.slashdot.org/Slashdot/slashdot");
      testLoad.xmlSet("http://del.icio.us/rss/tag/"+gRSSTag);
	
	//testLoad.xslSet("chrome://minimo/content/rssview/rss_template.xml");

	testLoad.xslSet("chrome://minimo/content/rssview/rss_template_html.xml");
	testLoad.refTab=refTab;

	//testLoad.setTargetDocument(document.getElementById("container-output").contentDocument);
	//testLoad.setTargetElement(document.getElementById("container-output").contentDocument.getElementsByTagName("treeitem").item(ii));

	testLoad.setTargetDocument(targetDoc);
	//testLoad.setTargetElement(getBrowser().selectedBrowser.contentDocument.getElementsByTagName("body").item(0));
	testLoad.setTargetElement(targetElement);

	testLoad.run();

}


function rssfetchpage(targetDoc, targetElement) {


	var testLoad=new blenderObject();
	testLoad.xmlSet(document.location.search.split("?url=")[1]);
	testLoad.xslSet("rss_template_html.xml");
	testLoad.setTargetDocument(targetDoc);
	testLoad.setTargetElement(targetElement);
	testLoad.run();

}


////
/// loads the XSL style and data-source and mix them into a new doc. 
//

function blenderObject() {
	this.xmlRef=document.implementation.createDocument("","",null);
	this.xslRef=document.implementation.createDocument("http://www.w3.org/1999/XSL/Transform","stylesheet",null);



	this.xmlUrl="";
	this.xslUrl="";

	this.refTab=null;

	var myThis=this;
	var lambda=function thisScopeFunction() { myThis.xmlLoaded(); }
	var omega=function thisScopeFunction2() { myThis.xslLoaded(); }

	this.xmlRef.addEventListener("load",lambda,false);
	this.xslRef.addEventListener("load",omega,false);

	this.xmlLoadedState=false;
	this.xslLoadedState=false;
}

blenderObject.prototype.xmlLoaded = function () {
	this.xmlLoadedState=true;
	//serialXML=new XMLSerializer();
	//alert(serialXML.serializeToString(this.xmlRef));
	this.apply();
}

blenderObject.prototype.xslLoaded = function () {
	this.xslLoadedState=true;
	this.apply();
}

blenderObject.prototype.xmlSet = function (urlstr) {
	this.xmlUrl=urlstr;
}

blenderObject.prototype.xslSet = function (urlstr) {
	this.xslUrl=urlstr;
}

blenderObject.prototype.setTargetDocument = function (targetDoc) {
	this.targetDocument=targetDoc;
}

blenderObject.prototype.setTargetElement = function (targetEle) {
	this.targetElement=targetEle;
}

blenderObject.prototype.apply = function () {
	if(this.xmlLoadedState&&this.xslLoadedState) {
		var xsltProcessor = new XSLTProcessor();
		var htmlFragment=null;
		try {
			xsltProcessor.importStylesheet(this.xslRef);
			htmlFragment = xsltProcessor.transformToFragment(this.xmlRef, this.targetDocument);
		} catch (e) {
		}
        this.targetElement.appendChild(htmlFragment.firstChild);

        /*
         * Updates the page title if pagetitle generated ID exists. 
         * Check the rss_*.xml templates for additional details..
         */
        if(document.getElementById("pagetitle")) {
          document.title=document.getElementById("pagetitle").innerHTML;
        }
	}
}

blenderObject.prototype.run = function () {
	try {
		//this.xmlRef.load(this.xmlUrl);

		req = new XMLHttpRequest();
		req.open('GET', this.xmlUrl, false); 
		req.send(null);
		if(req.status == 200) {
			this.xmlRef=req.responseXML;
			this.xmlLoadedState=true;

		}


	} catch (e) {
		alert(e);
	}
	try {
		this.xslRef.load(this.xslUrl);
	} catch (e) {
		alert(e);
	}

}

