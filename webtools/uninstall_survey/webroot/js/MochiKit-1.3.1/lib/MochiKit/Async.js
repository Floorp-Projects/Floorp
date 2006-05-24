/***

MochiKit.Async 1.3.1

See <http://mochikit.com/> for documentation, downloads, license, etc.

(c) 2005 Bob Ippolito.  All rights Reserved.

***/

if (typeof(dojo) != 'undefined') {
    dojo.provide("MochiKit.Async");
    dojo.require("MochiKit.Base");
}
if (typeof(JSAN) != 'undefined') {
    JSAN.use("MochiKit.Base", []);
}

try {
    if (typeof(MochiKit.Base) == 'undefined') {
        throw "";
    }
} catch (e) {
    throw "MochiKit.Async depends on MochiKit.Base!";
}

if (typeof(MochiKit.Async) == 'undefined') {
    MochiKit.Async = {};
}

MochiKit.Async.NAME = "MochiKit.Async";
MochiKit.Async.VERSION = "1.3.1";
MochiKit.Async.__repr__ = function () {
    return "[" + this.NAME + " " + this.VERSION + "]";
};
MochiKit.Async.toString = function () {
    return this.__repr__();
};

MochiKit.Async.Deferred = function (/* optional */ canceller) {
    this.chain = [];
    this.id = this._nextId();
    this.fired = -1;
    this.paused = 0;
    this.results = [null, null];
    this.canceller = canceller;
    this.silentlyCancelled = false;
    this.chained = false;
};

MochiKit.Async.Deferred.prototype = {
    repr: function () {
        var state;
        if (this.fired == -1) {
            state = 'unfired';
        } else if (this.fired === 0) {
            state = 'success';
        } else {
            state = 'error';
        }
        return 'Deferred(' + this.id + ', ' + state + ')';
    },

    toString: MochiKit.Base.forwardCall("repr"),

    _nextId: MochiKit.Base.counter(),

    cancel: function () {
        var self = MochiKit.Async;
        if (this.fired == -1) {
            if (this.canceller) {
                this.canceller(this);
            } else {
                this.silentlyCancelled = true;
            }
            if (this.fired == -1) {
                this.errback(new self.CancelledError(this));
            }
        } else if ((this.fired === 0) && (this.results[0] instanceof self.Deferred)) {
            this.results[0].cancel();
        }
    },
            

    _pause: function () {
        /***

        Used internally to signal that it's waiting on another Deferred

        ***/
        this.paused++;
    },

    _unpause: function () {
        /***

        Used internally to signal that it's no longer waiting on another
        Deferred.

        ***/
        this.paused--;
        if ((this.paused === 0) && (this.fired >= 0)) {
            this._fire();
        }
    },

    _continue: function (res) {
        /***

        Used internally when a dependent deferred fires.

        ***/
        this._resback(res);
        this._unpause();
    },

    _resback: function (res) {
        /***

        The primitive that means either callback or errback

        ***/
        this.fired = ((res instanceof Error) ? 1 : 0);
        this.results[this.fired] = res;
        this._fire();
    },

    _check: function () {
        if (this.fired != -1) {
            if (!this.silentlyCancelled) {
                throw new MochiKit.Async.AlreadyCalledError(this);
            }
            this.silentlyCancelled = false;
            return;
        }
    },

    callback: function (res) {
        this._check();
        if (res instanceof MochiKit.Async.Deferred) {
            throw new Error("Deferred instances can only be chained if they are the result of a callback");
        }
        this._resback(res);
    },

    errback: function (res) {
        this._check();
        var self = MochiKit.Async;
        if (res instanceof self.Deferred) {
            throw new Error("Deferred instances can only be chained if they are the result of a callback");
        }
        if (!(res instanceof Error)) {
            res = new self.GenericError(res);
        }
        this._resback(res);
    },

    addBoth: function (fn) {
        if (arguments.length > 1) {
            fn = MochiKit.Base.partial.apply(null, arguments);
        }
        return this.addCallbacks(fn, fn);
    },

    addCallback: function (fn) {
        if (arguments.length > 1) {
            fn = MochiKit.Base.partial.apply(null, arguments);
        }
        return this.addCallbacks(fn, null);
    },

    addErrback: function (fn) {
        if (arguments.length > 1) {
            fn = MochiKit.Base.partial.apply(null, arguments);
        }
        return this.addCallbacks(null, fn);
    },

    addCallbacks: function (cb, eb) {
        if (this.chained) {
            throw new Error("Chained Deferreds can not be re-used");
        }
        this.chain.push([cb, eb]);
        if (this.fired >= 0) {
            this._fire();
        }
        return this;
    },

    _fire: function () {
        /***

        Used internally to exhaust the callback sequence when a result
        is available.

        ***/
        var chain = this.chain;
        var fired = this.fired;
        var res = this.results[fired];
        var self = this;
        var cb = null;
        while (chain.length > 0 && this.paused === 0) {
            // Array
            var pair = chain.shift();
            var f = pair[fired];
            if (f === null) {
                continue;
            }
            try {
                res = f(res);
                fired = ((res instanceof Error) ? 1 : 0);
                if (res instanceof MochiKit.Async.Deferred) {
                    cb = function (res) {
                        self._continue(res);
                    };
                    this._pause();
                }
            } catch (err) {
                fired = 1;
                if (!(err instanceof Error)) {
                    err = new MochiKit.Async.GenericError(err);
                }
                res = err;
            }
        }
        this.fired = fired;
        this.results[fired] = res;
        if (cb && this.paused) {
            // this is for "tail recursion" in case the dependent deferred
            // is already fired
            res.addBoth(cb);
            res.chained = true;
        }
    }
};

MochiKit.Base.update(MochiKit.Async, {
    evalJSONRequest: function (/* req */) {
        return eval('(' + arguments[0].responseText + ')');
    },

    succeed: function (/* optional */result) {
        var d = new MochiKit.Async.Deferred();
        d.callback.apply(d, arguments);
        return d;
    },

    fail: function (/* optional */result) {
        var d = new MochiKit.Async.Deferred();
        d.errback.apply(d, arguments);
        return d;
    },

    getXMLHttpRequest: function () {
        var self = arguments.callee;
        if (!self.XMLHttpRequest) {
            var tryThese = [
                function () { return new XMLHttpRequest(); },
                function () { return new ActiveXObject('Msxml2.XMLHTTP'); },
                function () { return new ActiveXObject('Microsoft.XMLHTTP'); },
                function () { return new ActiveXObject('Msxml2.XMLHTTP.4.0'); },
                function () {
                    throw new MochiKit.Async.BrowserComplianceError("Browser does not support XMLHttpRequest");
                }
            ];
            for (var i = 0; i < tryThese.length; i++) {
                var func = tryThese[i];
                try {
                    self.XMLHttpRequest = func;
                    return func();
                } catch (e) {
                    // pass
                }
            }
        }
        return self.XMLHttpRequest();
    },

    _nothing: function () {},

    _xhr_onreadystatechange: function (d) {
        // MochiKit.Logging.logDebug('this.readyState', this.readyState);
        if (this.readyState == 4) {
            // IE SUCKS
            try {
                this.onreadystatechange = null;
            } catch (e) {
                try {
                    this.onreadystatechange = MochiKit.Async._nothing;
                } catch (e) {
                }
            }
            var status = null;
            try {
                status = this.status;
                if (!status && MochiKit.Base.isNotEmpty(this.responseText)) {
                    // 0 or undefined seems to mean cached or local
                    status = 304;
                }
            } catch (e) {
                // pass
                // MochiKit.Logging.logDebug('error getting status?', repr(items(e)));
            }
            //  200 is OK, 304 is NOT_MODIFIED
            if (status == 200 || status == 304) { // OK
                d.callback(this);
            } else {
                var err = new MochiKit.Async.XMLHttpRequestError(this, "Request failed");
                if (err.number) {
                    // XXX: This seems to happen on page change
                    d.errback(err);
                } else {
                    // XXX: this seems to happen when the server is unreachable
                    d.errback(err);
                }
            }
        }
    },

    _xhr_canceller: function (req) {
        // IE SUCKS
        try {
            req.onreadystatechange = null;
        } catch (e) {
            try {
                req.onreadystatechange = MochiKit.Async._nothing;
            } catch (e) {
            }
        }
        req.abort();
    },

    
    sendXMLHttpRequest: function (req, /* optional */ sendContent) {
        if (typeof(sendContent) == "undefined" || sendContent === null) {
            sendContent = "";
        }

        var m = MochiKit.Base;
        var self = MochiKit.Async;
        var d = new self.Deferred(m.partial(self._xhr_canceller, req));
        
        try {
            req.onreadystatechange = m.bind(self._xhr_onreadystatechange,
                req, d);
            req.send(sendContent);
        } catch (e) {
            try {
                req.onreadystatechange = null;
            } catch (ignore) {
                // pass
            }
            d.errback(e);
        }

        return d;

    },

    doSimpleXMLHttpRequest: function (url/*, ...*/) {
        var self = MochiKit.Async;
        var req = self.getXMLHttpRequest();
        if (arguments.length > 1) {
            var m = MochiKit.Base;
            var qs = m.queryString.apply(null, m.extend(null, arguments, 1));
            if (qs) {
                url += "?" + qs;
            }
        }
        req.open("GET", url, true);
        return self.sendXMLHttpRequest(req);
    },

    loadJSONDoc: function (url) {
        var self = MochiKit.Async;
        var d = self.doSimpleXMLHttpRequest.apply(self, arguments);
        d = d.addCallback(self.evalJSONRequest);
        return d;
    },

    wait: function (seconds, /* optional */value) {
        var d = new MochiKit.Async.Deferred();
        var m = MochiKit.Base;
        if (typeof(value) != 'undefined') {
            d.addCallback(function () { return value; });
        }
        var timeout = setTimeout(
            m.bind("callback", d),
            Math.floor(seconds * 1000));
        d.canceller = function () {
            try {
                clearTimeout(timeout);
            } catch (e) {
                // pass
            }
        };
        return d;
    },

    callLater: function (seconds, func) {
        var m = MochiKit.Base;
        var pfunc = m.partial.apply(m, m.extend(null, arguments, 1));
        return MochiKit.Async.wait(seconds).addCallback(
            function (res) { return pfunc(); }
        );
    }
});


MochiKit.Async.DeferredLock = function () {
    this.waiting = [];
    this.locked = false;
    this.id = this._nextId();
};

MochiKit.Async.DeferredLock.prototype = {
    __class__: MochiKit.Async.DeferredLock,
    acquire: function () {
        d = new MochiKit.Async.Deferred();
        if (this.locked) {
            this.waiting.push(d);
        } else {
            this.locked = true;
            d.callback(this);
        }
        return d;
    },
    release: function () {
        if (!this.locked) {
            throw TypeError("Tried to release an unlocked DeferredLock");
        }
        this.locked = false;
        if (this.waiting.length > 0) {
            this.locked = true;
            this.waiting.shift().callback(this);
        }
    },
    _nextId: MochiKit.Base.counter(),
    repr: function () {
        var state;
        if (this.locked) {
            state = 'locked, ' + this.waiting.length + ' waiting';
        } else {
            state = 'unlocked';
        }
        return 'DeferredLock(' + this.id + ', ' + state + ')';
    },
    toString: MochiKit.Base.forwardCall("repr")

};

MochiKit.Async.DeferredList = function (list, /* optional */fireOnOneCallback, fireOnOneErrback, consumeErrors, canceller) {
    this.list = list;
    this.resultList = new Array(this.list.length);

    // Deferred init
    this.chain = [];
    this.id = this._nextId();
    this.fired = -1;
    this.paused = 0;
    this.results = [null, null];
    this.canceller = canceller;
    this.silentlyCancelled = false;
    
    if (this.list.length === 0 && !fireOnOneCallback) {
        this.callback(this.resultList);
    }
    
    this.finishedCount = 0;
    this.fireOnOneCallback = fireOnOneCallback;
    this.fireOnOneErrback = fireOnOneErrback;
    this.consumeErrors = consumeErrors;

    var index = 0;
    MochiKit.Base.map(MochiKit.Base.bind(function (d) {
        d.addCallback(MochiKit.Base.bind(this._cbDeferred, this), index, true);
        d.addErrback(MochiKit.Base.bind(this._cbDeferred, this), index, false);
        index += 1;
    }, this), this.list);
};

MochiKit.Base.update(MochiKit.Async.DeferredList.prototype,
                     MochiKit.Async.Deferred.prototype);

MochiKit.Base.update(MochiKit.Async.DeferredList.prototype, {
    _cbDeferred: function (index, succeeded, result) {
        this.resultList[index] = [succeeded, result];
        this.finishedCount += 1;
        if (this.fired !== 0) {
            if (succeeded && this.fireOnOneCallback) {
                this.callback([index, result]);
            } else if (!succeeded && this.fireOnOneErrback) {
                this.errback(result);
            } else if (this.finishedCount == this.list.length) {
                this.callback(this.resultList);
            }
        }
        if (!succeeded && this.consumeErrors) {
            result = null;
        }
        return result;
    }
});

MochiKit.Async.gatherResults = function (deferredList) {
    var d = new MochiKit.Async.DeferredList(deferredList, false, true, false);
    d.addCallback(function (results) {
        var ret = [];
        for (var i = 0; i < results.length; i++) {
            ret.push(results[i][1]);
        }
        return ret;
    });
    return d;
};

MochiKit.Async.maybeDeferred = function (func) {
    var self = MochiKit.Async;
    var result;
    try {
        var r = func.apply(null, MochiKit.Base.extend([], arguments, 1));
        if (r instanceof self.Deferred) {
            result = r;
        } else if (r instanceof Error) {
            result = self.fail(r);
        } else {
            result = self.succeed(r);
        }
    } catch (e) {
        result = self.fail(e);
    }
    return result;
};


MochiKit.Async.EXPORT = [
    "AlreadyCalledError",
    "CancelledError",
    "BrowserComplianceError",
    "GenericError",
    "XMLHttpRequestError",
    "Deferred",
    "succeed",
    "fail",
    "getXMLHttpRequest",
    "doSimpleXMLHttpRequest",
    "loadJSONDoc",
    "wait",
    "callLater",
    "sendXMLHttpRequest",
    "DeferredLock",
    "DeferredList",
    "gatherResults",
    "maybeDeferred"
];
    
MochiKit.Async.EXPORT_OK = [
    "evalJSONRequest"
];

MochiKit.Async.__new__ = function () {
    var m = MochiKit.Base;
    var ne = m.partial(m._newNamedError, this);
    ne("AlreadyCalledError", 
        function (deferred) {
            /***

            Raised by the Deferred if callback or errback happens
            after it was already fired.

            ***/
            this.deferred = deferred;
        }
    );

    ne("CancelledError",
        function (deferred) {
            /***

            Raised by the Deferred cancellation mechanism.

            ***/
            this.deferred = deferred;
        }
    );

    ne("BrowserComplianceError",
        function (msg) {
            /***

            Raised when the JavaScript runtime is not capable of performing
            the given function.  Technically, this should really never be
            raised because a non-conforming JavaScript runtime probably
            isn't going to support exceptions in the first place.

            ***/
            this.message = msg;
        }
    );

    ne("GenericError", 
        function (msg) {
            this.message = msg;
        }
    );

    ne("XMLHttpRequestError",
        function (req, msg) {
            /***

            Raised when an XMLHttpRequest does not complete for any reason.

            ***/
            this.req = req;
            this.message = msg;
            try {
                // Strange but true that this can raise in some cases.
                this.number = req.status;
            } catch (e) {
                // pass
            }
        }
    );


    this.EXPORT_TAGS = {
        ":common": this.EXPORT,
        ":all": m.concat(this.EXPORT, this.EXPORT_OK)
    };

    m.nameFunctions(this);

};

MochiKit.Async.__new__();

MochiKit.Base._exportSymbols(this, MochiKit.Async);
