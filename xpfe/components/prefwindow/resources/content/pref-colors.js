  function setColorWell(menu,otherId,setbackground)
  {
    // Find the colorWell and colorPicker in the hierarchy.
    var colorWell = menu.firstChild;
    var colorPicker = menu.firstChild.nextSibling.nextSibling.firstChild;
    var colorRef = menu.nextSibling;                // colour value is held here
  
    // Extract color from colorPicker and assign to colorWell.
    var color = colorPicker.getAttribute('color');
    // set colour values in the display 
    setColorFromPicker( colorWell, color );
    // set the colour value internally for use in prefwindow
    colorRef.setAttribute( "value", color );
  }

  function getColorFromWellAndSetValue( menuid )
  {
    var menu      = document.getElementById(menuid);  // picker container
    var colorWell = menu.firstChild;                  // display for picker colour
    var colorRef  = menu.nextSibling;                 // prefs JS sets this.
    colorWell.style.backgroundColor = colorRef.value; // set the well from prefs.
    var color     = colorWell.style.backgroundColor;   
    setColorFromPicker( null, color );
  }     

  function setColorFromPicker(colorWell,color )
  {
    if (colorWell) {
      colorWell.style.backgroundColor = color;
    }
  }
  
  function Startup()
  {
    getColorFromWellAndSetValue("foregroundtextmenu");
    getColorFromWellAndSetValue("backgroundmenu");
    getColorFromWellAndSetValue("unvisitedlinkmenu");
    getColorFromWellAndSetValue("visitedlinkmenu");
    return true;
  }                   
  
