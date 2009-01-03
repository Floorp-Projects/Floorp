var _moveTable;
var _moveTableIndex = 0;

var _snavEnabledOld = null;
var _snavXULContentEnabledOld = null;
var _snavModOld = null ;
var _snavRightKeyOld = null;
var _snavLeftKeyOld = null;
var _snavDownKeyOld = null;
var _snavUpKeyOld = null;

function prepareTest(prefs) {

    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefService);
    var snavBranch = prefService.getBranch("snav.");

    var i;
    for (i = 0; i < prefs.length; i++)
    {
        switch (prefs[i][0])
        {
            case "enabled":
                try {
                    _snavEnabledOld = snavBranch.getBoolPref("enabled");
                } catch(e) {
                    _snavEnabledOld = false;
                }
                break;
            case "xulContentEnabled":
                try {
                    _snavXULContentEnabledOld = snavBranch.getBoolPref("xulContentEnabled");
                } catch(e) {
                    _snavXULContentEnabledOld = false;
                }
                break;
            case "keyCode.modifier":
                try {
                    _snavModOld = snavBranch.getCharPref("keyCode.modifier");
                } catch(e) {
                    _snavModOld = "alt+shift";
                }
                break;
            case "keyCode.right" :
                try {
                    _snavRightKeyOld = snavBranch.getIntPref("keyCode.right");
                } catch(e) {
                    _snavRightKeyOld = Components.interfaces.nsIDOMKeyEvent.DOM_VK_RIGHT;
                }
                break;
            case "keyCode.left" :
                try {
                    _snavLeftKeyOld = snavBranch.getIntPref("keyCode.left");
                } catch(e) {
                    _snavLeftKeyOld = Components.interfaces.nsIDOMKeyEvent.DOM_VK_LEFT;
                }
                break;
            case "keyCode.down" :
                try {
                    _snavDownKeyOld = snavBranch.getIntPref("keyCode.down");
                } catch(e) {
                    _snavDownKeyOld = Components.interfaces.nsIDOMKeyEvent.DOM_VK_DOWN;
                }
                break;
            case "keyCode.up" :
                try {
                    _snavUpKeyOld = snavBranch.getIntPref("keyCode.up");
                } catch(e) {
                    _snavUpKeyOld = Components.interfaces.nsIDOMKeyEvent.DOM_VK_UP;
                }
                break;
        } // switch

        if (prefs[i][1] == "bool")
            snavBranch.setBoolPref(prefs[i][0], prefs[i][2]);
        else if (prefs[i][1] == "int")
            snavBranch.setIntPref(prefs[i][0], prefs[i][2]);
        else if (prefs[i][1] == "char")
            snavBranch.setCharPref(prefs[i][0], prefs[i][2]);
    } // for
}

// setting pref values back.
function completeTest() {

    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
            .getService(Components.interfaces.nsIPrefService);
    var snavBranch = prefService.getBranch("snav.");

    if (_snavEnabledOld != null)
        snavBranch.setBoolPref("enabled", _snavEnabledOld);
    if (_snavXULContentEnabledOld != null)
        snavBranch.setBoolPref("xulContentEnabled", _snavXULContentEnabledOld);
    if (_snavModOld != null)
        snavBranch.setCharPref("keyCode.modifier", _snavModOld);
    if (_snavRightKeyOld != null)
        snavBranch.setIntPref("keyCode.right", _snavRightKeyOld);
    if (_snavUpKeyOld != null)
        snavBranch.setIntPref("keyCode.up", _snavUpKeyOld);
    if (_snavDownKeyOld != null)
        snavBranch.setIntPref("keyCode.down", _snavDownKeyOld);
    if (_snavLeftKeyOld != null)
        snavBranch.setIntPref("keyCode.left", _snavLeftKeyOld);
}

function testMoves(table) {
    //    document.addEventListener("focus", _verifyAndAdvance, true);

    _moveTable = table;
    _moveTableIndex = 0;
    _move();
}

function _nextMove()
{
    _moveTableIndex++;
    
    // When a table ends with "DONE", call finish.
    if (_moveTable[_moveTableIndex][0] == "DONE") {
        completeTest ();
        SimpleTest.finish();
        return;
    }
    
    // when a table has an empty elment, end the moves.
    if (_moveTable[_moveTableIndex][0] == "") {
        return;
    }
    
    _move();
}

function _move()
{
    sendKey( _moveTable[_moveTableIndex][0], document.activeElement);
    setTimeout( _verifyAndAdvance , 100);
}

function _verifyAndAdvance()
{
    var direction = _moveTable[_moveTableIndex][0];
    var expectedID = _moveTable[_moveTableIndex][1];
    
    ok(document.activeElement.getAttribute("id") == expectedID,
       "Move " + direction + " to " + expectedID + ". Found " + document.activeElement.getAttribute("id"));
    
    _nextMove();
}

