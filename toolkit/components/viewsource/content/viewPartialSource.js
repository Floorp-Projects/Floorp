// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/Services.jsm");

var gDebug = 0;
var gLineCount = 0;
var gStartTargetLine = 0;
var gEndTargetLine = 0;
var gTargetNode = null;

var gEntityConverter = null;
var gWrapLongLines = false;
const gViewSourceCSS = 'resource://gre-resources/viewsource.css';

function onLoadViewPartialSource()
{
  // check the view_source.wrap_long_lines pref
  // and set the menuitem's checked attribute accordingly
  gWrapLongLines = Services.prefs.getBoolPref("view_source.wrap_long_lines");
  document.getElementById("menu_wrapLongLines").setAttribute("checked", gWrapLongLines);
  document.getElementById("menu_highlightSyntax")
          .setAttribute("checked",
                        Services.prefs.getBoolPref("view_source.syntax_highlight"));

  if (window.arguments[3] == 'selection')
    viewSourceChrome.loadViewSourceFromSelection(window.arguments[2]);
  else
    viewPartialSourceForFragment(window.arguments[2], window.arguments[3]);

  window.content.focus();
}

////////////////////////////////////////////////////////////////////////////////
// special handler for markups such as MathML where reformatting the output is
// helpful
function viewPartialSourceForFragment(node, context)
{
  gTargetNode = node;
  if (gTargetNode && gTargetNode.nodeType == Node.TEXT_NODE)
    gTargetNode = gTargetNode.parentNode;

  // walk up the tree to the top-level element (e.g., <math>, <svg>)
  var topTag;
  if (context == 'mathml')
    topTag = 'math';
  else
    throw 'not reached';
  var topNode = gTargetNode;
  while (topNode && topNode.localName != topTag)
    topNode = topNode.parentNode;
  if (!topNode)
    return;

  // serialize
  var title = gViewSourceBundle.getString("viewMathMLSourceTitle");
  var wrapClass = gWrapLongLines ? ' class="wrap"' : '';
  var source =
    '<!DOCTYPE html>'
  + '<html>'
  + '<head><title>' + title + '</title>'
  + '<link rel="stylesheet" type="text/css" href="' + gViewSourceCSS + '">'
  + '<style type="text/css">'
  + '#target { border: dashed 1px; background-color: lightyellow; }'
  + '</style>'
  + '</head>'
  + '<body id="viewsource"' + wrapClass
  +        ' onload="document.title=\''+title+'\';document.getElementById(\'target\').scrollIntoView(true)">'
  + '<pre>'
  + getOuterMarkup(topNode, 0)
  + '</pre></body></html>'
  ; // end

  // display
  gBrowser.loadURI("data:text/html;charset=utf-8," + encodeURIComponent(source));
}

////////////////////////////////////////////////////////////////////////////////
function getInnerMarkup(node, indent) {
  var str = '';
  for (var i = 0; i < node.childNodes.length; i++) {
    str += getOuterMarkup(node.childNodes.item(i), indent);
  }
  return str;
}

////////////////////////////////////////////////////////////////////////////////
function getOuterMarkup(node, indent) {
  var newline = '';
  var padding = '';
  var str = '';
  if (node == gTargetNode) {
    gStartTargetLine = gLineCount;
    str += '</pre><pre id="target">';
  }

  switch (node.nodeType) {
  case Node.ELEMENT_NODE: // Element
    // to avoid the wide gap problem, '\n' is not emitted on the first
    // line and the lines before & after the <pre id="target">...</pre>
    if (gLineCount > 0 &&
        gLineCount != gStartTargetLine &&
        gLineCount != gEndTargetLine) {
      newline = '\n';
    }
    gLineCount++;
    if (gDebug) {
      newline += gLineCount;
    }
    for (var k = 0; k < indent; k++) {
      padding += ' ';
    }
    str += newline + padding
        +  '&lt;<span class="start-tag">' + node.nodeName + '</span>';
    for (var i = 0; i < node.attributes.length; i++) {
      var attr = node.attributes.item(i);
      if (!gDebug && attr.nodeName.match(/^[-_]moz/)) {
        continue;
      }
      str += ' <span class="attribute-name">'
          +  attr.nodeName
          +  '</span>=<span class="attribute-value">"'
          +  unicodeTOentity(attr.nodeValue)
          +  '"</span>';
    }
    if (!node.hasChildNodes()) {
      str += '/&gt;';
    }
    else {
      str += '&gt;';
      var oldLine = gLineCount;
      str += getInnerMarkup(node, indent + 2);
      if (oldLine == gLineCount) {
        newline = '';
        padding = '';
      }
      else {
        newline = (gLineCount == gEndTargetLine) ? '' : '\n';
        gLineCount++;
        if (gDebug) {
          newline += gLineCount;
        }
      }
      str += newline + padding
          +  '&lt;/<span class="end-tag">' + node.nodeName + '</span>&gt;';
    }
    break;
  case Node.TEXT_NODE: // Text
    var tmp = node.nodeValue;
    tmp = tmp.replace(/(\n|\r|\t)+/g, " ");
    tmp = tmp.replace(/^ +/, "");
    tmp = tmp.replace(/ +$/, "");
    if (tmp.length != 0) {
      str += '<span class="text">' + unicodeTOentity(tmp) + '</span>';
    }
    break;
  default:
    break;
  }

  if (node == gTargetNode) {
    gEndTargetLine = gLineCount;
    str += '</pre><pre>';
  }
  return str;
}

////////////////////////////////////////////////////////////////////////////////
function unicodeTOentity(text)
{
  const charTable = {
    '&': '&amp;<span class="entity">amp;</span>',
    '<': '&amp;<span class="entity">lt;</span>',
    '>': '&amp;<span class="entity">gt;</span>',
    '"': '&amp;<span class="entity">quot;</span>'
  };

  function charTableLookup(letter) {
    return charTable[letter];
  }

  function convertEntity(letter) {
    try {
      var unichar = gEntityConverter.ConvertToEntity(letter, entityVersion);
      var entity = unichar.substring(1); // extract '&'
      return '&amp;<span class="entity">' + entity + '</span>';
    } catch (ex) {
      return letter;
    }
  }

  if (!gEntityConverter) {
    try {
      gEntityConverter =
        Components.classes["@mozilla.org/intl/entityconverter;1"]
                  .createInstance(Components.interfaces.nsIEntityConverter);
    } catch(e) { }
  }

  const entityVersion = Components.interfaces.nsIEntityConverter.entityW3C;

  var str = text;

  // replace chars in our charTable
  str = str.replace(/[<>&"]/g, charTableLookup);

  // replace chars > 0x7f via nsIEntityConverter
  str = str.replace(/[^\0-\u007f]/g, convertEntity);

  return str;
}
