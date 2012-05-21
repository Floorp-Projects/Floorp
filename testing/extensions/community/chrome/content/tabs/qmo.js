/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
            [{text : item.title.plainText(), url : item.link.resolve("")}]);
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
            [{text : item.title.plainText(), url : item.link.resolve("")}]);
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
