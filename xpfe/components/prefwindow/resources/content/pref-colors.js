function setColorWell(aPicker)
{
  var colorRef = aPicker.nextSibling;                // colour value is held here
  colorRef.setAttribute( "value", aPicker.color );
}

  function setColorWellSr(menu,otherId,setbackground)
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

  function getColorFromWellAndSetValue(aPickerId)
  {
    var picker = document.getElementById(aPickerId);
    var colorRef  = picker.nextSibling;
    var color = colorRef.getAttribute("value");
    picker.color = color;
    return color;
  }     

  function Startup()
  {
    getColorFromWellAndSetValue("foregroundtextmenu");
    getColorFromWellAndSetValue("backgroundmenu");
    getColorFromWellAndSetValue("unvisitedlinkmenu");
    getColorFromWellAndSetValue("visitedlinkmenu");

    return true;
  }                   
  
