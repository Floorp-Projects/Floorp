enumerator = "component://netscape/gfx/fontenumerator";
enumerator = Components.classes[enumerator].createInstance();
enumerator = enumerator.QueryInterface(Components.interfaces.nsIFontEnumerator);
fontCount = {value: 0 }
fonts = enumerator.EnumerateAllFonts(fontCount);

generics = [
  "serif",
 // "sans-serif",
 // "cursive",
 // "fantasy",
  "monospace"
];

function startUp()
{
    selectLangs = document.getElementById("selectLangs");
    selectLangs.value = "x-western";
	selectLang();
}



function get() {
    dump('\n pref: ' + document.getElementById('serif').getAttribute('pref')  + '\n');
    dump('\n pref: ' + document.getElementById('serif').getAttribute('preftype')  + '\n');
    dump('\n element: ' + document.getElementById('serif').getAttribute('prefstring') + '\n');
    dump('***');
	dump('\n Size pref: ' + document.getElementById('size').getAttribute('pref')  + '\n');
    dump('\n Size pref: ' + document.getElementById('size').getAttribute('preftype')  + '\n');
    dump('\n Size element: ' + document.getElementById('size').getAttribute('prefstring') + '\n');
    dump('\n Value element: ' + document.getElementById('size').value + '\n');
  }

  
//Clears select list
 function ClearList(list)
 {
   if (list) {
     dump(list.length + " \n");
     list.selectedIndex = -1;
     for( j=list.length-1; j >= 0; j--) {
       dump(list + " \n");
       list.remove(j);
	  }
   }
 }



 function AppendStringToListByID(list, stringID) 
 { 
   AppendStringToList(list, editorShell.GetString(stringID)); 
 } 

 function AppendStringToList(list, string) 
 { 
  // THIS DOESN'T WORK! Result is a XULElement -- namespace problem 
  // optionNode1 = document.createElement("option"); 
  // This works - Thanks to Vidur! Params = name, value 
   optionNode = new Option(string, string); 

   if (optionNode) { 
     list.add(optionNode, null); 
   } else { 
     dump("Failed to create OPTION node. String content="+string+"\n"); 
   } 
 } 


function selectLang()
	{
    //get pref services
	var pref = null;
	try
	{
		pref = Components.classes["component://netscape/preferences"];
		if (pref)	pref = pref.getService();
		if (pref)	pref = pref.QueryInterface(Components.interfaces.nsIPref);
	}
	catch(ex)
	{
		dump("failed to get prefs service!\n");
		pref = null;
	}

    //Getting variables needed
	lang = document.getElementById("selectLangs").value;
    dump("LangGroup selected: " +lang +" \n");
	//set prefstring of size
	var fontVarPref = 'font.size.variable.' + lang;
	var fontfixPref = 'font.size.fixed.' + lang;
	var sizeVar =  document.getElementById('size');
	var sizeFix =  document.getElementById('sizeFixed');
	sizeVar.setAttribute('prefstring', fontVarPref);	
	sizeFix.setAttribute('prefstring', fontfixPref);

	//Get size from the preferences
	var fixedFont = null;
	try
	{
		if (pref)
		{
			var fontVarInt = pref.GetIntPref(fontVarPref);
			var fontfixInt = pref.GetIntPref(fontfixPref);
		}
	}
	catch(ex)
	{
	    //Default to 16 and 13 if no size is in the pref
		fixedFont = null;
		fontVarInt = "16"
		fontfixInt = "13"
	}

	dump(fixedFont + "\n");


		

	for (i = 0; i < generics.length; i++)  {
		var select = document.getElementById(generics[i]);
		var selectParent = select.parentNode;
		//create name of font prefstring
		var fontPrefstring = 'font.name.' + generics[i] + '.' + lang;
		select.setAttribute('prefstring', fontPrefstring);
		//Clear the select list
		ClearList(select);
		
		
		fonts = enumerator.EnumerateFonts(lang, generics[i], fontCount);		
		//Append to the select list.  Build the new select list
		for (j = 0; j < fonts.length; j++) {
		    AppendStringToList(select, fonts[j]) 
		}
        
		//Get the pref value of the font
		var fixedFont = null;
		try
		{
			if (pref)
			{
				var selectVal = pref.CopyUniCharPref(fontPrefstring);
			}
		}
		catch(ex)
		{
			selectVal = fonts[0];
		}

	    dump(fixedFont + "\n");
		//select the value of the string
		select.value = selectVal;
		dump('select:' + select.getAttribute('prefstring') + "\n");
		
	}
    //set the value of the sizes
	sizeVar.value = fontVarInt;
    sizeFix.value = fontfixInt;
}

