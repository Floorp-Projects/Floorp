/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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
        dump(err);
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
      }
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
  httpPostRequest : function (url, data, callback, errback) {
    // do a xmlhttprequest sending data with the post method
    var req = getXMLHttpRequest();
    req.open("POST", url, true);
      req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
      req.setRequestHeader("Content-length", data.length);
      req.setRequestHeader("Connection", "close");
      req = sendXMLHttpRequest(req, data);
      req.addErrback(errback);
      req.addCallback(callback);
  },
  showHideLoadingMessage : function(box, bool) {
    if (bool == true) { // show
      var loading = document.createElementNS("http://www.w3.org/1999/xhtml", "p");
      loading.textContent = qaMain.bundle.getString("qa.extension.loading");
      loading.setAttributeNS("http://www.w3.org/1999/xhtml",
                             "class", "loading_message");
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
    arrayify : function(obj) {
        if (obj instanceof Array) {
            return obj;
        }
        var newArray = new Array();
        newArray[0] = obj;
        return newArray;
    },
    writeSafeHTML : function(elementID, htmlstr) {
        document.getElementById(elementID).textContent = "";  //clear it.
        var utils = Components.classes["@mozilla.org/parserutils;1"].getService(Components.interfaces.nsIParserUtils);
        var context = document.getElementById(elementID);
        var fragment = utils.parseFragment(htmlstr, 0, false, null, context);
        context.appendChild(fragment);
    },

    assignLinkHandlers : function(node) {
        var children = node.getElementsByTagName('a');
        for (var i = 0; i < children.length; i++)
           children[i].addEventListener("click", qaTools.handleLink, false);
    },
    assignLinkHandler : function(link) {
        link.addEventListener("click", qaTools.handleLink, false);
    },
    handleLink : function(event) {
        var url = this.href;
        var type = qaPref.getPref("browser.link.open_external", "int");
        var where = "tab";
        if (type == 2) where = "window";

        openUILinkIn(url, where);
        event.preventDefault(); // prevent it from simply following the href
    },
    makeUniqueArray : function(array) {
        var RV = new Array();
        for( var i = 0; i < array.length; i++ ) {
            if( RV.indexOf(array[i]) < 0 ) { RV.push( array[i] ); }
        }
        return RV;
    }
};

function getCleanText(inputText)
{
  inputText = inputText.replace(/&#64;/g,"@");
  inputText = inputText.replace(/&quot;/g,"\"");
  inputText = inputText.replace(/&lt;/g, "<");
  inputText = inputText.replace(/&gt;/g, ">");
  inputText = inputText.replace(/&amp;/g, "&");
  inputText = inputText.replace(/<[^>]*>/g, "");
  inputText = inputText.replace(/\s+/g, " ");
  inputText = inputText.replace(/\n*/g, "");

  return inputText;
}
