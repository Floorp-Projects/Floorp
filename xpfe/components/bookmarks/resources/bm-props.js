/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// This is the set of fields that are visible in the window.
var gFields;

// ...and this is a parallel array that contains the RDF properties
// that they are associated with.
var gProperties;

// The ID of the current shown bookmark
var gBookmarkID;

function Init()
{
  initServices();
  initBMService();

  // This is the set of fields that are visible in the window.
  gFields     = ["name", "url", "shortcut", "description"];

  // ...and this is a parallel array that contains the RDF properties
  // that they are associated with.
  gProperties = [NC_NS + "Name",
                 NC_NS + "URL",
                 NC_NS + "ShortcutURL",
                 NC_NS + "Description"];

  gBookmarkID = window.arguments[0];

  var i;
  var resource = RDF.GetResource(gBookmarkID);
  // Initialize the properties panel by copying the values from the
  // RDF graph into the fields on screen.

  for (i = 0; i < gFields.length; ++i) {
    var field = document.getElementById(gFields[i]);

    var value = BMDS.GetTarget(resource, RDF.GetResource(gProperties[i]), true);

    if (value)
      value = value.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;

    if (value) //make sure were aren't stuffing null into any fields
      field.value = value;
  }

  var nameNode = document.getElementById("name");
  document.title = document.title.replace(/\*\*bm_title\*\*/gi, nameNode.value);

  // if its a container, disable some things
  var isContainerFlag = RDFCU.IsContainer(BMDS, resource);
  if (!isContainerFlag) {
    // XXX To do: the "RDFCU.IsContainer" call above only works for RDF sequences;
    //            if its not a RDF sequence, we should to more checking to see if
    //            the item in question is really a container of not.  A good example
    //            of this is the "File System" container.
  }

  var isSeparator = BookmarksUtils.resolveType(resource) == "BookmarkSeparator";

  if (isContainerFlag || isSeparator) {
    // If it is a folder, it has no URL or Keyword
    document.getElementById("locationrow").setAttribute("hidden", "true");
    document.getElementById("shortcutrow").setAttribute("hidden", "true");
    if (isSeparator) {
      document.getElementById("descriptionrow").setAttribute("hidden", "true");
    }
  }

  var showScheduling = false;
  var url = BMDS.GetTarget(resource, RDF.GetResource(gProperties[1]), true);
  if (url) {
    url = url.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
    if (url.substr(0, 7).toLowerCase() == "http://" ||
        url.substr(0, 8).toLowerCase() == "https://") {
      showScheduling = true;
    }
  }

  if (!showScheduling) {
    // only allow scheduling of http/https URLs
    document.getElementById("scheduling").setAttribute("hidden", "true");
  } else {
    // check bookmark schedule
    var scheduleArc = RDF.GetResource("http://home.netscape.com/WEB-rdf#Schedule");
    value = BMDS.GetTarget(resource, scheduleArc, true);
  
    if (value) {
      value = value.QueryInterface(Components.interfaces.nsIRDFLiteral).Value;
  
      if (value) {
        var values = value.split("|");
        if (values.length == 4) {
          // get day range
          var days = values[0];
          var dayNode = document.getElementById("dayRange");
          var dayItems = dayNode.childNodes[0].childNodes;
          for (i=0; i < dayItems.length; ++i) {
            if (dayItems[i].getAttribute("value") == days) {
              dayNode.selectedItem = dayItems[i];
              break;
            }
          }
          // Set up the enabled of controls on the scheduling panels
          dayRangeChange(dayNode);
  
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
          for (i=0; i < startHourItems.length; ++i) {
            if (startHourItems[i].getAttribute("value") == startHour) {
              startHourNode.selectedItem = startHourItems[i];
              break;
            }
          }
  
          // set end hour
          var endHourNode = document.getElementById("endHourRange");
          var endHourItems = endHourNode.childNodes[0].childNodes;
          for (i=0; i < endHourItems.length; ++i) {
            if (endHourItems[i].getAttribute("value") == endHour) {
              endHourNode.selectedItem = endHourItems[i];
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
  }

  sizeToContent();
  
  // set initial focus
  nameNode.focus();
  nameNode.select();

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

      var oldvalue = BMDS.GetTarget(RDF.GetResource(gBookmarkID),
                                    RDF.GetResource(gProperties[i]), true);

      if (oldvalue)
        oldvalue = oldvalue.QueryInterface(Components.interfaces.nsIRDFLiteral);

      if (newvalue && gProperties[i] == (NC_NS + "ShortcutURL")) {
        // shortcuts are always lowercased internally
        newvalue = newvalue.toLowerCase();
      }
      else if (newvalue && gProperties[i] == (NC_NS + "URL")) {
        // we're dealing with the URL attribute;
        // if a scheme isn't specified, use "http://"
        if (newvalue.indexOf(":") < 0)
          newvalue = "http://" + newvalue;
      }

      if (newvalue)
        newvalue = RDF.GetLiteral(newvalue);

      if (updateAttribute(gProperties[i], oldvalue, newvalue)) {
        changed = true;
      }
    }
  }

  // Update bookmark schedule if necessary;
  // if the tab was removed, just skip it
  var scheduling = document.getElementById("scheduling");
  var schedulingHidden = scheduling.getAttribute("hidden");
  if (schedulingHidden != "true") {
    var scheduleRes = "http://home.netscape.com/WEB-rdf#Schedule";
    oldvalue = BMDS.GetTarget(RDF.GetResource(gBookmarkID),
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

      var duration = document.getElementById("duration").value;
      if (!duration) {
        alert(BookmarksUtils.getLocaleString("pleaseEnterADuration"));
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
        alert(BookmarksUtils.getLocaleString("pleaseSelectANotification"));
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
    var remote = BMDS.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
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
      BMDS.Unassert(RDF.GetResource(gBookmarkID), 
                    RDF.GetResource(prop), oldvalue);
    }
    else if (!oldvalue && newvalue) {
      BMDS.Assert(RDF.GetResource(gBookmarkID),
                  RDF.GetResource(prop), newvalue, true);
    }
    else /* if (oldvalue && newvalue) */ {
      BMDS.Change(RDF.GetResource(gBookmarkID), 
                  RDF.GetResource(prop), oldvalue, newvalue);
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
