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
 * Contributor(s):
 *   Robert John Churchill <rjc@netscape.com>
 *   Mark Olson <maolson@earthlink.net>
 */


function checkEngine()
{
  var engineList = document.getElementById("engineList");
  var engineValue = engineList.label;
  
  //nothing is selected
  if (!engineValue)
  {

    try
    {
        var strDefaultSearchEngineName = parent.hPrefWindow.pref.getLocalizedUnicharPref("browser.search.defaultenginename");

        var engineListSelection = engineList.getElementsByAttribute( "label", strDefaultSearchEngineName );
        var selectedItem = engineListSelection.length ? engineListSelection[0] : null;
    
        if (selectedItem)
        {
            //select a locale-dependent predefined search engine in absence of a user default
            engineList.selectedItem = selectedItem;
        }
        else
        {
            //select the first listed search engine
            engineList.selectedIndex = 1;
        }
    }

    catch(e)
    {
        //select the first listed search engine
        engineList.selectedIndex = 1;
    }

  }
}

