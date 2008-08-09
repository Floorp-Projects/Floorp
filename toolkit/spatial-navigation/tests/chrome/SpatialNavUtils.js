var _moveTable;
var _moveTableIndex = 0;

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

