enumerator = "component://netscape/gfx/fontenumerator";
enumerator = Components.classes[enumerator].createInstance();
enumerator = enumerator.QueryInterface(Components.interfaces.nsIFontEnumerator);
fontCount = {value: 0 }
fonts = enumerator.EnumerateAllFonts(fontCount);

generics = [
  "serif",
  "sans-serif",
  "cursive",
  "fantasy",
  "monospace"
];

selects = [
  "serif",
  "sans-serif",
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


    var faceList        = new String();
    var combinedFonts	= new Array();

    for (i = 0; i < generics.length; i++)  {
		    
		fonts = enumerator.EnumerateFonts(lang, generics[i], fontCount);		
		
		for (j = 0; j < fonts.length; j++) {
                   
            if (faceList.indexOf(fonts[j]+',') == -1) {
               faceList += fonts[j]+',';
            }

		} //for fonts
		
	} //for generics
    
    //strip trailing delimiter
    if (faceList.length > 0)  faceList = faceList.substring(0,faceList.length-1);
    
    dump('faceList: ' + faceList + '\n');
    combinedFonts = faceList.split(',');

    //sort serif and sans serif fonts
    combinedFonts.sort();

    dump('Combined fonts: \n');

    for (i = 0; i < combinedFonts.length; i++) {
         dump(combinedFonts[i] + '\n');
    }

    for (i = 0; i < selects.length; i++)  {

        var fontList = selects[i];
        var select = document.getElementById(fontList);

        //Clear the select list
        ClearList(select);

	    //create name of font prefstring
        var fontPrefstring = 'font.name.' + fontList + '.' + lang;

        select.setAttribute('prefstring', fontPrefstring);

        for (j = 0; j < combinedFonts.length; j++) {
             AppendStringToList(select, combinedFonts[j]);
        }

	    try
	    {
		    if (pref)
		    {
			    var selectVal = pref.CopyUnicharPref(fontPrefstring);
		    }
	    }

	    catch(ex)
	    {
		    selectVal = '';
	    }

	    //select the value of the string
	    select.value = selectVal;

	    dump('fontPrefstring:' + fontPrefstring + "\n");
   	    dump('selectVal:' + selectVal + "\n");
	    dump('select:' + select.getAttribute('prefstring') + "\n");

    } //for select

    //set the value of the sizes
	sizeVar.value = fontVarInt;
    sizeFix.value = fontfixInt;
}

