var roundedCornersOnLoad = function () {
    swapDOM("visual_version", SPAN(null, MochiKit.Visual.VERSION));
    roundClass("h1", null);
    roundClass("h2", null, {corners: "bottom"});
};
addLoadEvent(roundedCornersOnLoad);

// rewrite the view-source links
addLoadEvent(function () {
    var elems = getElementsByTagAndClassName("A", "view-source");
    var page = "rounded_corners/";
    for (var i = 0; i < elems.length; i++) {
        var elem = elems[i];
        var href = elem.href.split(/\//).pop();
        elem.target = "_blank";
        elem.href = "../view-source/view-source.html#" + page + href;
    }
});

