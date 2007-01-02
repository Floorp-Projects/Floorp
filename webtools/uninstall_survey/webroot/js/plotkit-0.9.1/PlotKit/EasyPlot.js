/* 
    PlotKit EasyPlot
    ================

    User friendly wrapper around the common plotting functions.

    Copyright
    ---------
    Copyright 2005,2006 (c) Alastair Tse <alastair^liquidx.net>
    For use under the BSD license. <http://www.liquidx.net/plotkit>
    
*/

try {    
    if (typeof(PlotKit.CanvasRenderer) == 'undefined')
    {
        throw ""
    }
} 
catch (e) {    
    throw "PlotKit.EasyPlot depends on all of PlotKit's components";
}

// --------------------------------------------------------------------
// Start of EasyPlot definition
// --------------------------------------------------------------------

if (typeof(PlotKit.EasyPlot) == 'undefined') {
    PlotKit.EasyPlot = {};
}

PlotKit.EasyPlot.NAME = "PlotKit.EasyPlot";
PlotKit.EasyPlot.VERSION = PlotKit.VERSION;

PlotKit.EasyPlot.__repr__ = function() {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

PlotKit.EasyPlot.toString = function() {
    return this.__repr__();
}

// --------------------------------------------------------------------
// Start of EasyPlot definition
// --------------------------------------------------------------------

PlotKit.EasyPlot = function(style, options, divElem, datasources) {
    this.layout = new Layout(style, options);
    this.divElem = divElem;
    this.width = parseInt(divElem.getAttribute('width'));
    this.height = parseInt(divElem.getAttribute('height'));
    this.deferredCount = 0;

    // make sure we have non-zero width
    if (this.width < 1) {
        this.width = this.divElem.width ? this.divElem.width : 300;
    }
    
    if (this.height < 1) {
        this.height = this.divElem.height ? this.divElem.height : 300;
    }
    
    // load data sources
    if (isArrayLike(datasources)) {
        for (var i = 0; i < datasources.length; i++) {
            if (typeof(datasources[i]) == "string") {
                this.deferredCount++;
                // load CSV via ajax
                var d = MochiKit.Async.doSimpleXMLHttpRequest(datasources[i]);
                d.addCallback(MochiKit.Base.bind(PlotKit.EasyPlot.onDataLoaded, this));
            }
            else if (isArrayLike(datasources[i])) {
                this.layout.addDataset("data-" + i, datasources[i]);
            }
        }
    }
    else if (!isUndefinedOrNull(datasources)) {
        throw "Passed datasources are not Array like";
    }
    
    // setup canvas to render
    
    if (CanvasRenderer.isSupported()) {
        this.element = CANVAS({"id": this.divElem.getAttribute("id") + "-canvas",
                               "width": this.width,
                               "height": this.height}, "");
        this.divElem.appendChild(this.element);
        this.renderer = new SweetCanvasRenderer(this.element, this.layout, options);
    }
    else if (SVGRenderer.isSupported()) {
        this.element = SVGRenderer.SVG({"id": this.divElem.getAttribute("id") + "-svg",
                                        "width": this.width,
                                        "height": this.height,
                                        "version": "1.1",
                                        "baseProfile": "full"}, "");
        this.divElem.appendChild(this.element);
        this.renderer = new SweetSVGRenderer(this.element, this.layout, options);
    }
    
    if ((this.deferredCount == 0) && (PlotKit.Base.keys(this.layout.datasets).length > 0)) {
        this.layout.evaluate();
        this.renderer.clear();
        this.renderer.render();    
    }
    
};

PlotKit.EasyPlot.onDataLoaded = function(request) {
    
    // very primitive CSV parser, should fix to make it more compliant.
    var table = new Array();
    var lines = request.responseText.split('\n');
    for (var i = 0; i < lines.length; i++) {
        var stripped = MochiKit.Format.strip(lines[i]);
        if ((stripped.length > 1) && (stripped.charAt(0) != '#')) {
            table.push(stripped.split(','));
        }
    }
  
    this.layout.addDataset("data-ajax-" + this.deferredCount, table);
    this.deferredCount--;
    
    if ((this.deferredCount == 0) && (PlotKit.Base.keys(this.layout.datasets).length > 0)) {
        this.layout.evaluate();
        this.renderer.clear();
        this.renderer.render();
    }
};

PlotKit.EasyPlot.prototype.reload = function() {
    this.layout.evaluate();
    this.renderer.clear();
    this.renderer.render();
};

// Namespace Iniitialisation

PlotKit.EasyPlotModule = {};
PlotKit.EasyPlotModule.EasyPlot = PlotKit.EasyPlot;

PlotKit.EasyPlotModule.EXPORT = [
    "EasyPlot"
];

PlotKit.EasyPlotModule.EXPORT_OK = [];

PlotKit.EasyPlotModule.__new__ = function() {
    var m = MochiKit.Base;
    
    m.nameFunctions(this);
    
    this.EXPORT_TAGS = {
        ":common": this.EXPORT,
        ":all": m.concat(this.EXPORT, this.EXPORT_OK)
    };
};

PlotKit.EasyPlotModule.__new__();
MochiKit.Base._exportSymbols(this, PlotKit.EasyPlotModule);


