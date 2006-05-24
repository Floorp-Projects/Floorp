/*

    Key Events: A Really Simple Key Handler
    
*/

KeyEvents = {
    handled: false,
    handleF1: function() {
        replaceChildNodes('specialMessage', 'You invoked the special F1 handler!');
    },
    handleEscape: function() {
        replaceChildNodes('specialMessage', 'You invoked the special Escape handler!');
    },
    updateModifiers: function(e) {
        var modifiers = e.modifier();
        replaceChildNodes('shift', modifiers.shift);
        replaceChildNodes('ctrl', modifiers.ctrl);
        replaceChildNodes('alt', modifiers.alt);
        replaceChildNodes('meta', modifiers.meta);
    }
};

KeyEvents.specialKeyMap = {
    'KEY_F1': KeyEvents.handleF1,
    'KEY_ESCAPE': KeyEvents.handleEscape
};

connect(document, 'onkeydown', 
    function(e) {
        // We're storing a handled flag to work around a Safari bug: 
        // http://bugzilla.opendarwin.org/show_bug.cgi?id=3387
        if (!KeyEvents.handled) {
            var key = e.key();
            var fn = KeyEvents.specialKeyMap[key.string];
            if (fn) {
                fn();
            }
            replaceChildNodes('onkeydown_code', key.code);
            replaceChildNodes('onkeydown_string', key.string);
            KeyEvents.updateModifiers(e);
        }
        KeyEvents.handled = true;
    });
    
connect(document, 'onkeyup', 
    function(e) {
        KeyEvents.handled = false;
        var key = e.key();
        replaceChildNodes('onkeyup_code', key.code);
        replaceChildNodes('onkeyup_string', key.string);
        KeyEvents.updateModifiers(e);
    });

connect(document, 'onkeypress', 
    function(e) {
        var key = e.key();
        replaceChildNodes('onkeypress_code', key.code);
        replaceChildNodes('onkeypress_string', key.string);
        KeyEvents.updateModifiers(e);
    });

connect(window, 'onload',
    function() {        
        var elems = getElementsByTagAndClassName("A", "view-source");
        var page = "key_events/";
        for (var i = 0; i < elems.length; i++) {
            var elem = elems[i];
            var href = elem.href.split(/\//).pop();
            elem.target = "_blank";
            elem.href = "../view-source/view-source.html#" + page + href;
        }
    });