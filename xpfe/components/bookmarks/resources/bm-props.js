/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

var NC_NAMESPACE_URI = "http://home.netscape.com/NC-rdf#";

// XXX MAKE SURE that the "url" field is LAST!
// This is important for what happens if/when the URL itself is changed.
// Ask rjc@netscape.com if you want to know why exactly this is.

// This is the set of fields that are visible in the window.
var Fields     = ["name", "shortcut", "description", "url"];

// ...and this is a parallel array that contains the RDF properties
// that they are associated with.
var Properties = [NC_NAMESPACE_URI + "Name",
                  NC_NAMESPACE_URI + "ShortcutURL",
                  NC_NAMESPACE_URI + "Description",
                  NC_NAMESPACE_URI + "URL"];

var RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var RDFC = Components.classes["@mozilla.org/rdf/container-utils;1"].getService();
RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainerUtils);

var Bookmarks = RDF.GetDataSource("rdf:bookmarks");

// Init() will fill this in.
var bookmark_url = '';



function Init()
{
  bookmark_url = window.arguments[0];

  // set up action buttons
  doSetOKCancel(Commit);

  // Initialize the properties panel by copying the values from the
  // RDF graph into the fields on screen.

  for (var i = 0; i < Fields.length; ++i) {
    var field = document.getElementById(Fields[i]);

    var value = Bookmarks.GetTarget(RDF.GetResource(bookmark_url),
                                    RDF.GetResource(Properties[i]),
                                    true);

    if (value) value = value.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (value) value = value.Value;

    dump("field '" + Fields[i] + "' <== '" + value + "'\n");

    if (value) field.value = value;
  }

/*
	// try and set window title
	var nameNode = document.getElementById("name");
	if (nameNode)
	{
		var name = nameNode.value;
		if (name && name != "")
		{
			dump("\n    Set window name to '" + name + "'\n");
			window.title = name;
		}
	}
*/

  // check bookmark schedule
    var value = Bookmarks.GetTarget(RDF.GetResource(bookmark_url),
                                    RDF.GetResource("http://home.netscape.com/WEB-rdf#Schedule"),
                                    true);

    if (value) value = value.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (value) value = value.Value;
    if ((value) && (value != ""))
    {
	var sep;
	
	// get day range
	if ((sep = value.indexOf("|")) > 0)
	{
		var days = value.substr(0, sep);
		value = value.substr(sep+1, value.length-1);

		var dayNode = document.getElementById("dayRange");
		for (var x=0; x < dayNode.childNodes[0].childNodes.length; x++)
		{
			if (dayNode.childNodes[0].childNodes[x].getAttribute("data") == days)
			{
				dayNode.selectedItem = dayNode.childNodes[0].childNodes[x];
				break;
			}
		}
	}

	// get hour range
	if ((sep = value.indexOf("|")) > 0)
	{
		var hours = value.substr(0, sep);
		value = value.substr(sep+1, value.length-1);

		var startHour = "";
		var endHour = "";

		var dashSep = hours.indexOf("-");
		if (dashSep > 0)
		{
			startHour = hours.substr(0, dashSep);
			endHour = hours.substr(dashSep + 1, hours.length-1);
		}

		// set start hour
		var startHourNode = document.getElementById("startHourRange");
		for (var x=0; x < startHourNode.childNodes[0].childNodes.length; x++)
		{
			if (startHourNode.childNodes[0].childNodes[x].getAttribute("data") == startHour)
			{
				startHourNode.selectedItem = startHourNode.childNodes[0].childNodes[x];
				break;
			}
		}

		// set end hour
		var endHourNode = document.getElementById("endHourRange");
		for (var x=0; x < endHourNode.childNodes[0].childNodes.length; x++)
		{
			if (endHourNode.childNodes[0].childNodes[x].getAttribute("data") == endHour)
			{
				endHourNode.selectedItem = endHourNode.childNodes[0].childNodes[x];
				break;
			}
		}
	}

	// get duration
	if ((sep = value.indexOf("|")) > 0)
	{
		var duration = value.substr(0, sep);
		value = value.substr(sep+1, value.length-1);

		var durationNode = document.getElementById("duration");
		durationNode.value = duration;
	}

	// get notification method
    	if (value.indexOf("icon") >= 0)
    	{
    		document.getElementById("bookmarkIcon").checked = true;
    	}
    	if (value.indexOf("sound") >= 0)
    	{
    		document.getElementById("playSound").checked = true;
    	}
    	if (value.indexOf("alert") >= 0)
    	{
    		document.getElementById("showAlert").checked = true;
    	}
    	if (value.indexOf("open") >= 0)
    	{
    		document.getElementById("openWindow").checked = true;
    	}
    }

  // if its a container, disable some things
  var isContainerFlag = RDFC.IsContainer(Bookmarks, RDF.GetResource(bookmark_url));
  if (!isContainerFlag)
  {
  	// XXX To do: the "RDFC.IsContainer" call above only works for RDF sequences;
  	//            if its not a RDF sequence, we should to more checking to see if
  	//            the item in question is really a container of not.  A good example
  	//            of this is the "File System" container.
  }

	if (isContainerFlag)
	{
		// If it is a folder, it has no URL.
		var locationBox = document.getElementById("locationBox");
		if (locationBox)
		{
			dump("Hide location box\n");
			var parentNode = locationBox.parentNode;
			parentNode.removeChild(locationBox);
		}

		// If it is a folder, it has no Shortcut URL.
		var shortcutBox = document.getElementById("shortcutBox");
		if (shortcutBox)
		{
			dump("Hide shortcut box\n");
			var parentNode = shortcutBox.parentNode;
			parentNode.removeChild(shortcutBox);
		}
	}

	if ((bookmark_url.indexOf("http://") != 0) && (bookmark_url.indexOf("https://") != 0))
	{
		// only allow scheduling of http/https URLs
		var scheduleTab = document.getElementById("ScheduleTab");
		if (scheduleTab)
		{
			dump("Hide schedule tab\n");
			var parentNode = scheduleTab.parentNode;
			parentNode.removeChild(scheduleTab);
		}
	}

	window.sizeToContent();

	// set initial focus
	var nameNode = document.getElementById("name");
	if (nameNode)
	{
		nameNode.focus();
	}
}



function Commit()
{
  var changed = false;

  // Grovel through the fields to see if any of the values have
  // changed. If so, update the RDF graph and force them to be saved
  // to disk.
  for (var i = 0; i < Fields.length; ++i)
  {
    var field = document.getElementById(Fields[i]);
    // if the field was removed, just skip it
    if (!field)	continue;

    // Get the new value as a literal, using 'null' if the value is
    // empty.
    var newvalue = field.value;
    dump("field value = " + newvalue + "\n");

    // if the field was removed, just skip it
    if (!Properties[i])	continue;

    var oldvalue = Bookmarks.GetTarget(RDF.GetResource(bookmark_url),
                                       RDF.GetResource(Properties[i]),
                                       true);

    if (oldvalue) oldvalue = oldvalue.QueryInterface(Components.interfaces.nsIRDFLiteral);

    if ((newvalue) && (Properties[i] == (NC_NAMESPACE_URI + "ShortcutURL")))
    {
    	// shortcuts are always lowercased internally
    	newvalue = newvalue.toLowerCase();
    }
    else if ((newvalue) && (Properties[i] == (NC_NAMESPACE_URI + "URL")))
    {
    	// we're dealing with the URL attribute;
    	// if a scheme isn't specified, use "http://"
    	if (newvalue.indexOf(":") < 0)
    	{
    		dump("Setting default URL scheme to HTTP.\n");
    		newvalue = "http://" + newvalue;
    	}
    }
    
    if (updateAttribute(Properties[i], oldvalue, newvalue) == true)
    {
    	changed = true;
    }
  }
  
	// Update bookmark schedule if necessary;
	// if the tab was removed, just skip it
	var scheduleTab = document.getElementById("ScheduleTab");
	if (scheduleTab)
	{
	  	var scheduleRes = "http://home.netscape.com/WEB-rdf#Schedule";
		var oldvalue = Bookmarks.GetTarget(RDF.GetResource(bookmark_url),
	                               RDF.GetResource(scheduleRes), true);
	        var newvalue = "";

		var dayRange = "";
		var dayRangeNode = document.getElementById("dayRange");
		if (dayRangeNode)
		{
			dayRange = dayRangeNode.selectedItem.getAttribute("data");
		}
		if (dayRange != "")
		{
			var startHourRange = "";
			var startHourRangeNode = document.getElementById("startHourRange");
			if (startHourRangeNode)
			{
				startHourRange = startHourRangeNode.selectedItem.getAttribute("data");
			}
			var endHourRange = "";
			var endHourRangeNode = document.getElementById("endHourRange");
			if (endHourRangeNode)
			{
				endHourRange = endHourRangeNode.selectedItem.getAttribute("data");
			}

			if (parseInt(startHourRange) > parseInt(endHourRange))
			{
				var temp = startHourRange;
				startHourRange = endHourRange;
				endHourRange = temp;
			}

			var duration = document.getElementById("duration").value;
			if (duration == "")
			{
				var bundle = srGetStrBundle("chrome://communicator/locale/bookmarks/bookmark.properties");
				alert( bundle.GetStringFromName("pleaseEnterADuration") );
				return(false);
			}

			var method = "";
			if (document.getElementById("bookmarkIcon").checked)	method += ",icon";
			if (document.getElementById("playSound").checked)	method += ",sound";
			if (document.getElementById("showAlert").checked)	method += ",alert";
			if (document.getElementById("openWindow").checked)	method += ",open";
			if (method.length < 1)
			{
				var bundle = srGetStrBundle("chrome://communicator/locale/bookmarks/bookmark.properties");
				alert( bundle.GetStringFromName("pleaseSelectANotification") );
				return(false);
			}
			method = method.substr(1, method.length - 1);	// trim off the initial comma

			dump("dayRange: " + dayRange + "\n");
			dump("startHourRange: " + startHourRange + "\n");
			dump("endHourRange: " + endHourRange + "\n");
			dump("duration: " + duration + "\n");
			dump("method: " + method + "\n");
			
			newvalue = dayRange + "|" + startHourRange + "-" + endHourRange + "|" + duration + "|" + method;
		}
	}

	if (updateAttribute(scheduleRes, oldvalue, newvalue) == true)
	{
		changed = true;
	}

	if (changed == true)
	{
		dump("re-writing bookmarks.html\n");
		var remote = Bookmarks.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
		if (remote)
		{
			remote.Flush();
		}
	}

	window.close();
}



function updateAttribute(prop, oldvalue, newvalue)
{
	var changed = false;

	if (!prop)	return(changed)

    newvalue = (newvalue != '') ? RDF.GetLiteral(newvalue) : null;

    if (oldvalue != newvalue)
    {
      dump("replacing value for " + prop + "\n");
      dump("  oldvalue = " + oldvalue + "\n");
      dump("  newvalue = " + newvalue + "\n");

      if (oldvalue && !newvalue) {
        Bookmarks.Unassert(RDF.GetResource(bookmark_url),
                           RDF.GetResource(prop),
                           oldvalue);
      }
      else if (!oldvalue && newvalue) {
        Bookmarks.Assert(RDF.GetResource(bookmark_url),
                         RDF.GetResource(prop),
                         newvalue,
                         true);
      }
      else if (oldvalue && newvalue) {
        Bookmarks.Change(RDF.GetResource(bookmark_url),
                         RDF.GetResource(prop),
                         oldvalue,
                         newvalue);
      }

      changed = true;
    }
  return(changed);
}



function setEndHourRange()
{
   // Get the values of the start-time and end-time as ints
   var startHourRange = "";
   var startHourRangeNode = document.getElementById("startHourRange");
   if (startHourRangeNode)
   {
      startHourRange = startHourRangeNode.selectedItem.getAttribute("data");
      var startHourRangeInt = parseInt(startHourRange);
   }
   var endHourRange = "";
   var endHourRangeNode = document.getElementById("endHourRange");
   if (endHourRangeNode)
   {
      endHourRange = endHourRangeNode.selectedItem.getAttribute("data");
      var endHourRangeInt = parseInt(endHourRange);
   }
   
   if (endHourRangeNode)
   {
      var endHourItemNode = endHourRangeNode.firstChild.firstChild;

      if (endHourItemNode) {

         // disable all those end-times before the start-time
         for (var index = 0; index < startHourRangeInt; index++) {
            endHourItemNode.setAttribute("disabled", "true");
            endHourItemNode = endHourItemNode.nextSibling;
         }

         // update the selected value if it's out of the allowed range
         if (startHourRangeInt >= endHourRangeInt) {
            endHourRangeNode.selectedItem = endHourItemNode;
         }

         // make sure all the end-times after the start-time are enabled
         for (; index < 24; index++) {
            endHourItemNode.removeAttribute("disabled");
            endHourItemNode = endHourItemNode.nextSibling;
         }
      }
   }
}
