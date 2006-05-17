enumerator = "component://netscape/gfx/fontenumerator";
enumerator = Components.classes[enumerator].createInstance();
enumerator = enumerator.QueryInterface(Components.interfaces.nsIFontEnumerator);
fontCount = {value: 0 }
fonts = enumerator.EnumerateAllFonts(fontCount);


langs = [
  "x-western",
  "x-central-euro",
  "ja",
  "zh-TW",
  "zh-CN",
  "ko",
  "x-cyrillic",
  "x-baltic",
  "el",
  "tr",
  "x-unicode",
  "x-user-def",
  "th",
  "he",
  "ar"
];
generics = [
  "serif",
  "sans-serif",
  "cursive",
  "fantasy",
  "monospace"
];

function startUp()
{
 
	selectLangs = document.getElementById("selectLangs");
	for (i = 0; i < langs.length; i++) {
		option = document.createElement("html:option");
		text = document.createTextNode(langs[i]);
		option.appendChild(text);
		selectLangs.appendChild(option);
		option.setAttribute('value' , langs[i]);
 
  } 

	selectLang();
}



function selectLang()
	{


	lang = document.getElementById("selectLangs").value;
	var fontSizePrefint = 'font.size.variable.' + lang;
	var fontSizePrefFontint = 'font.size.fixed.' + lang;
	document.getElementById('size').setAttribute('prefstring', fontSizePrefint);	
	document.getElementById('sizeFixed').setAttribute('prefstring', fontSizePrefFontint);	

	for (i = 0; i < generics.length; i++)  {
		var select = document.getElementById(generics[i]);
		var selectParent = select.parentNode;
		var selectNew = select.cloneNode(false);
		var fontPrefstring = 'font.name.' + generics[i] + '.' + lang;
        selectNew.setAttribute('prefstring', fontPrefstring);
	//	dump('selectNew:' + selectNew.getAttribute('prefstring'));
		fonts = enumerator.EnumerateFonts(lang, generics[i], fontCount);

		
		for (j = 0; j < fonts.length; j++) {
			var option = document.createElement("html:option");
			var text = document.createTextNode(fonts[j]);
			option.appendChild(text);
			option.setAttribute('value' , fonts[j]);
			selectNew.appendChild(option);
	//		dump(fonts[j] + " || ");

		}

		selectParent.replaceChild(selectNew , select);
       	//dump('select:' + document.getElementById(generics[i]).getAttribute('prefstring') + '\n');
		
	}
}


