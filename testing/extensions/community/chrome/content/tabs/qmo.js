/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla Community QA Extension
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Zach Lipton <zach@zachlipton.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
* ***** END LICENSE BLOCK ***** */

var qmo = {
  populateFields : function() {
    qmo.populateForumPosts();
    qmo.populateEvents();
    qmo.populateQMONews();
    qmo.populateHowHelp();
  },
  populateForumPosts : function() {
    var numPosts = 5; // show top 5 topics
    var postBox = $('qa-qmo-forumposts');
    qaTools.showHideLoadingMessage(postBox, true);
    var url = qaMain.urlbundle.getString("qa.extension.url.qmo.forum_topics");
    var callback = function(feed) {
      var items = feed.items;
      qaTools.showHideLoadingMessage(postBox, false);
      if (items.length < numPosts)
        numPosts=items.length;
      for (var i=0; i<numPosts; i++) {
        var item = items.queryElementAt(i, Ci.nsIFeedEntry);
        if (item != null) {
          qmo.populateLinkBox(postBox,
            [{text : item.title.plainText(), url : item.link.resolve("")}],
            64);
        }
      }
    };
    qaTools.fetchFeed(url, callback);
  },
  populateEvents : function() {
    var numEvents = 5; // show 5 events
    var eventBox = $('qa-qmo-events');
    qaTools.showHideLoadingMessage(eventBox, true);
    var url = qaMain.urlbundle.getString("qa.extension.url.qmo.upcomingEvents");
    var callback = function(feed) {
      var items = feed.items;
      qaTools.showHideLoadingMessage(eventBox, false);
      if (items.length < numEvents)
        numEvents=items.length;
      for (var i=0; i<numEvents; i++) {
        var item = items.queryElementAt(i, Ci.nsIFeedEntry);
        if (item != null) {
          qmo.populateLinkBox(eventBox,
            [{text : item.title.plainText(), url : item.link.resolve("")}],
            37);
        }
      }
    };
    qaTools.fetchFeed(url, callback);
  },
  populateQMONews : function() {
    var box = $('qa-qmo-latest');
    var url = qaMain.urlbundle.getString("qa.extension.url.qmo.news");
    qaTools.showHideLoadingMessage(box, true);
    var callback = function(feed) {
      var items = feed.items;
      var item = items.queryElementAt(0, Ci.nsIFeedEntry);
      qaTools.showHideLoadingMessage(box, false);
      if (item != null) { // just grab the first item
        var content = item.summary;
        var fragment = content.createDocumentFragment(box);

        box.appendChild(fragment);
      }
      qaTools.assignLinkHandlers(box);
    }
    qaTools.fetchFeed(url, callback);
  },
  populateHowHelp : function() {
    var box = $('qa-qmo-help');
    var url = qaMain.urlbundle.getString("qa.extension.url.qmo.howhelp");
    qaTools.showHideLoadingMessage(box, true);
    var callback = function(feed) {
      var items = feed.items;
      var item = items.queryElementAt(0, Ci.nsIFeedEntry);
      qaTools.showHideLoadingMessage(box, false);
      if (item != null) { // just grab the first item
        var content = item.summary;
        var fragment = content.createDocumentFragment(box);

        box.appendChild(fragment);
      }
      qaTools.assignLinkHandlers(box);
    }
    qaTools.fetchFeed(url, callback);
  },
  populateLinkBox : function(box, links, chars) {
    var list = box.childNodes[1];
    for (var i=0; i<links.length; i++) {
      var elem = list.appendChild(document.createElementNS(qaMain.htmlNS,"li"));
      var a = elem.appendChild(document.createElementNS(qaMain.htmlNS,"a"));

      // limit text to chars characters:
      var text = links[i].text;
      if (chars && chars>0 && text.length > chars) {
        a.setAttribute("tooltiptext", text);
        text = text.substring(0, chars-3)+"...";
      }

      a.textContent = text;
      a.href = links[i].url;
      qaTools.assignLinkHandler(a);
    }
  }
};

function $() {
  var elements = new Array();
  for (var i = 0; i < arguments.length; i++) {
    var element = arguments[i];
    if (typeof element == 'string')
      element = document.getElementById(element);
    if (arguments.length == 1)
      return element;

    elements.push(element);
  }
  return elements;
}
