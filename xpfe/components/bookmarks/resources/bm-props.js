/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var NC_NAMESPACE_URI = "http://home.netscape.com/NC-rdf#";

// XXX MAKE SURE that the "url" field is LAST!
// This is important for what happens if/when the URL itself is changed.
// Ask rjc@netscape.com if you want to know why exactly this is.

// This is the set of fields that are visible in the window.
var gFields     = ["name", "shortcut", "description", "url"];

// ...and this is a parallel array that contains the RDF properties
// that they are associated with.
var gProperties = [NC_NAMESPACE_URI + "Name",
                   NC_NAMESPACE_URI + "ShortcutURL",
                   NC_NAMESPACE_URI + "Description",
                   NC_NAMESPACE_URI + "URL"];

var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"]
                    .getService(Components.interfaces.nsIRDFService);

var RDFC = Components.classes["@mozilla.org/rdf/container-utils;1"]
                     .getService(Components.interfaces.nsIRDFContainerUtils);

var Bookmarks = RDF.GetDataSource("rdf:bookmarks");

var gBookmarkURL = "";

function Init()
{
  var x;
  gBookmarkURL = window.arguments[0];

  // set up action buttons
  doSetOKCancel(Commit);

  // Initialize the properties panel by copying the values from the
  // RDF graph into the fields on screen.

  for (var i = 0; i < gFields.length; ++i) {
    var field = document.getElementById(gFields[i]);

    var value = Bookmarks.GetTarget(RDF.GetResource(gBookmarkURL),
                                    RDF.GetResource(gProperties[i]),
                                    true);

    if (value)
      value = value.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

    if (value) //make sure were aren't stuffing null into any fields
      field.value = value;
  }

  var propsWindow = document.getElementById("bmPropsWindow");
  var nameNode = document.getElementById("name");
  var title = propsWindow.getAttribute("title");
  title = title.replace(/\*\*bm_title\*\*/gi, nameNode.value);
  propsWindow.setAttribute("title", title);

  // check bookmark schedule
  value = Bookmarks.GetTarget(RDF.GetResource(gBookmarkURL),
                              RDF.GetResource("http://home.netscape.com/WEB-rdf#Schedule"),
                              true);

  if (value) {
    value = value.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

    if (value) {
      var values = value.split("|");
      if (values.length == 4) {
        // get day range
        var days = values[0];
        var dayNode = document.getElementById("dayRange");
        var dayItems = dayNode.childNodes[0].childNodes;
        for (x=0; x < dayItems.length; ++x) {
          if (dayItems[x].getAttribute("value") == days) {
            dayNode.selectedItem = dayItems[x];
            break;
          }
        }

        // get hour range
        var hours = values[1].split("-");
        var startHour = "";
        var endHour = "";

        if (hours.length == 2) {
          startHour = hours[0];
          endHour = hours[1];
        }

        // set start hour
        var startHourNode = document.getElementById("startHourRange");
        var startHourItems = startHourNode.childNodes[0].childNodes;
        for (x=0; x < startHourItems.length; ++x) {
          if (startHourItems[x].getAttribute("value") == startHour) {
            startHourNode.selectedItem = startHourItems[x];
            break;
          }
        }

        // set end hour
        var endHourNode = document.getElementById("endHourRange");
        var endHourItems = endHourNode.childNodes[0].childNodes;
        for (x=0; x < endHourItems.length; ++x) {
          if (endHourItems[x].getAttribute("value") == endHour) {
            endHourNode.selectedItem = endHourItems[x];
            break;
          }
        }

        // get duration
        var duration = values[2];
        var durationNode = document.getElementById("duration");
        durationNode.value = duration;

        // get notification method
        var method = values[3];
        if (method.indexOf("icon") >= 0)
          document.getElementById("bookmarkIcon").checked = true;

        if (method.indexOf("sound") >= 0)
          document.getElementById("playSound").checked = true;

        if (method.indexOf("alert") >= 0)
          document.getElementById("showAlert").checked = true;

        if (method.indexOf("open") >= 0)
          document.getElementById("openWindow").checked = true;
      }
    }
  }

  // if its a container, disable some things
  var isContainerFlag = RDFC.IsContainer(Bookmarks, RDF.GetResource(gBookmarkURL));
  if (!isContainerFlag) {
    // XXX To do: the "RDFC.IsContainer" call above only works for RDF sequences;
    //            if its not a RDF sequence, we should to more checking to see if
    //            the item in question is really a container of not.  A good example
    //            of this is the "File System" container.
  }

  if (isContainerFlag) {
    // If it is a folder, it has no URL or Keyword
    document.getElementById("locationrow").setAttribute("hidden", "true");
    document.getElementById("shortcutrow").setAttribute("hidden", "true");
  }

  if (gBookmarkURL.substr(0, 7).toLowerCase() != "http://" &&
      gBookmarkURL.substr(0, 8).toLowerCase() != "https://") {
    // only allow scheduling of http/https URLs
    document.getElementById("ScheduleTab").setAttribute("hidden", "true");
    document.getElementById("NotifyTab").setAttribute("hidden", "true");
  }

  sizeToContent();
  
  // Set up the enabled of controls on the scheduling panels
  dayRangeChange(document.getElementById("dayRange"));

  // set initial focus
  var name = document.getElementById("name");
  name.focus();
  name.select();
}



function Commit()
{
  var changed = false;

  // Grovel through the fields to see if any of the values have
  // changed. If so, update the RDF graph and force them to be saved
  // to disk.
  for (var i = 0; i < gFields.length; ++i) {
    var field = document.getElementById(gFields[i]);

    if (field) {
      // Get the new value as a literal, using 'null' if the value is empty.
      var newvalue = field.value;

      var oldvalue = Bookmarks.GetTarget(RDF.GetResource(gBookmarkURL),
                                         RDF.GetResource(gProperties[i]),
                                         true);

      if (oldvalue)
        oldvalue = oldvalue.QueryInterface(Components.interfaces.nsIRDFLiteral);

      if (newvalue && gProperties[i] == (NC_NAMESPACE_URI + "ShortcutURL")) {
        // shortcuts are always lowercased internally
        newvalue = newvalue.toLowerCase();
      }
      else if (newvalue && gProperties[i] == (NC_NAMESPACE_URI + "URL")) {
        // we're dealing with the URL attribute;
        // if a scheme isn't specified, use "http://"
        if (newvalue.indexOf(":") < 0)
          newvalue = "http://" + newvalue;
      }

      if (newvalue)
        newvalue = RDF.GetLiteral(newvalue);

      if (updateAttribute(gProperties[i], oldvalue, newvalue)) {
        // Update gBookmarkURL if the url changed
        if (newvalue && gProperties[i] == NC_NAMESPACE_URI + "URL")
          gBookmarkURL = newvalue.Value;

        changed = true;
      }
    }
  }

  // Update bookmark schedule if necessary;
  // if the tab was removed, just skip it
  var scheduleTab = document.getElementById("ScheduleTab");
  if (scheduleTab) {
    var scheduleRes = "http://home.netscape.com/WEB-rdf#Schedule";
    oldvalue = Bookmarks.GetTarget(RDF.GetResource(gBookmarkURL),
                                   RDF.GetResource(scheduleRes), true);
    newvalue = "";
    var dayRangeNode = document.getElementById("dayRange");
    var dayRange = dayRangeNode.selectedItem.getAttribute("value");

    if (dayRange) {
      var startHourRangeNode = document.getElementById("startHourRange");
      var startHourRange = startHourRangeNode.selectedItem.getAttribute("value");

      var endHourRangeNode = document.getElementById("endHourRange");
      var endHourRange = endHourRangeNode.selectedItem.getAttribute("value");

      if (parseInt(startHourRange) > parseInt(endHourRange)) {
        var temp = startHourRange;
        startHourRange = endHourRange;
        endHourRange = temp;
      }

      var bookmarkBundle;
      var duration = document.getElementById("duration").value;
      if (!duration) {
        bookmarkBundle = document.getElementById("bundle_bookmark");
        alert (bookmarkBundle.getString("pleaseEnterADuration"));
        return false;
      }

      var methods = [];
      if (document.getElementById("bookmarkIcon").checked)
        methods.push("icon");
      if (document.getElementById("playSound").checked)
        methods.push("sound");
      if (document.getElementById("showAlert").checked)
        methods.push("alert");
      if (document.getElementById("openWindow").checked)
        methods.push("open");

      if (methods.length == 0) {
        bookmarkBundle = document.getElementById("bundle_bookmark");
        alert (bookmarkBundle.getString("pleaseSelectANotification"));
        return false;
      }

      var method = methods.join(); // join string in array with ","

      newvalue = dayRange + "|" + startHourRange + "-" + endHourRange + "|" + duration + "|" + method;
    }

    if (newvalue)
      newvalue = RDF.GetLiteral(newvalue);

    if (updateAttribute(scheduleRes, oldvalue, newvalue))
      changed = true;
  }

  if (changed) {
    var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    if (remote)
      remote.Flush();
  }

  window.close();
  return true;
}

function updateAttribute(prop, oldvalue, newvalue)
{
  var changed = false;

  if (prop && (oldvalue || newvalue) && oldvalue != newvalue) {

    if (oldvalue && !newvalue) {
      Bookmarks.Unassert(RDF.GetResource(gBookmarkURL),
                         RDF.GetResource(prop),
                         oldvalue);
    }
    else if (!oldvalue && newvalue) {
      Bookmarks.Assert(RDF.GetResource(gBookmarkURL),
                       RDF.GetResource(prop),
                       newvalue,
                       true);
    }
    else /* if (oldvalue && newvalue) */ {
      Bookmarks.Change(RDF.GetResource(gBookmarkURL),
                       RDF.GetResource(prop),
                       oldvalue,
                       newvalue);
    }

    changed = true;
  }

  return changed;
}

function setEndHourRange()
{
  // Get the values of the start-time and end-time as ints
  var startHourRangeNode = document.getElementById("startHourRange");
  var startHourRange = startHourRangeNode.selectedItem.getAttribute("value");
  var startHourRangeInt = parseInt(startHourRange);

  var endHourRangeNode = document.getElementById("endHourRange");
  var endHourRange = endHourRangeNode.selectedItem.getAttribute("value");
  var endHourRangeInt = parseInt(endHourRange);

  var endHourItemNode = endHourRangeNode.firstChild.firstChild;

  var index = 0;

  // disable all those end-times before the start-time
  for (; index < startHourRangeInt; ++index) {
    endHourItemNode.setAttribute("disabled", "true");
    endHourItemNode = endHourItemNode.nextSibling;
  }

  // update the selected value if it's out of the allowed range
  if (startHourRangeInt >= endHourRangeInt)
    endHourRangeNode.selectedItem = endHourItemNode;

  // make sure all the end-times after the start-time are enabled
  for (; index < 24; ++index) {
    endHourItemNode.removeAttribute("disabled");
    endHourItemNode = endHourItemNode.nextSibling;
  }
}

function dayRangeChange (aMenuList)
{
  var controls = ["startHourRange", "endHourRange", "duration", "bookmarkIcon", 
                  "showAlert", "openWindow", "playSound", "durationSubLabel", 
                  "durationLabel", "startHourRangeLabel", "endHourRangeLabel"];
  for (var i = 0; i < controls.length; ++i)
    document.getElementById(controls[i]).disabled = !aMenuList.value;
}


