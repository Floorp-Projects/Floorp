
function switchPage( aElement )
{
  var index = aElement.options[ aElement.selectedIndex ].value;
  var deck = document.getElementById( "modifierDeck" );
  dump("*** switch to deck page: " + index + "\n");
  deck.setAttribute( "index", index );
}

function doEnableElement( aEventTarget, aElementID )
{
  var aElement = document.getElementById( aElementID );
  if( aEventTarget.checked == true )
    aElement.setAttribute( "disabled", "true" );
  else
    aElement.removeAttribute( "disabled" );
}

function Startup()
{
  var fields = ["mousewheelWithNoKeyNumlines", "mousewheelWithAltKeyNumlines", "mousewheelWithCtrlKeyNumlines", "mousewheelWithShiftKeyNumlines"];
  var checkboxes = ["mousewheelWithNoKeySysNumlines", "mousewheelWithAltKeySysNumlines", "mousewheelWithCtrlKeySysNumlines", "mousewheelWithShiftKeySysNumlines"];
  for( var i = 0; i < checkboxes.length; i++ )
  {
    var currEl = document.getElementById( checkboxes[i] );
    doEnableElement( currEl, fields[i] );
  }
}