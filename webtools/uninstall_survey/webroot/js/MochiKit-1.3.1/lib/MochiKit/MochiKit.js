/***

MochiKit.MochiKit 1.3.1

See <http://mochikit.com/> for documentation, downloads, license, etc.

(c) 2005 Bob Ippolito.  All rights Reserved.

***/

if (typeof(MochiKit) == 'undefined') {
    MochiKit = {};
}

if (typeof(MochiKit.MochiKit) == 'undefined') {
    MochiKit.MochiKit = {};
}

MochiKit.MochiKit.NAME = "MochiKit.MochiKit";
MochiKit.MochiKit.VERSION = "1.3.1";
MochiKit.MochiKit.__repr__ = function () {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

MochiKit.MochiKit.toString = function () {
    return this.__repr__();
};

MochiKit.MochiKit.SUBMODULES = [
    "Base",
    "Iter",
    "Logging",
    "DateTime",
    "Format",
    "Async",
    "DOM",
    "LoggingPane",
    "Color",
    "Signal",
    "Visual"
];

if (typeof(JSAN) != 'undefined' || typeof(dojo) != 'undefined') {
    if (typeof(dojo) != 'undefined') {
        dojo.provide('MochiKit.MochiKit');
        dojo.require("MochiKit.*");
    }
    if (typeof(JSAN) != 'undefined') {
        // hopefully this makes it easier for static analysis?
        JSAN.use("MochiKit.Base", []);
        JSAN.use("MochiKit.Iter", []);
        JSAN.use("MochiKit.Logging", []);
        JSAN.use("MochiKit.DateTime", []);
        JSAN.use("MochiKit.Format", []);
        JSAN.use("MochiKit.Async", []);
        JSAN.use("MochiKit.DOM", []);
        JSAN.use("MochiKit.LoggingPane", []);
        JSAN.use("MochiKit.Color", []);
        JSAN.use("MochiKit.Signal", []);
        JSAN.use("MochiKit.Visual", []);
    }
    (function () {
        var extend = MochiKit.Base.extend;
        var self = MochiKit.MochiKit;
        var modules = self.SUBMODULES;
        var EXPORT = [];
        var EXPORT_OK = [];
        var EXPORT_TAGS = {};
        var i, k, m, all;
        for (i = 0; i < modules.length; i++) {
            m = MochiKit[modules[i]];
            extend(EXPORT, m.EXPORT);
            extend(EXPORT_OK, m.EXPORT_OK);
            for (k in m.EXPORT_TAGS) {
                EXPORT_TAGS[k] = extend(EXPORT_TAGS[k], m.EXPORT_TAGS[k]);
            }
            all = m.EXPORT_TAGS[":all"];
            if (!all) {
                all = extend(null, m.EXPORT, m.EXPORT_OK);
            }
            var j;
            for (j = 0; j < all.length; j++) {
                k = all[j];
                self[k] = m[k];
            }
        }
        self.EXPORT = EXPORT;
        self.EXPORT_OK = EXPORT_OK;
        self.EXPORT_TAGS = EXPORT_TAGS;
    }());
    
} else {
    if (typeof(MochiKit.__compat__) == 'undefined') {
        MochiKit.__compat__ = true;
    }
    (function () {
        var scripts = document.getElementsByTagName("script");
        var kXULNSURI = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
        var base = null;
        var baseElem = null;
        var allScripts = {};
        var i;
        for (i = 0; i < scripts.length; i++) {
            var src = scripts[i].getAttribute("src");
            if (!src) {
                continue;
            }
            allScripts[src] = true;
            if (src.match(/MochiKit.js$/)) {
                base = src.substring(0, src.lastIndexOf('MochiKit.js'));
                baseElem = scripts[i];
            }
        }
        if (base === null) {
            return;
        }
        var modules = MochiKit.MochiKit.SUBMODULES;
        for (var i = 0; i < modules.length; i++) {
            if (MochiKit[modules[i]]) {
                continue;
            }
            var uri = base + modules[i] + '.js';
            if (uri in allScripts) {
                continue;
            }
            if (document.documentElement &&
                document.documentElement.namespaceURI == kXULNSURI) {
                // XUL
                var s = document.createElementNS(kXULNSURI, 'script');
                s.setAttribute("id", "MochiKit_" + base + modules[i]);
                s.setAttribute("src", uri);
                s.setAttribute("type", "application/x-javascript");
                baseElem.parentNode.appendChild(s);
            } else {
                // HTML
                /*
                    DOM can not be used here because Safari does
                    deferred loading of scripts unless they are
                    in the document or inserted with document.write

                    This is not XHTML compliant.  If you want XHTML
                    compliance then you must use the packed version of MochiKit
                    or include each script individually (basically unroll
                    these document.write calls into your XHTML source)

                */
                document.write('<script src="' + uri +
                    '" type="text/javascript"></script>');
            }
        };
    })();
}
