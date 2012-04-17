/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["inspectElement"]

var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var mozmill = {}; Components.utils.import('resource://mozmill/modules/mozmill.js', mozmill);
var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);

var arrays = {}; Components.utils.import('resource://mozmill/stdlib/arrays.js', arrays);
var dom = {}; Components.utils.import('resource://mozmill/stdlib/dom.js', dom);
var objects = {}; Components.utils.import('resource://mozmill/stdlib/objects.js', objects);
var json2 = {}; Components.utils.import('resource://mozmill/stdlib/json2.js', json2);
var withs = {}; Components.utils.import('resource://mozmill/stdlib/withs.js', withs);

var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
           .getService(Components.interfaces.nsIWindowMediator);

var isNotAnonymous = function (elem, result) {
  if (result == undefined) {
    var result = true;
  }
  if ( elem.parentNode ) {
    var p = elem.parentNode;
    return isNotAnonymous(p, result == arrays.inArray(p.childNodes, elem) == true);
  } else {
    return result;
  }
}

var elemIsAnonymous = function (elem) {
  if (elem.getAttribute('anonid') || !arrays.inArray(elem.parentNode.childNodes, elem)) {
    return true;
  }
  return false;
}

var getXPath = function (node, path) {
  path = path || [];

  if(node.parentNode) {
    path = getXPath(node.parentNode, path);
  }

  if(node.previousSibling) {
    var count = 1;
    var sibling = node.previousSibling
    do {
      if(sibling.nodeType == 1 && sibling.nodeName == node.nodeName) {count++;}
      sibling = sibling.previousSibling;
    } while(sibling);
    if(count == 1) {count = null;}
  } else if(node.nextSibling) {
    var sibling = node.nextSibling;
    do {
      if(sibling.nodeType == 1 && sibling.nodeName == node.nodeName) {
        var count = 1;
        sibling = null;
      } else {
        var count = null;
        sibling = sibling.previousSibling;
      }
    } while(sibling);
  }

  if(node.nodeType == 1) {
 //   if ($('absXpaths').checked){
      path.push(node.nodeName.toLowerCase() + (node.id ? "[@id='"+node.id+"']" : count > 0 ? "["+count+"]" : ''));
  //  }
  //  else{
  //    path.push(node.nodeName.toLowerCase() + (node.id ? "" : count > 0 ? "["+count+"]" : ''));
  //  }
  }
  return path;
};

function getXSPath(node){
  var xpArray = getXPath(node);
  var stringXpath = xpArray.join('/');
  stringXpath = '/'+stringXpath;
  stringXpath = stringXpath.replace('//','/');
  return stringXpath;
}
function getXULXpath (el, xml) {
	var xpath = '';
	var pos, tempitem2;

	while(el !== xml.documentElement) {		
		pos = 0;
		tempitem2 = el;
		while(tempitem2) {
			if (tempitem2.nodeType === 1 && tempitem2.nodeName === el.nodeName) {
			  // If it is ELEMENT_NODE of the same name
				pos += 1;
			}
			tempitem2 = tempitem2.previousSibling;
		}

		xpath = "*[name()='"+el.nodeName+"' and namespace-uri()='"+(el.namespaceURI===null?'':el.namespaceURI)+"']["+pos+']'+'/'+xpath;

		el = el.parentNode;
	}
	xpath = '/*'+"[name()='"+xml.documentElement.nodeName+"' and namespace-uri()='"+(el.namespaceURI===null?'':el.namespaceURI)+"']"+'/'+xpath;
	xpath = xpath.replace(/\/$/, '');
	return xpath;
}

var getDocument = function (elem) {
  while (elem.parentNode) {
    var elem = elem.parentNode;
  }
  return elem;
}

var getTopWindow = function(doc) {
  return utils.getChromeWindow(doc.defaultView);
}

var attributeToIgnore = ['focus', 'focused', 'selected', 'select', 'flex', // General Omissions
                         'linkedpanel', 'last-tab', 'afterselected', // From Tabs UI, thanks Farhad
                         'style', // Gets set dynamically all the time, also effected by dx display code
                         ];

var getUniqueAttributesReduction = function (attributes, node) {
  for (var i in attributes) {
    if ( node.getAttribute(i) == attributes[i] || arrays.inArray(attributeToIgnore, i) || arrays.inArray(attributeToIgnore, attributes[i]) || i == 'id') {
      delete attributes[i];
    }
  }
  return attributes;
}

var getLookupExpression = function (_document, elem) {
  expArray = [];
  while ( elem.parentNode ) {
    var exp = getLookupForElem(_document, elem);
    expArray.push(exp);
    var elem = elem.parentNode;
  }
  expArray.reverse();
  return '/' + expArray.join('/');
}

var getLookupForElem = function (_document, elem) {
  if ( !elemIsAnonymous(elem) ) {
    if (elem.id != "" && !withs.startsWith(elem.id, 'panel')) {
      identifier = {'name':'id', 'value':elem.id};
    } else if ((elem.name != "") && (typeof(elem.name) != "undefined")) {
      identifier = {'name':'name', 'value':elem.name};
    } else {
      identifier = null;
    }

    if (identifier) {
      var result = {'id':elementslib._byID, 'name':elementslib._byName}[identifier.name](_document, elem.parentNode, identifier.value);
      if ( typeof(result != 'array') ) {
        return identifier.name+'('+json2.JSON.stringify(identifier.value)+')';
      }
    }

    // At this point there is either no identifier or it returns multiple
    var parse = [n for each (n in elem.parentNode.childNodes) if
                 (n.getAttribute && n != elem)
                 ];
    parse.unshift(dom.getAttributes(elem));
    var uniqueAttributes = parse.reduce(getUniqueAttributesReduction);

    if (!result) {
      var result = elementslib._byAttrib(elem.parentNode, uniqueAttributes);
    }

    if (!identifier && typeof(result) == 'array' ) {
      return json2.JSON.stringify(uniqueAttributes) + '['+arrays.indexOf(result, elem)+']'
    } else {
      var aresult = elementslib._byAttrib(elem.parentNode, uniqueAttributes);
      if ( typeof(aresult != 'array') ) {
        if (objects.getLength(uniqueAttributes) == 0) {
          return '['+arrays.indexOf(elem.parentNode.childNodes, elem)+']'
        }
        return json2.JSON.stringify(uniqueAttributes)
      } else if ( result.length > aresult.length ) {
        return json2.JSON.stringify(uniqueAttributes) + '['+arrays.indexOf(aresult, elem)+']'
      } else {
        return identifier.name+'('+json2.JSON.stringify(identifier.value)+')' + '['+arrays.indexOf(result, elem)+']'
      }
    }

  } else {
    // Handle Anonymous Nodes
    var parse = [n for each (n in _document.getAnonymousNodes(elem.parentNode)) if
                 (n.getAttribute && n != elem)
                 ];
    parse.unshift(dom.getAttributes(elem));
    var uniqueAttributes = parse.reduce(getUniqueAttributesReduction);
    if (uniqueAttributes.anonid && typeof(elementslib._byAnonAttrib(_document,
        elem.parentNode, {'anonid':uniqueAttributes.anonid})) != 'array') {
      uniqueAttributes = {'anonid':uniqueAttributes.anonid};
    }

    if (objects.getLength(uniqueAttributes) == 0) {
      return 'anon(['+arrays.indexOf(_document.getAnonymousNodes(elem.parentNode), elem)+'])';
    } else if (arrays.inArray(uniqueAttributes, 'anonid')) {
      return 'anon({"anonid":"'+uniqueAttributes['anonid']+'"})';
    } else {
      return 'anon('+json2.JSON.stringify(uniqueAttributes)+')';
    }

  }
  return 'broken '+elemIsAnonymous(elem)
}

var removeHTMLTags = function(str){
 	 	str = str.replace(/&(lt|gt);/g, function (strMatch, p1){
 		 	return (p1 == "lt")? "<" : ">";
 		});
 		var strTagStrippedText = str.replace(/<\/?[^>]+(>|$)/g, "");
 		strTagStrippedText = strTagStrippedText.replace(/&nbsp;/g,"");
	return strTagStrippedText;
}

var isMagicAnonymousDiv = function (_document, node) {
  if (node.getAttribute && node.getAttribute('class') == 'anonymous-div') {
    if (!arrays.inArray(node.parentNode.childNodes, node) && (_document.getAnonymousNodes(node) == null ||
        !arrays.inArray(_document.getAnonymousNodes(node), node) ) ) {
          return true;
        }
  }
  return false;
}

var copyToClipboard = function(str){
  const gClipboardHelper = Components.classes["@mozilla.org/widget/clipboardhelper;1"] .getService(Components.interfaces.nsIClipboardHelper);
  gClipboardHelper.copyString(str, _window.document);
}

var getControllerAndDocument = function (_document, _window) {
  var windowtype = _window.document.documentElement.getAttribute('windowtype');
  var controllerString, documentString, activeTab;

  // TODO replace with object based cases
  switch(windowtype) {
    case 'navigator:browser':
      controllerString = 'mozmill.getBrowserController()';
      activeTab = mozmill.getBrowserController().tabs.activeTab;
      break;
    case 'Browser:Preferences':
      controllerString = 'mozmill.getPreferencesController()';
      break;
    case 'Extension:Manager':
      controllerString = 'mozmill.getAddonsController()';
      break;
    default:
      if(windowtype)
        controllerString = 'new mozmill.controller.MozMillController(mozmill.utils.getWindowByType("' + windowtype + '"))';
      else if(_window.document.title)
        controllerString = 'new mozmill.controller.MozMillController(mozmill.utils.getWindowByTitle("'+_window.document.title+'"))';
      else
        controllerString = 'Cannot find window';
      break;
  }

  if(activeTab == _document) {
    documentString = 'controller.tabs.activeTab';
  } else if(activeTab == _document.defaultView.top.document) {
    // if this document is from an iframe in the active tab
    var stub = getDocumentStub(_document, activeTab.defaultView);
    documentString = 'controller.tabs.activeTab.defaultView' + stub;
  } else {
    var stub = getDocumentStub(_document, _window);
    if(stub)
      documentString = 'controller.window' + stub;
    else
      documentString = 'Cannot find document';
  }
  return {'controllerString':controllerString, 'documentString':documentString}
}

getDocumentStub = function( _document, _window) {
  if(_window.document == _document)
    return '.document';
  for(var i = 0; i < _window.frames.length; i++) {
    var stub = getDocumentStub(_document, _window.frames[i]);
    if (stub)
      return '.frames['+i+']' + stub;
  }
  return '';
}

var inspectElement = function(e){
  if (e.originalTarget != undefined) {
    target = e.originalTarget;
  } else {
    target = e.target;
  }

  //Element highlighting
  try {
    if (this.lastEvent)
      this.lastEvent.target.style.outline = "";
  } catch(err) {}

  this.lastEvent = e;

  try {
     e.target.style.outline = "1px solid darkblue";
  } catch(err){}

  var _document = getDocument(target);


  if (isMagicAnonymousDiv(_document, target)) {
    target = target.parentNode;
  }

  var windowtype = _document.documentElement.getAttribute('windowtype');
  var _window = getTopWindow(_document);
  r = getControllerAndDocument(_document, _window);

  // displayText = "Controller: " + r.controllerString + '\n\n';
  if ( isNotAnonymous(target) ) {
    // Logic for which identifier to use is duplicated above
    if (target.id != "" && !withs.startsWith(target.id, 'panel')) {
      elemText = "new elementslib.ID("+ r.documentString + ', "' + target.id + '")';
      var telem = new elementslib.ID(_document, target.id);
    } else if ((target.name != "") && (typeof(target.name) != "undefined")) {
      elemText = "new elementslib.Name("+ r.documentString + ', "' + target.name + '")';
      var telem = new elementslib.Name(_document, target.name);
    } else if (target.nodeName == "A") {
      var linkText = removeHTMLTags(target.innerHTML);
      elemText = "new elementslib.Link("+ r.documentString + ', "' + linkText + '")';
      var telem = new elementslib.Link(_document, linkText);
    }
  }
  // Fallback on XPath
  if (telem == undefined || telem.getNode() != target) {
    if (windowtype == null) {
      var stringXpath = getXSPath(target);
    } else {
      var stringXpath = getXULXpath(target, _document);
    }
    var telem = new elementslib.XPath(_document, stringXpath);
    if ( telem.getNode() == target ) {
      elemText = "new elementslib.XPath("+ r.documentString + ', "' + stringXpath + '")';
    }
  }
  // Fallback to Lookup
  if (telem == undefined || telem.getNode() != target) {
    var exp = getLookupExpression(_document, target);
    elemText = "new elementslib.Lookup("+ r.documentString + ", '" + exp + "')";
    var telem = new elementslib.Lookup(_document, exp);
  }

  return {'validation':( target == telem.getNode() ),
          'elementText':elemText,
          'elementType':telem.constructor.name,
          'controllerText':r.controllerString,
          'documentString':r.documentString,
          }
}



