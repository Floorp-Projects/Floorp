// -*- Mode: Java -*-

function Init()
{
  var panel_name_div  = document.getElementById('panelname');
  var customize_frame = document.getElementById('customizeframe');
  var panel_text      = document.createTextNode(panel_name)
  panel_name_div.appendChild(panel_text);
  // The customize page currently is not loading because of a redirect bug.
  customize_frame.setAttribute('src', panel_customize_URL);
}
