dojo.hostenv.conditionalLoadModule({
    "common": [
        "MochiKit.Base",
        "MochiKit.Iter",
        "MochiKit.Logging",
        "MochiKit.DateTime",
        "MochiKit.Format",
        "MochiKit.Async",
        "MochiKit.Color"
    ],
    "browser": [
        "MochiKit.DOM",
        "MochiKit.Style",
        "MochiKit.Signal",
        "MochiKit.LoggingPane",
        "MochiKit.Visual"
    ]
});
dojo.hostenv.moduleLoaded("MochiKit.*");
