/* 
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): Robert John Churchill (rjc@netscape.com)
 */

	var pref = null;
	try
	{
		pref = Components.classes["@mozilla.org/preferences;1"];
		if (pref)	pref = pref.getService();
		if (pref)	pref = pref.QueryInterface(Components.interfaces.nsIPref);
	}
	catch(ex)
	{
		dump("failed to get prefs service!\n");
		pref = null;
	}



function InitSingleEngineList()
{
	dump("InitSingleEngineList called.\n");

	var defaultEngineURI = null;
	try
	{
		if (pref)
		{
			var defaultEngineURI = pref.CopyCharPref("browser.search.defaultengine");
		}
	}
	catch(ex)
	{
		defaultEngineURI = null;
	}

	if ((!defaultEngineURI) || (defaultEngineURI == ""))	return;

	var engineList = document.getElementById("engineList");
	if (!engineList)	return;
	var numEngines = engineList.length;
	if (numEngines < 1)	return;

	for (var x=0; x<numEngines; x++)
	{
		var engineNode = engineList.childNodes[x];
		if (!engineNode)	continue;
		var uri = engineNode.getAttribute("id");
		if (!uri)		continue;
		if (uri != defaultEngineURI)	continue;

		engineList.selectedIndex = x;
		break;
	}
}



function setDefaultSearchEngine(object)
{
	var	defaultEngineURI = object.options[object.selectedIndex].getAttribute("value");
	dump("Default Engine: '" + defaultEngineURI + "'\n");

	try
	{
		if (pref)
		{
			pref.SetCharPref("browser.search.defaultengine", defaultEngineURI);
		}
	}
	catch (ex)
	{
		dump("failed to set 'browser.search.defaultengine' pref\n");
	}
	return(true);
}
