/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*

  Script for the bookmarks properties window

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

var RDF = Components.classes["component://netscape/rdf/rdf-service"].getService();
RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

var RDFC = Components.classes["component://netscape/rdf/container-utils"].getService();
RDFC = RDFC.QueryInterface(Components.interfaces.nsIRDFContainerUtils);

var Bookmarks = RDF.GetDataSource("rdf:bookmarks");


function Init()
{
  // Initialize the properties panel by copying the values from the
  // RDF graph into the fields on screen.

  for (var i = 0; i < Fields.length; ++i) {
    var field = document.getElementById(Fields[i]);

    var value = Bookmarks.GetTarget(RDF.GetResource(BookmarkURL),
                                    RDF.GetResource(Properties[i]),
                                    true);

    if (value) value = value.QueryInterface(Components.interfaces.nsIRDFLiteral);
    if (value) value = value.Value;

    dump("field '" + Fields[i] + "' <== '" + value + "'\n");

    if (value) field.value = value;
  }

  // check bookmark schedule
    var value = Bookmarks.GetTarget(RDF.GetResource(BookmarkURL),
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
		for (var x=0; x < dayNode.options.length; x++)
		{
			if (dayNode.options[x].value == days)
			{
				dayNode.selectedIndex = x;
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
		for (var x=0; x < startHourNode.options.length; x++)
		{
			if (startHourNode.options[x].value == startHour)
			{
				startHourNode.selectedIndex = x;
				break;
			}
		}

		// set end hour
		var endHourNode = document.getElementById("endHourRange");
		for (var x=0; x < endHourNode.options.length; x++)
		{
			if (endHourNode.options[x].value == endHour)
			{
				endHourNode.selectedIndex = x;
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
    		document.getElementById("bookmarkIcon").setAttribute("checked", "1");
    		document.getElementById("bookmarkIcon").checked = true;
    	}
    	if (value.indexOf("sound") >= 0)
    	{
    		document.getElementById("playSound").setAttribute("checked", "1");
    		document.getElementById("playSound").checked = true;
    	}
    	if (value.indexOf("alert") >= 0)
    	{
    		document.getElementById("showAlert").setAttribute("checked", "1");
    		document.getElementById("showAlert").checked = true;
    	}
    	if (value.indexOf("open") >= 0)
    	{
    		document.getElementById("openWindow").setAttribute("checked", "1");
    		document.getElementById("openWindow").checked = true;
    	}
    }
  
  // if its a container, disable some things
  if (RDFC.IsContainer(Bookmarks, RDF.GetResource(BookmarkURL))) {
    // If it is a folder, it has no URL.
    dump("disabling url field for folder\n");
    document.getElementById("url").disabled = true;
    // If it is a folder, it has no Shortcut URL.
    dump("disabling shortcut url field for folder\n");
    document.getElementById("shortcut").disabled = true;

	// If it is a folder, no scheduling!
	var scheduleSepNode = document.getElementById("scheduleSeparator");
	if (scheduleSepNode)
	{
		var parentNode = scheduleSepNode.parentNode;
		parentNode.removeChild(scheduleSepNode);
	}
	var scheduleNode = document.getElementById("scheduleInfo");
	if (scheduleNode)
	{
		var parentNode = scheduleNode.parentNode;
		parentNode.removeChild(scheduleNode);
	}
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

    // Get the new value as a literal, using 'null' if the value is
    // empty.
    var newvalue = field.value;
    dump("field value = " + newvalue + "\n");

    var oldvalue = Bookmarks.GetTarget(RDF.GetResource(BookmarkURL),
                                       RDF.GetResource(Properties[i]),
                                       true);

    if (oldvalue) oldvalue = oldvalue.QueryInterface(Components.interfaces.nsIRDFLiteral);
    
    if (updateAttribute(Properties[i], oldvalue, newvalue) == true)
    {
    	changed = true;
    }
  }
  
  // Update bookmark schedule if necessary
  	var scheduleRes = "http://home.netscape.com/WEB-rdf#Schedule";
	var oldvalue = Bookmarks.GetTarget(RDF.GetResource(BookmarkURL),
                               RDF.GetResource(scheduleRes), true);
        var newvalue = "";

	var dayRange = "";
	var dayRangeNode = document.getElementById("dayRange");
	if (dayRangeNode)
	{
		dayRange = dayRangeNode.options[dayRangeNode.selectedIndex].value;
	}
	if (dayRange != "")
	{
		var startHourRange = "";
		var startHourRangeNode = document.getElementById("startHourRange");
		if (startHourRangeNode)
		{
			startHourRange = startHourRangeNode.options[startHourRangeNode.selectedIndex].value;
		}
		var endHourRange = "";
		var endHourRangeNode = document.getElementById("endHourRange");
		if (endHourRangeNode)
		{
			endHourRange = endHourRangeNode.options[endHourRangeNode.selectedIndex].value;
		}

		if (startHourRange > endHourRange)
		{
			var temp = startHourRange;
			startHourRange = endHourRange;
			endHourRange = temp;
		}

		var duration = document.getElementById("duration").value;
		if (duration == "")
		{
			alert("Please enter a duration.");
			return(false);
		}

		var method = "";
		if (document.getElementById("bookmarkIcon").checked)	method += ",icon";
		if (document.getElementById("playSound").checked)	method += ",sound";
		if (document.getElementById("showAlert").checked)	method += ",alert";
		if (document.getElementById("openWindow").checked)	method += ",open";
		if (method.length < 1)
		{
			alert("Please select a notification method.");
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

    newvalue = (newvalue != '') ? RDF.GetLiteral(newvalue) : null;

    if (oldvalue != newvalue)
    {
      dump("replacing value for " + prop + "\n");
      dump("  oldvalue = " + oldvalue + "\n");
      dump("  newvalue = " + newvalue + "\n");

      if (oldvalue && !newvalue) {
        Bookmarks.Unassert(RDF.GetResource(BookmarkURL),
                           RDF.GetResource(prop),
                           oldvalue);
      }
      else if (!oldvalue && newvalue) {
        Bookmarks.Assert(RDF.GetResource(BookmarkURL),
                         RDF.GetResource(prop),
                         newvalue,
                         true);
      }
      else if (oldvalue && newvalue) {
        Bookmarks.Change(RDF.GetResource(BookmarkURL),
                         RDF.GetResource(prop),
                         oldvalue,
                         newvalue);
      }

      changed = true;
    }
  return(changed);
}


function Cancel()
{
  // Ignore any changes.
  window.close();
}
