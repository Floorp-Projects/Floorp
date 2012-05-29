/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Elem", "ID", "Link", "XPath", "Selector", "Name", "Anon", "AnonXPath",
                        "Lookup", "_byID", "_byName", "_byAttrib", "_byAnonAttrib",
                       ];

var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);
var strings = {}; Components.utils.import('resource://mozmill/stdlib/strings.js', strings);
var arrays = {}; Components.utils.import('resource://mozmill/stdlib/arrays.js', arrays);
var json2 = {}; Components.utils.import('resource://mozmill/stdlib/json2.js', json2);
var withs = {}; Components.utils.import('resource://mozmill/stdlib/withs.js', withs);
var dom = {}; Components.utils.import('resource://mozmill/stdlib/dom.js', dom);
var objects = {}; Components.utils.import('resource://mozmill/stdlib/objects.js', objects);

var countQuotes = function(str){
  var count = 0;
  var i = 0;
  while(i < str.length) {
    i = str.indexOf('"', i);
    if (i != -1) {
      count++;
      i++;
    } else {
      break;
    }
  }
  return count;
};

/**
 * smartSplit()
 *
 * Takes a lookup string as input and returns
 * a list of each node in the string
 */
var smartSplit = function (str) {
  // Ensure we have an even number of quotes
  if (countQuotes(str) % 2 != 0) {
    throw new Error ("Invalid Lookup Expression");
  }

  /**
   * This regex matches a single "node" in a lookup string.
   * In otherwords, it matches the part between the two '/'s
   *
   * Regex Explanation:
   * \/ - start matching at the first forward slash
   * ([^\/"]*"[^"]*")* - match as many pairs of quotes as possible until we hit a slash (ignore slashes inside quotes)
   * [^\/]* - match the remainder of text outside of last quote but before next slash
   */
  var re = /\/([^\/"]*"[^"]*")*[^\/]*/g
  var ret = []
  var match = re.exec(str);
  while (match != null) {
    ret.push(match[0].replace(/^\//, ""));
    match = re.exec(str);
  }
  return ret;
};

/**
 * defaultDocuments()
 *
 * Returns a list of default documents in which to search for elements
 * if no document is provided
 */
function defaultDocuments() {
  var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService(Components.interfaces.nsIWindowMediator);
  win = windowManager.getMostRecentWindow("navigator:browser");
  return [win.gBrowser.selectedBrowser.contentDocument, win.document];
};

/**
 * nodeSearch()
 *
 * Takes an optional document, callback and locator string
 * Returns a handle to the located element or null
 */
function nodeSearch(doc, func, string) {
  if (doc != undefined) {
    var documents = [doc];
  } else {
    var documents = defaultDocuments();
  }
  var e = null;
  var element = null;
  //inline function to recursively find the element in the DOM, cross frame.
  var search = function(win, func, string) {
    if (win == null)
      return;

    //do the lookup in the current window
    element = func.call(win, string);

    if (!element || (element.length == 0)) {
      var frames = win.frames;
      for (var i=0; i < frames.length; i++) {
        search(frames[i], func, string);
      }
    }
    else { e = element; }
  };

  for (var i = 0; i < documents.length; ++i) {
    var win = documents[i].defaultView;
    search(win, func, string);
    if (e) break;
  }
  return e;
};

/**
 * Selector()
 *
 * Finds an element by selector string
 */
function Selector(_document, selector, index) {
  if (selector == undefined) {
    throw new Error('Selector constructor did not recieve enough arguments.');
  }
  this.selector = selector;
  this.getNodeForDocument = function (s) {
    return this.document.querySelectorAll(s);
  };
  var nodes = nodeSearch(_document, this.getNodeForDocument, this.selector);
  return nodes ? nodes[index || 0] : null;
};

/**
 * ID()
 *
 * Finds an element by ID
 */
function ID(_document, nodeID) {
  if (nodeID == undefined) {
    throw new Error('ID constructor did not recieve enough arguments.');
  }
  this.getNodeForDocument = function (nodeID) {
    return this.document.getElementById(nodeID);
  };
  return nodeSearch(_document, this.getNodeForDocument, nodeID);
};

/**
 * Link()
 *
 * Finds a link by innerHTML
 */
function Link(_document, linkName) {
  if (linkName == undefined) {
    throw new Error('Link constructor did not recieve enough arguments.');
  }

  this.getNodeForDocument = function (linkName) {
    var getText = function(el){
      var text = "";
      if (el.nodeType == 3){ //textNode
        if (el.data != undefined){
          text = el.data;
        } else {
          text = el.innerHTML;
        }
      text = text.replace(/n|r|t/g, " ");
      }
      if (el.nodeType == 1){ //elementNode
        for (var i = 0; i < el.childNodes.length; i++) {
          var child = el.childNodes.item(i);
          text += getText(child);
        }
        if (el.tagName == "P" || el.tagName == "BR" || el.tagName == "HR" || el.tagName == "DIV") {
          text += "n";
        }
      }
      return text;
    };

    //sometimes the windows won't have this function
    try {
      var links = this.document.getElementsByTagName('a'); }
    catch(err){ // ADD LOG LINE mresults.write('Error: '+ err, 'lightred');
    }
    for (var i = 0; i < links.length; i++) {
      var el = links[i];
      //if (getText(el).indexOf(this.linkName) != -1) {
      if (el.innerHTML.indexOf(linkName) != -1){
        return el;
      }
    }
    return null;
  };

  return nodeSearch(_document, this.getNodeForDocument, linkName);
};

/**
 * XPath()
 *
 * Finds an element by XPath
 */
function XPath(_document, expr) {
  if (expr == undefined) {
    throw new Error('XPath constructor did not recieve enough arguments.');
  }

  this.getNodeForDocument = function (s) {
    var aNode = this.document;
    var aExpr = s;
    var xpe = null;

    if (this.document.defaultView == null) {
      xpe = new getMethodInWindows('XPathEvaluator')();
    } else {
      xpe = new this.document.defaultView.XPathEvaluator();
    }
    var nsResolver = xpe.createNSResolver(aNode.ownerDocument == null ? aNode.documentElement : aNode.ownerDocument.documentElement);
    var result = xpe.evaluate(aExpr, aNode, nsResolver, 0, null);
    var found = [];
    var res;
    while (res = result.iterateNext())
      found.push(res);
    return found[0];
  };
  return nodeSearch(_document, this.getNodeForDocument, expr);
};

/**
 * Name()
 *
 * Finds an element by Name
 */
function Name(_document, nName) {
  if (nName == undefined) {
    throw new Error('Name constructor did not recieve enough arguments.');
  }
  this.getNodeForDocument = function (s) {
    try{
      var els = this.document.getElementsByName(s);
      if (els.length > 0) { return els[0]; }
    }
    catch(err){};
    return null;
  };
  return nodeSearch(_document, this.getNodeForDocument, nName);
};


var _returnResult = function (results) {
  if (results.length == 0) {
    return null
  } else if (results.length == 1) {
    return results[0];
  } else {
    return results;
  }
}
var _forChildren = function (element, name, value) {
  var results = [];
  var nodes = [e for each (e in element.childNodes) if (e)]
  for (var i in nodes) {
    var n = nodes[i];
    if (n[name] == value) {
      results.push(n);
    }
  }
  return results;
}
var _forAnonChildren = function (_document, element, name, value) {
  var results = [];
  var nodes = [e for each (e in _document.getAnoymousNodes(element)) if (e)];
  for (var i in nodes ) {
    var n = nodes[i];
    if (n[name] == value) {
      results.push(n);
    }
  }
  return results;
}
var _byID = function (_document, parent, value) {
  return _returnResult(_forChildren(parent, 'id', value));
}
var _byName = function (_document, parent, value) {
  return _returnResult(_forChildren(parent, 'tagName', value));
}
var _byAttrib = function (parent, attributes) {
  var results = [];

  var nodes = parent.childNodes;
  for (var i in nodes) {
    var n = nodes[i];
    requirementPass = 0;
    requirementLength = 0;
    for (var a in attributes) {
      requirementLength++;
      try {
        if (n.getAttribute(a) == attributes[a]) {
          requirementPass++;
        }
      } catch (err) {
        // Workaround any bugs in custom attribute crap in XUL elements
      }
    }
    if (requirementPass == requirementLength) {
      results.push(n);
    }
  }
  return _returnResult(results)
}
var _byAnonAttrib = function (_document, parent, attributes) {
  var results = [];

  if (objects.getLength(attributes) == 1) {
    for (var i in attributes) {var k = i; var v = attributes[i]; }
    var result = _document.getAnonymousElementByAttribute(parent, k, v)
    if (result) {
      return result;

    }
  }
  var nodes = [n for each (n in _document.getAnonymousNodes(parent)) if (n.getAttribute)];
  function resultsForNodes (nodes) {
    for (var i in nodes) {
      var n = nodes[i];
      requirementPass = 0;
      requirementLength = 0;
      for (var a in attributes) {
        requirementLength++;
        if (n.getAttribute(a) == attributes[a]) {
          requirementPass++;
        }
      }
      if (requirementPass == requirementLength) {
        results.push(n);
      }
    }
  }
  resultsForNodes(nodes)
  if (results.length == 0) {
    resultsForNodes([n for each (n in parent.childNodes) if (n != undefined && n.getAttribute)])
  }
  return _returnResult(results)
}
var _byIndex = function (_document, parent, i) {
  if (parent instanceof Array) {
    return parent[i];
  }
  return parent.childNodes[i];
}
var _anonByName = function (_document, parent, value) {
  return _returnResult(_forAnonChildren(_document, parent, 'tagName', value));
}
var _anonByAttrib = function (_document, parent, value) {
  return _byAnonAttrib(_document, parent, value);
}
var _anonByIndex = function (_document, parent, i) {
  return _document.getAnonymousNodes(parent)[i];
}

/**
 * Lookup()
 *
 * Finds an element by Lookup expression
 */
function Lookup (_document, expression) {
  if (expression == undefined) {
    throw new Error('Lookup constructor did not recieve enough arguments.');
  }

  var expSplit = [e for each (e in smartSplit(expression) ) if (e != '')];
  expSplit.unshift(_document)
  var nCases = {'id':_byID, 'name':_byName, 'attrib':_byAttrib, 'index':_byIndex};
  var aCases = {'name':_anonByName, 'attrib':_anonByAttrib, 'index':_anonByIndex};


  var reduceLookup = function (parent, exp) {
    // Handle case where only index is provided
    var cases = nCases;

    // Handle ending index before any of the expression gets mangled
    if (withs.endsWith(exp, ']')) {
      var expIndex = json2.JSON.parse(strings.vslice(exp, '[', ']'));
    }
    // Handle anon
    if (withs.startsWith(exp, 'anon')) {
      var exp = strings.vslice(exp, '(', ')');
      var cases = aCases;
    }
    if (withs.startsWith(exp, '[')) {
      try {
        var obj = json2.JSON.parse(strings.vslice(exp, '[', ']'));
      } catch (err) {
        throw new Error(err+'. String to be parsed was || '+strings.vslice(exp, '[', ']')+' ||');
      }
      var r = cases['index'](_document, parent, obj);
      if (r == null) {
        throw new Error('Expression "'+exp+'" returned null. Anonymous == '+(cases == aCases));
      }
      return r;
    }

    for (var c in cases) {
      if (withs.startsWith(exp, c)) {
        try {
          var obj = json2.JSON.parse(strings.vslice(exp, '(', ')'))
        } catch(err) {
           throw new Error(err+'. String to be parsed was || '+strings.vslice(exp, '(', ')')+'  ||');
        }
        var result = cases[c](_document, parent, obj);
      }
    }

    if (!result) {
      if ( withs.startsWith(exp, '{') ) {
        try {
          var obj = json2.JSON.parse(exp)
        } catch(err) {
          throw new Error(err+'. String to be parsed was || '+exp+' ||');
        }

        if (cases == aCases) {
          var result = _anonByAttrib(_document, parent, obj)
        } else {
          var result = _byAttrib(parent, obj)
        }
      }
      if (!result) {
        throw new Error('Expression "'+exp+'" returned null. Anonymous == '+(cases == aCases));
      }
    }

    // Final return
    if (expIndex) {
      // TODO: Check length and raise error
      return result[expIndex];
    } else {
      // TODO: Check length and raise error
      return result;
    }
    // Maybe we should cause an exception here
    return false;
  };
  return expSplit.reduce(reduceLookup);
};
