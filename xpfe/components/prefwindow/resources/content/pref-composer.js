  function setColorWell(menu,otherId,setbackground)
  {
      // Find the colorWell and colorPicker in the hierarchy.
      var colorWell = menu.firstChild;
      var colorPicker = menu.firstChild.nextSibling.nextSibling.firstChild;
      var colorRef = menu.nextSibling;                // colour value is held here
  
      // Extract color from colorPicker and assign to colorWell.
      var color = colorPicker.getAttribute('color');
      // set colour values in the display 
      setColorFromPicker(colorWell,color,otherId,setbackground);
      // set the colour value internally for use in prefwindow
      dump("*** setting value of internal field to : " + color + "\n");
      colorRef.setAttribute( "value", color );
  }

  function getColorFromWellAndSetValue(menuid,otherId,setbackground)
  {
    var menu      = document.getElementById(menuid);  // picker container
    var colorWell = menu.firstChild;                  // display for picker colour
    var colorRef  = menu.nextSibling;                 // prefs JS sets this.
    colorWell.style.backgroundColor = colorRef.value; // set the well from prefs.
    var color     = colorWell.style.backgroundColor;   
    setColorFromPicker(null,color,otherId,setbackground);
  }     

  function setColorFromPicker(colorWell,color,otherId,setbackground)
  {
    if (colorWell) {
      colorWell.style.backgroundColor = color;
    }
    if (otherId) {
	    otherElement=document.getElementById(otherId);
	    if (setbackground) {
		    var basestyle = otherElement.getAttribute('basestyle');
		    var newstyle = basestyle + "background-color:" + color + ";";
		    otherElement.setAttribute('style',newstyle);
	    }
	    else {
		    otherElement.style.color = color;
	    }
    }
  }
  
  function InitColors()
  {
    getColorFromWellAndSetValue('normaltextmenu','normaltext',false);
    getColorFromWellAndSetValue('linktextmenu','linktext',false);
    getColorFromWellAndSetValue('activelinktextmenu','activelinktext',false);
    getColorFromWellAndSetValue('followedlinktextmenu','followedlinktext',false);
    getColorFromWellAndSetValue('colorpreviewmenu','colorpreview',true);
    return true;
  }                   
  
