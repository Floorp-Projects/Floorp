/***

MochiKit.Signal 1.3.1

See <http://mochikit.com/> for documentation, downloads, license, etc.

(c) 2006 Jonathan Gardner, Beau Hartshorne, Bob Ippolito.  All rights Reserved.

***/

if (typeof(dojo) != 'undefined') {
    dojo.provide('MochiKit.Signal');
    dojo.require('MochiKit.Base');
    dojo.require('MochiKit.DOM');
}
if (typeof(JSAN) != 'undefined') {
    JSAN.use('MochiKit.Base', []);
    JSAN.use('MochiKit.DOM', []);
}

try {
    if (typeof(MochiKit.Base) == 'undefined') {
        throw '';
    }
} catch (e) {
    throw 'MochiKit.Signal depends on MochiKit.Base!';
}

try {
    if (typeof(MochiKit.DOM) == 'undefined') {
        throw '';
    }
} catch (e) {
    throw 'MochiKit.Signal depends on MochiKit.DOM!';
}

if (typeof(MochiKit.Signal) == 'undefined') {
    MochiKit.Signal = {};
}

MochiKit.Signal.NAME = 'MochiKit.Signal';
MochiKit.Signal.VERSION = '1.3.1';

MochiKit.Signal._observers = [];

MochiKit.Signal.Event = function (src, e) {
    this._event = e || window.event;
    this._src = src;
};

MochiKit.Base.update(MochiKit.Signal.Event.prototype, {

    __repr__: function() {
        var repr = MochiKit.Base.repr;
        var str = '{event(): ' + repr(this.event()) +
            ', src(): ' + repr(this.src()) + 
            ', type(): ' + repr(this.type()) +
            ', target(): ' + repr(this.target()) +
            ', modifier(): ' + '{alt: ' + repr(this.modifier().alt) +
            ', ctrl: ' + repr(this.modifier().ctrl) +
            ', meta: ' + repr(this.modifier().meta) +
            ', shift: ' + repr(this.modifier().shift) + 
            ', any: ' + repr(this.modifier().any) + '}';
        
        if (this.type() && this.type().indexOf('key') === 0) {
            str += ', key(): {code: ' + repr(this.key().code) +
                ', string: ' + repr(this.key().string) + '}';
        }

        if (this.type() && (
            this.type().indexOf('mouse') === 0 ||
            this.type().indexOf('click') != -1 ||
            this.type() == 'contextmenu')) {

            str += ', mouse(): {page: ' + repr(this.mouse().page) +
                ', client: ' + repr(this.mouse().client);

            if (this.type() != 'mousemove') {
                str += ', button: {left: ' + repr(this.mouse().button.left) +
                    ', middle: ' + repr(this.mouse().button.middle) +
                    ', right: ' + repr(this.mouse().button.right) + '}}';
            } else {
                str += '}';
            }
        }
        if (this.type() == 'mouseover' || this.type() == 'mouseout') {
            str += ', relatedTarget(): ' + repr(this.relatedTarget());
        }
        str += '}';
        return str;
    },

    toString: function () {
        return this.__repr__();
    },

    src: function () {
        return this._src;
    },

    event: function () {
        return this._event;
    },

    type: function () {
        return this._event.type || undefined;
    },

    target: function () {
        return this._event.target || this._event.srcElement;
    },

    relatedTarget: function () {
        if (this.type() == 'mouseover') {
            return (this._event.relatedTarget ||
                this._event.fromElement);
        } else if (this.type() == 'mouseout') {
            return (this._event.relatedTarget ||
                this._event.toElement);
        }
        // throw new Error("relatedTarget only available for 'mouseover' and 'mouseout'");
        return undefined;
    },

    modifier: function () {
        var m = {};
        m.alt = this._event.altKey;
        m.ctrl = this._event.ctrlKey;
        m.meta = this._event.metaKey || false; // IE and Opera punt here
        m.shift = this._event.shiftKey;
        m.any = m.alt || m.ctrl || m.shift || m.meta;
        return m;
    },

    key: function () {
        var k = {};
        if (this.type() && this.type().indexOf('key') === 0) {

            /*

    			If you're looking for a special key, look for it in keydown or
                keyup, but never keypress. If you're looking for a Unicode
                chracter, look for it with keypress, but never keyup or
                keydown.
	
    			Notes:
	
    			FF key event behavior:
    			key     event   charCode    keyCode
    			DOWN    ku,kd   0           40
    			DOWN    kp      0           40
    			ESC     ku,kd   0           27
    			ESC     kp      0           27
    			a       ku,kd   0           65
    			a       kp      97          0
    			shift+a ku,kd   0           65
    			shift+a kp      65          0
    			1       ku,kd   0           49
    			1       kp      49          0
    			shift+1 ku,kd   0           0
    			shift+1 kp      33          0
	
    			IE key event behavior:
    			(IE doesn't fire keypress events for special keys.)
    			key     event   keyCode
    			DOWN    ku,kd   40
    			DOWN    kp      undefined
    			ESC     ku,kd   27
    			ESC     kp      27
    			a       ku,kd   65
    			a       kp      97
    			shift+a ku,kd   65
    			shift+a kp      65
    			1       ku,kd   49
    			1       kp      49
    			shift+1 ku,kd   49
    			shift+1 kp      33

    			Safari key event behavior:
    			(Safari sets charCode and keyCode to something crazy for
    			special keys.)
    			key     event   charCode    keyCode
    			DOWN    ku,kd   63233       40
    			DOWN    kp      63233       63233
    			ESC     ku,kd   27          27
    			ESC     kp      27          27
    			a       ku,kd   97          65
    			a       kp      97          97
    			shift+a ku,kd   65          65
    			shift+a kp      65          65
    			1       ku,kd   49          49
    			1       kp      49          49
    			shift+1 ku,kd   33          49
    			shift+1 kp      33          33

            */

            /* look for special keys here */
            if (this.type() == 'keydown' || this.type() == 'keyup') {
                k.code = this._event.keyCode;
                k.string = (MochiKit.Signal._specialKeys[k.code] ||
                    'KEY_UNKNOWN');
                return k;
        
            /* look for characters here */
            } else if (this.type() == 'keypress') {
            
                /*
            
                    Special key behavior:
                
                    IE: does not fire keypress events for special keys
                    FF: sets charCode to 0, and sets the correct keyCode
                    Safari: sets keyCode and charCode to something stupid
            
                */
            
                k.code = 0;
                k.string = '';
                        
                if (typeof(this._event.charCode) != 'undefined' && 
                    this._event.charCode !== 0 &&
                    !MochiKit.Signal._specialMacKeys[this._event.charCode]) {
                    k.code = this._event.charCode;
                    k.string = String.fromCharCode(k.code);
                } else if (this._event.keyCode && 
                    typeof(this._event.charCode) == 'undefined') { // IE
                    k.code = this._event.keyCode;
                    k.string = String.fromCharCode(k.code);
                }
            
                return k;
            }
        }
        // throw new Error('This is not a key event');
        return undefined;
    },

    mouse: function () {
        var m = {};
        var e = this._event;
        
        if (this.type() && (
            this.type().indexOf('mouse') === 0 ||
            this.type().indexOf('click') != -1 ||
            this.type() == 'contextmenu')) {
            
            m.client = new MochiKit.DOM.Coordinates(0, 0);
            if (e.clientX || e.clientY) {
                m.client.x = (!e.clientX || e.clientX < 0) ? 0 : e.clientX;
                m.client.y = (!e.clientY || e.clientY < 0) ? 0 : e.clientY;
            }

            m.page = new MochiKit.DOM.Coordinates(0, 0);
            if (e.pageX || e.pageY) {
                m.page.x = (!e.pageX || e.pageX < 0) ? 0 : e.pageX;
                m.page.y = (!e.pageY || e.pageY < 0) ? 0 : e.pageY;
            } else {
                /*
            
    				IE keeps the document offset in:
        				document.documentElement.clientTop ||
        				document.body.clientTop
				
    				and:
        				document.documentElement.clientLeft ||
        				document.body.clientLeft

                    see:
    				http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/getboundingclientrect.asp

    				The offset is (2,2) in standards mode and (0,0) in quirks 
    				mode.
				
                */
            
                var de = MochiKit.DOM._document.documentElement;
                var b = MochiKit.DOM._document.body;
            
                m.page.x = e.clientX +
                    (de.scrollLeft || b.scrollLeft) - 
                    (de.clientLeft || b.clientLeft);
            
                m.page.y = e.clientY +
                    (de.scrollTop || b.scrollTop) - 
                    (de.clientTop || b.clientTop);
            
            }
            if (this.type() != 'mousemove') {
                m.button = {};
                m.button.left = false;
                m.button.right = false;
                m.button.middle = false;

                /* we could check e.button, but which is more consistent */
                if (e.which) {
                    m.button.left = (e.which == 1);
                    m.button.middle = (e.which == 2);
                    m.button.right = (e.which == 3);

                    /*
                
    					Mac browsers and right click:
					
    						- Safari doesn't fire any click events on a right
    						  click:
    						  http://bugzilla.opendarwin.org/show_bug.cgi?id=6595
						  
    						- Firefox fires the event, and sets ctrlKey = true
						  
    						- Opera fires the event, and sets metaKey = true
					
    					oncontextmenu is fired on right clicks between 
    					browsers and across platforms.
					
                    */
                
                } else {
                    m.button.left = !!(e.button & 1);
                    m.button.right = !!(e.button & 2);
                    m.button.middle = !!(e.button & 4);
                }
            }
            return m;
        }
        // throw new Error('This is not a mouse event');
        return undefined;
    },

    stop: function () {
        this.stopPropagation();
        this.preventDefault();
    },

    stopPropagation: function () {
        if (this._event.stopPropagation) {
            this._event.stopPropagation();
        } else {
            this._event.cancelBubble = true;
        }
    },

    preventDefault: function () {
        if (this._event.preventDefault) {
            this._event.preventDefault();
        } else {
            this._event.returnValue = false;
        }
    }

});

/* Safari sets keyCode to these special values onkeypress. */
MochiKit.Signal._specialMacKeys = {
    3: 'KEY_ENTER',
    63289: 'KEY_NUM_PAD_CLEAR',
    63276: 'KEY_PAGE_UP',
    63277: 'KEY_PAGE_DOWN',
    63275: 'KEY_END',
    63273: 'KEY_HOME',
    63234: 'KEY_ARROW_LEFT',
    63232: 'KEY_ARROW_UP',
    63235: 'KEY_ARROW_RIGHT',
    63233: 'KEY_ARROW_DOWN',
    63302: 'KEY_INSERT',
    63272: 'KEY_DELETE'
};

/* for KEY_F1 - KEY_F12 */
for (i = 63236; i <= 63242; i++) {
    MochiKit.Signal._specialMacKeys[i] = 'KEY_F' + (i - 63236 + 1); // no F0
}

/* Standard keyboard key codes. */
MochiKit.Signal._specialKeys = {
    8: 'KEY_BACKSPACE',
    9: 'KEY_TAB',
    12: 'KEY_NUM_PAD_CLEAR', // weird, for Safari and Mac FF only
    13: 'KEY_ENTER',
    16: 'KEY_SHIFT',
    17: 'KEY_CTRL',
    18: 'KEY_ALT',
    19: 'KEY_PAUSE',
    20: 'KEY_CAPS_LOCK',
    27: 'KEY_ESCAPE',
    32: 'KEY_SPACEBAR',
    33: 'KEY_PAGE_UP',
    34: 'KEY_PAGE_DOWN',
    35: 'KEY_END',
    36: 'KEY_HOME',
    37: 'KEY_ARROW_LEFT',
    38: 'KEY_ARROW_UP',
    39: 'KEY_ARROW_RIGHT',
    40: 'KEY_ARROW_DOWN',
    44: 'KEY_PRINT_SCREEN', 
    45: 'KEY_INSERT',
    46: 'KEY_DELETE',
    59: 'KEY_SEMICOLON', // weird, for Safari and IE only
    91: 'KEY_WINDOWS_LEFT', 
    92: 'KEY_WINDOWS_RIGHT', 
    93: 'KEY_SELECT', 
    106: 'KEY_NUM_PAD_ASTERISK',
    107: 'KEY_NUM_PAD_PLUS_SIGN',
    109: 'KEY_NUM_PAD_HYPHEN-MINUS',
    110: 'KEY_NUM_PAD_FULL_STOP',
    111: 'KEY_NUM_PAD_SOLIDUS',
    144: 'KEY_NUM_LOCK',
    145: 'KEY_SCROLL_LOCK',
    186: 'KEY_SEMICOLON',
    187: 'KEY_EQUALS_SIGN',
    188: 'KEY_COMMA',
    189: 'KEY_HYPHEN-MINUS',
    190: 'KEY_FULL_STOP',
    191: 'KEY_SOLIDUS',
    192: 'KEY_GRAVE_ACCENT',
    219: 'KEY_LEFT_SQUARE_BRACKET',
    220: 'KEY_REVERSE_SOLIDUS',
    221: 'KEY_RIGHT_SQUARE_BRACKET',
    222: 'KEY_APOSTROPHE'
    // undefined: 'KEY_UNKNOWN'
};

/* for KEY_0 - KEY_9 */
for (var i = 48; i <= 57; i++) {
    MochiKit.Signal._specialKeys[i] = 'KEY_' + (i - 48);
}

/* for KEY_A - KEY_Z */
for (i = 65; i <= 90; i++) {
    MochiKit.Signal._specialKeys[i] = 'KEY_' + String.fromCharCode(i);
}

/* for KEY_NUM_PAD_0 - KEY_NUM_PAD_9 */
for (i = 96; i <= 105; i++) {
    MochiKit.Signal._specialKeys[i] = 'KEY_NUM_PAD_' + (i - 96);
}

/* for KEY_F1 - KEY_F12 */
for (i = 112; i <= 123; i++) {
    MochiKit.Signal._specialKeys[i] = 'KEY_F' + (i - 112 + 1); // no F0
}

MochiKit.Base.update(MochiKit.Signal, {

    __repr__: function () {
        return '[' + this.NAME + ' ' + this.VERSION + ']';
    },

    toString: function () {
        return this.__repr__();
    },

    _unloadCache: function () {
        var self = MochiKit.Signal;
        var observers = self._observers;
        
        for (var i = 0; i < observers.length; i++) {
            self._disconnect(observers[i]);
        }
        
        delete self._observers;
        
        try {
            window.onload = undefined;
        } catch(e) {
            // pass
        }

        try {
            window.onunload = undefined;
        } catch(e) {
            // pass
        }
    },

    _listener: function (src, func, obj, isDOM) {
        var E = MochiKit.Signal.Event;
        if (!isDOM) {
            return MochiKit.Base.bind(func, obj);
        } 
        obj = obj || src;
        if (typeof(func) == "string") {
            return function (nativeEvent) {
                obj[func].apply(obj, [new E(src, nativeEvent)]);
            };
        } else {
            return function (nativeEvent) {
                func.apply(obj, [new E(src, nativeEvent)]);
            };
        }
    },
    
    connect: function (src, sig, objOrFunc/* optional */, funcOrStr) {
        src = MochiKit.DOM.getElement(src);
        var self = MochiKit.Signal;
        
        if (typeof(sig) != 'string') {
            throw new Error("'sig' must be a string");
        }
        
        var obj = null;
        var func = null;
        if (typeof(funcOrStr) != 'undefined') {
            obj = objOrFunc;
            func = funcOrStr;
            if (typeof(funcOrStr) == 'string') {
                if (typeof(objOrFunc[funcOrStr]) != "function") {
                    throw new Error("'funcOrStr' must be a function on 'objOrFunc'");
                }
            } else if (typeof(funcOrStr) != 'function') {
                throw new Error("'funcOrStr' must be a function or string");
            }
        } else if (typeof(objOrFunc) != "function") {
            throw new Error("'objOrFunc' must be a function if 'funcOrStr' is not given");
        } else {
            func = objOrFunc;
        }
        if (typeof(obj) == 'undefined' || obj === null) {
            obj = src;
        }
        
        var isDOM = !!(src.addEventListener || src.attachEvent);
        var listener = self._listener(src, func, obj, isDOM);
        
        if (src.addEventListener) {
            src.addEventListener(sig.substr(2), listener, false);
        } else if (src.attachEvent) {
            src.attachEvent(sig, listener); // useCapture unsupported
        }

        var ident = [src, sig, listener, isDOM, objOrFunc, funcOrStr];
        self._observers.push(ident);
        
       
        return ident;
    },
    
    _disconnect: function (ident) {
        // check isDOM
        if (!ident[3]) { return; }
        var src = ident[0];
        var sig = ident[1];
        var listener = ident[2];
        if (src.removeEventListener) {
            src.removeEventListener(sig.substr(2), listener, false);
        } else if (src.detachEvent) {
            src.detachEvent(sig, listener); // useCapture unsupported
        } else {
            throw new Error("'src' must be a DOM element");
        }
    },
    
    disconnect: function (ident) {
        var self = MochiKit.Signal;
        var observers = self._observers;
        var m = MochiKit.Base;
        if (arguments.length > 1) {
            // compatibility API
            var src = MochiKit.DOM.getElement(arguments[0]);
            var sig = arguments[1];
            var obj = arguments[2];
            var func = arguments[3];
            for (var i = observers.length - 1; i >= 0; i--) {
                var o = observers[i];
                if (o[0] === src && o[1] === sig && o[4] === obj && o[5] === func) {
                    self._disconnect(o);
                    observers.splice(i, 1);
                    return true;
                }
            }
        } else {
            var idx = m.findIdentical(observers, ident);
            if (idx >= 0) {
                self._disconnect(ident);
                observers.splice(idx, 1);
                return true;
            }
        }
        return false;
    },
    
    disconnectAll: function(src/* optional */, sig) {
        src = MochiKit.DOM.getElement(src);
        var m = MochiKit.Base;
        var signals = m.flattenArguments(m.extend(null, arguments, 1));
        var self = MochiKit.Signal;
        var disconnect = self._disconnect;
        var observers = self._observers;
        if (signals.length === 0) {
            // disconnect all
            for (var i = observers.length - 1; i >= 0; i--) {
                var ident = observers[i];
                if (ident[0] === src) {
                    disconnect(ident);
                    observers.splice(i, 1);
                }
            }
        } else {
            var sigs = {};
            for (var i = 0; i < signals.length; i++) {
                sigs[signals[i]] = true;
            }
            for (var i = observers.length - 1; i >= 0; i--) {
                var ident = observers[i];
                if (ident[0] === src && ident[1] in sigs) {
                    disconnect(ident);
                    observers.splice(i, 1);
                }
            }
        }
        
    },

    signal: function (src, sig) {
        var observers = MochiKit.Signal._observers;
        src = MochiKit.DOM.getElement(src);
        var args = MochiKit.Base.extend(null, arguments, 2);
        var errors = [];
        for (var i = 0; i < observers.length; i++) {
            var ident = observers[i];
            if (ident[0] === src && ident[1] === sig) {
                try {
                    ident[2].apply(src, args);
                } catch (e) {
                    errors.push(e);
                }
            }
        }
        if (errors.length == 1) {
            throw errors[0];
        } else if (errors.length > 1) {
            var e = new Error("Multiple errors thrown in handling 'sig', see errors property");
            e.errors = errors;
            throw e;
        }
    }

});

MochiKit.Signal.EXPORT_OK = [];

MochiKit.Signal.EXPORT = [
    'connect',
    'disconnect',
    'signal',
    'disconnectAll'
];

MochiKit.Signal.__new__ = function (win) {
    var m = MochiKit.Base;
    this._document = document;
    this._window = win;

    try {
        this.connect(window, 'onunload', this._unloadCache);
    } catch (e) {
        // pass: might not be a browser
    }

    this.EXPORT_TAGS = {
        ':common': this.EXPORT,
        ':all': m.concat(this.EXPORT, this.EXPORT_OK)
    };

    m.nameFunctions(this);
};

MochiKit.Signal.__new__(this);

//
// XXX: Internet Explorer blows
//
if (!MochiKit.__compat__) {
    connect = MochiKit.Signal.connect;
    disconnect = MochiKit.Signal.disconnect;
    disconnectAll = MochiKit.Signal.disconnectAll;
    signal = MochiKit.Signal.signal;
}

MochiKit.Base._exportSymbols(this, MochiKit.Signal);
