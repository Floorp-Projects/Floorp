/*

    Drag: A Really Simple Drag Handler
    
*/
Drag = {
    _move: null,
    _down: null,
    
    start: function(e) {
        e.stop();
        
        // We need to remember what we're dragging.
        Drag._target = e.target();
        
        /*
            There's no cross-browser way to get offsetX and offsetY, so we
            have to do it ourselves. For performance, we do this once and
            cache it.
        */
        Drag._offset = Drag._diff(
            e.mouse().page,
            elementPosition(Drag._target));
        
        Drag._move = connect(document, 'onmousemove', Drag._drag);
        Drag._down = connect(document, 'onmouseup', Drag._stop);
    },

    _offset: null,
    _target: null,
    
    _diff: function(lhs, rhs) {
        return new MochiKit.DOM.Coordinates(lhs.x - rhs.x, lhs.y - rhs.y);
    },
        
    _drag: function(e) {
        e.stop();
        setElementPosition(
            Drag._target,
            Drag._diff(e.mouse().page, Drag._offset));
    },
    
    _stop: function(e) {
        disconnect(Drag._move);
        disconnect(Drag._down);
    }
};

connect(window, 'onload',   
    function() {
        /*
            Find all DIVs tagged with the draggable class, and connect them to
            the Drag handler.
        */
        var d = getElementsByTagAndClassName('DIV', 'draggable');
        forEach(d,
            function(elem) {
                connect(elem, 'onmousedown', Drag.start);
            });
                        
    });
    
connect(window, 'onload',
    function() {
        var elems = getElementsByTagAndClassName("A", "view-source");
        var page = "draggable/";
        for (var i = 0; i < elems.length; i++) {
            var elem = elems[i];
            var href = elem.href.split(/\//).pop();
            elem.target = "_blank";
            elem.href = "../view-source/view-source.html#" + page + href;
        } 
    });
