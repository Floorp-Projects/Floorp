const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

var qaTools = {
	// load a JSON doc into a menulist/menupopup
	// takes the url to load, the menulist to populate, the name of the method 
	// to use to get the name from the JSON doc, and the name of the method to 
	// use to get the value from the JSON doc, plus an optional callback
	loadJsonMenu : function(url, menulist, nameMethod, valueMethod, callback) {
		var d = loadJSONDoc(url);
		d.addErrback(function (err) { 
        if (err instanceof CancelledError) { 
          return; 
        }
        alert(err);
      });
	  d.addCallback(function(obj) {
	      if (obj instanceof Array) {
	          for (var i=0; i<obj.length; i++) {
	              var item = obj[i];
	              if (! item) { continue; }
	              var newitem = menulist.appendItem(item[nameMethod], 
	              									item[valueMethod]);
	          }
	      } else {
	          var newitem = menulist.appendItem(obj[nameMethod], obj[valueMethod]);
	      }
	      // stash the JSON object in the userData attribute for 
	      // later use (e.g. when filtering the list).
	      newitem.userData = item;
	      if (callback) {
	      	callback();
	      }
	  });
	},
	fetchFeed : function(url, callback) {
		var httpRequest = null;
		function FeedResultListener() {}
		FeedResultListener.prototype = {
			handleResult : function(result) {
				var feed = result.doc;
				feed.QueryInterface(Ci.nsIFeed);
				callback(feed);
			},
		};
		
		function infoReceived() {
			var data = httpRequest.responseText;
			var ioService = Cc['@mozilla.org/network/io-service;1']
							   .getService(Ci.nsIIOService);
			var uri = ioService.newURI(url, null, null);
			if (data.length) {
				var processor = Cc["@mozilla.org/feed-processor;1"]
								   .createInstance(Ci.nsIFeedProcessor);
				try {
					processor.listener = new FeedResultListener;
					processor.parseFromString(data, uri);
				} catch(e) {
					alert("Error parsing feed: " + e);
				}
			}
		}
		httpRequest = new XMLHttpRequest();
		httpRequest.open("GET", url, true);
		try {
			httpRequest.onload = infoReceived;
			httpRequest.send(null);
		} catch(e) {
			alert(e);
		}
	},
	showHideLoadingMessage : function(box, bool) {
		if (bool == true) { // show
			var loading = document.createElementNS("http://www.w3.org/1999/xhtml", "p");
			loading.textContent = qaMain.bundle.getString("qa.extension.loading");
			loading.setAttributeNS("http://www.w3.org/1999/xhtml", "class", "loading_message");
			box.appendChild(loading);
		} else { // hide
			var elements = box.childNodes;
			for (var i=0; i<elements.length; i++) {
				if (elements[i] && elements[i].getAttributeNS && 
				  elements[i].getAttributeNS(
				  "http://www.w3.org/1999/xhtml", "class") == "loading_message") {
					box.removeChild(elements[i]);
					break;
				}
			}
		}
	},
};
