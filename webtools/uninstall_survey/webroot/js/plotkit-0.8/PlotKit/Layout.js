/* 
    PlotKit Layout
    ==============
    
    Handles laying out data on to a virtual canvas square canvas between 0.0 
    and 1.0. If you want to add new chart/plot types such as point plots,
    you need to add them here.
    
    Copyright
    ---------
    Copyright 2005,2006 (c) Alastair Tse <alastair^liquidx.net>
    For use under the BSD license. <http://www.liquidx.net/plotkit>
    
*/

try {    
    if (typeof(PlotKit.Base) == 'undefined')
    {
        throw ""
    }
} 
catch (e) {    
    throw "PlotKit.Layout depends on MochiKit.{Base,Color,DOM,Format} and PlotKit.Base"
}

// --------------------------------------------------------------------
// Start of Layout definition
// --------------------------------------------------------------------

if (typeof(PlotKit.Layout) == 'undefined') {
    PlotKit.Layout = {};
}

PlotKit.Layout.NAME = "PlotKit.Layout";
PlotKit.Layout.VERSION = PlotKit.VERSION;

PlotKit.Layout.__repr__ = function() {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

PlotKit.Layout.toString = function() {
    return this.__repr__();
}

PlotKit.Layout.valid_styles = ["bar", "line", "pie", "point"];

// --------------------------------------------------------------------
// Start of Layout definition
// --------------------------------------------------------------------

PlotKit.Layout = function(style, options) {
    
    this.options = {
        "barWidthFillFraction": 0.75,
        "barOrientation": "vertical",
        "xOriginIsZero": true,
        "yOriginIsZero": true,
        "xAxis": null, // [xmin, xmax]
        "yAxis": null, // [ymin, ymax]
        "xTicks": null, // [{label: "somelabel", v: value}, ..] (label opt.)
        "yTicks": null, // [{label: "somelabel", v: value}, ..] (label opt.)
        "xNumberOfTicks": 10,
        "yNumberOfTicks": 5,
        "xTickPrecision": 1,
        "yTickPrecision": 3,
        "pieRadius": 0.4
    };

    // valid external options : TODO: input verification
    this.style = style; 
    MochiKit.Base.update(this.options, options ? options : {});

    // externally visible states
    // overriden if xAxis and yAxis are set in options
    if (!MochiKit.Base.isUndefinedOrNull(this.options.xAxis)) {
        this.minxval = this.options.xAxis[0];
        this.maxxval = this.options.xAxis[1];
        this.xscale = this.maxxval - this.minxval; 
    }
    else {
        this.minxval = 0;
        this.maxxval = null;
        this.xscale = null; // val -> pos factor (eg, xval * xscale = xpos)
    }

    if (!MochiKit.Base.isUndefinedOrNull(this.options.yAxis)) {
        this.minyval = this.options.yAxis[0];
        this.maxyval = this.options.yAxis[1];
        this.yscale = this.maxyval - this.maxymin;
    }
    else {
        this.minyval = 0;
        this.maxyval = null;
        this.yscale = null;
    }

    this.bars = new Array();   // array of bars to plot for bar charts
    this.points = new Array(); // array of points to plot for line plots
    this.slices = new Array(); // array of slices to draw for pie charts

    this.xticks = new Array();
    this.yticks = new Array();

    // internal states
    this.datasets = new Array();
    this.minxdelta = 0;
    this.xrange = 1;
    this.yrange = 1;

    this.hitTestCache = {x2maxy: null};
    
};

// --------------------------------------------------------------------
// Dataset Manipulation
// --------------------------------------------------------------------


PlotKit.Layout.prototype.addDataset = function(setname, set_xy) {
    this.datasets[setname] = set_xy;
};

PlotKit.Layout.prototype.removeDataset = function(setname, set_xy) {
    this.datasets[setname] = null;
};

PlotKit.Layout.prototype.addDatasetFromTable = function(name, tableElement, xcol, ycol) {
    var isNil = MochiKit.Base.isUndefinedOrNull;
    var scrapeText = MochiKit.DOM.scrapeText;
    var strip = MochiKit.Format.strip;
    
    if (isNil(xcol))
        xcol = 0;
    if (isNil(ycol))
        ycol = 1;
        
    var rows = tableElement.tBodies[0].rows;
    var data = new Array();
    if (!isNil(rows)) {
        for (var i = 0; i < rows.length; i++) {
            data.push([parseFloat(strip(scrapeText(rows[i].cells[xcol]))),
                       parseFloat(strip(scrapeText(rows[i].cells[ycol])))]);
        }
        this.addDataset(name, data);
        return true;
    }
    return false;
};

// --------------------------------------------------------------------
// Evaluates the layout for the current data and style.
// --------------------------------------------------------------------

PlotKit.Layout.prototype.evaluate = function() {
    this._evaluateLimits();
    this._evaluateScales();
    if (this.style == "bar") {
        if (this.options.barOrientation == "horizontal") {
            this._evaluateHorizBarCharts();
        }
        else {
            this._evaluateBarCharts();
        }
        this._evaluateBarTicks();
    }
    else if (this.style == "line") {
        this._evaluateLineCharts();
        this._evaluateLineTicks();
    }
    else if (this.style == "pie") {
        this._evaluatePieCharts();
        this._evaluatePieTicks();
    }
};



// Given the fractional x, y positions, report the corresponding
// x, y values.
PlotKit.Layout.prototype.hitTest = function(x, y) {
    // TODO: make this more efficient with better datastructures
    //       for this.bars, this.points and this.slices

    var f = MochiKit.Format.twoDigitFloat;

    if ((this.style == "bar") && this.bars && (this.bars.length > 0)) {
        for (var i = 0; i < this.bars.length; i++) {
            var bar = this.bars[i];
            if ((x >= bar.x) && (x <= bar.x + bar.w) 
                && (y >= bar.y) && (y - bar.y <= bar.h))
                return bar;
        }
    }

    else if (this.style == "line") {
        if (this.hitTestCache.x2maxy == null) {
            this._regenerateHitTestCache();
        }

        // 1. find the xvalues that equal or closest to the give x
        var xval = x / this.xscale;
        var xvalues = this.hitTestCache.xvalues;
        var xbefore = null;
        var xafter = null;

        for (var i = 1; i < xvalues.length; i++) {
            if (xvalues[i] > xval) {
                xbefore = xvalues[i-1];
                xafter = xvalues[i];
                break;
            }
        }

        if ((xbefore != null)) {
            var ybefore = this.hitTestCache.x2maxy[xbefore];
            var yafter = this.hitTestCache.x2maxy[xafter];
            var yval = (1.0 - y)/this.yscale;

            // interpolate whether we will fall inside or outside
            var gradient = (yafter - ybefore) / (xafter - xbefore);
            var projmaxy = ybefore + gradient * (xval - xbefore);
            if (projmaxy >= yval) {
                // inside the highest curve (roughly)
                var obj = {xval: xval, yval: yval,
                           xafter: xafter, yafter: yafter,
                           xbefore: xbefore, ybefore: ybefore,
                           yprojected: projmaxy
                };
                return obj;
            }
        }
    }

    else if (this.style == "pie") {
        var dist = Math.sqrt((y-0.5)*(y-0.5) + (x-0.5)*(x-0.5));
        if (dist > this.options.pieRadius)
            return null;

        // TODO: actually doesn't work if we don't know how the Canvas
        //       lays it out, need to fix!
        var angle = Math.atan2(y - 0.5, x - 0.5) - Math.PI/2;
        for (var i = 0; i < this.slices.length; i++) {
            var slice = this.slices[i];
            if (slice.startAngle < angle && slice.endAngle >= angle)
                return slice;
        }
    }

    return null;
};

// Reports valid position rectangle for X value (only valid for bar charts)
PlotKit.Layout.prototype.rectForX = function(x) {
    return null;
};

// Reports valid angles through which X value encloses (only valid for pie charts)
PlotKit.Layout.prototype.angleRangeForX = function(x) {
    return null;
};

// --------------------------------------------------------------------
// START Internal Functions
// --------------------------------------------------------------------

PlotKit.Layout.prototype._evaluateLimits = function() {
    // take all values from all datasets and find max and min
    var map = MochiKit.Base.map;
    var items = MochiKit.Base.items;
    var itemgetter = MochiKit.Base.itemgetter;
    var collapse = PlotKit.Base.collapse;
    var listMin = MochiKit.Base.listMin;
    var listMax = MochiKit.Base.listMax;
    var isNil = MochiKit.Base.isUndefinedOrNull;

    var all = collapse(map(itemgetter(1), items(this.datasets)));

    if (isNil(this.options.xAxis)) {
        if (this.options.xOriginIsZero)
            this.minxval = 0;
        else
            this.minxval = listMin(map(parseFloat, map(itemgetter(0), all)));
    }
    
    if (isNil(this.options.yAxis)) {
        if (this.options.yOriginIsZero)
            this.minyval = 0;
        else
            this.minyval = listMin(map(parseFloat, map(itemgetter(1), all)));
    }

    this.maxxval = listMax(map(parseFloat, map(itemgetter(0), all)));
    this.maxyval = listMax(map(parseFloat, map(itemgetter(1), all)));
};

PlotKit.Layout.prototype._evaluateScales = function() {
    var isNil = MochiKit.Base.isUndefinedOrNull;

    this.xrange = this.maxxval - this.minxval;
    if (this.xrange == 0)
        this.xscale = 1.0;
    else
        this.xscale = 1/this.xrange;

    this.yrange = this.maxyval - this.minyval;
    if (this.yrange == 0)
        this.yscale = 1.0;
    else
        this.yscale = 1/this.yrange;
};

PlotKit.Layout.prototype._uniqueXValues = function() {
    var collapse = PlotKit.Base.collapse;
    var map = MochiKit.Base.map;
    var uniq = PlotKit.Base.uniq;
    var getter = MochiKit.Base.itemgetter;

    var xvalues = map(parseFloat, map(getter(0), collapse(map(getter(1), items(this.datasets)))));
    xvalues.sort(MochiKit.Base.compare);
    return uniq(xvalues);
};

// Create the bars
PlotKit.Layout.prototype._evaluateBarCharts = function() {
    var keys = MochiKit.Base.keys;
    var items = MochiKit.Base.items;

    var setCount = keys(this.datasets).length;

    // work out how far separated values are
    var xdelta = 10000000;
    var xvalues = this._uniqueXValues();
    for (var i = 1; i < xvalues.length; i++) {
        xdelta = Math.min(Math.abs(xvalues[i] - xvalues[i-1]), xdelta);
    }

    var barWidth = 0;
    var barWidthForSet = 0;
    var barMargin = 0;
    if (xvalues.length == 1) {
        // note we have to do something smarter if we only plot one value
        xdelta = 1.0;
        this.xscale = 1.0;
        this.minxval = xvalues[0];
        barWidth = 1.0 * this.options.barWidthFillFraction;
        barWidthForSet = barWidth/setCount;
        barMargin = (1.0 - this.options.barWidthFillFraction)/2;
    }
    else {
        // readjust xscale to fix with bar charts
        this.xscale = (1.0 - xdelta/this.xrange)/this.xrange;
        barWidth = xdelta * this.xscale * this.options.barWidthFillFraction;
        barWidthForSet = barWidth / setCount;
        barMargin = xdelta * this.xscale * (1.0 - this.options.barWidthFillFraction)/2;
    }

    this.minxdelta = xdelta; // need this for tick positions

    // add all the rects
    this.bars = new Array();
    var i = 0;
    for (var setName in this.datasets) {
        var dataset = this.datasets[setName];
        for (var j = 0; j < dataset.length; j++) {
            var item = dataset[j];
            var rect = {
                x: ((parseFloat(item[0]) - this.minxval) * this.xscale) + (i * barWidthForSet) + barMargin,
                y: 1.0 - ((parseFloat(item[1]) - this.minyval) * this.yscale),
                w: barWidthForSet,
                h: ((parseFloat(item[1]) - this.minyval) * this.yscale),
                xval: parseFloat(item[0]),
                yval: parseFloat(item[1]),
                name: setName
            };
            this.bars.push(rect);
        }
        i++;
    }
};

// Create the horizontal bars
PlotKit.Layout.prototype._evaluateHorizBarCharts = function() {
    var keys = MochiKit.Base.keys;
    var items = MochiKit.Base.items;

    var setCount = keys(this.datasets).length;

    // work out how far separated values are
    var xdelta = 10000000;
    var xvalues = this._uniqueXValues();
    for (var i = 1; i < xvalues.length; i++) {
        xdelta = Math.min(Math.abs(xvalues[i] - xvalues[i-1]), xdelta);
    }

    var barWidth = 0;
    var barWidthForSet = 0;
    var barMargin = 0;
    
    // work out how far each far each bar is separated
    if (xvalues.length == 1) {
        // do something smarter if we only plot one value
        xdelta = 1.0;
        this.xscale = 1.0;
        this.minxval = xvalues[0];
        barWidth = 1.0 * this.options.barWidthFillFraction;
        barWidthForSet = barWidth/setCount;
        barMargin = (1.0 - this.options.barWidthFillFraction)/2;
    }
    else {
        // readjust yscale to fix with bar charts
        this.xscale = (1.0 - xdelta/this.xrange)/this.xrange;
        barWidth = xdelta * this.xscale * this.options.barWidthFillFraction;
        barWidthForSet = barWidth / setCount;
        barMargin = xdelta * this.xscale * (1.0 - this.options.barWidthFillFraction)/2;
    }

    this.minxdelta = xdelta; // need this for tick positions

    // add all the rects
    this.bars = new Array();
    var i = 0;
    for (var setName in this.datasets) {
        var dataset = this.datasets[setName];
        for (var j = 0; j < dataset.length; j++) {
            var item = dataset[j];
            var rect = {
                y: ((parseFloat(item[0]) - this.minxval) * this.xscale) + (i * barWidthForSet) + barMargin,
                x: 0.0,
                h: barWidthForSet,
                w: ((parseFloat(item[1]) - this.minyval) * this.yscale),
                xval: parseFloat(item[0]),
                yval: parseFloat(item[1]),
                label: item[2],
                name: setName
            };
            this.bars.push(rect);
        }
        i++;
    }
};


// Create the line charts
PlotKit.Layout.prototype._evaluateLineCharts = function() {
    var keys = MochiKit.Base.keys;
    var items = MochiKit.Base.items;

    var setCount = keys(this.datasets).length;

    // add all the rects
    this.points = new Array();
    var i = 0;
    for (var setName in this.datasets) {
        var dataset = this.datasets[setName];
        dataset.sort(function(a, b) { return compare(parseFloat(a[0]), parseFloat(b[0])); });
        for (var j = 0; j < dataset.length; j++) {
            var item = dataset[j];
            var point = {
                x: ((parseFloat(item[0]) - this.minxval) * this.xscale),
                y: 1.0 - ((parseFloat(item[1]) - this.minyval) * this.yscale),
                xval: parseFloat(item[0]),
                yval: parseFloat(item[1]),
                name: setName
            };
            this.points.push(point);
        }
        i++;
    }
};

// Create the pie charts
PlotKit.Layout.prototype._evaluatePieCharts = function() {
    var items = MochiKit.Base.items;
    var sum = MochiKit.Iter.sum;
    var getter = MochiKit.Base.itemgetter;

    var setCount = keys(this.datasets).length;

    // we plot the y values of the first dataset
    var dataset = items(this.datasets)[0][1];
    var total = sum(map(getter(1), dataset));

    this.slices = new Array();
    var currentAngle = 0.0;
    for (var i = 0; i < dataset.length; i++) {
        var fraction = dataset[i][1] / total;
        var startAngle = currentAngle * Math.PI * 2;
        var endAngle = (currentAngle + fraction) * Math.PI * 2;
            
        var slice = {fraction: fraction,
                     xval: dataset[i][0],
                     yval: dataset[i][1],
                     startAngle: startAngle,
                     endAngle: endAngle
        };
        this.slices.push(slice);
        currentAngle += fraction;
    }
};

PlotKit.Layout.prototype._evaluateLineTicksForXAxis = function() {
    var isNil = MochiKit.Base.isUndefinedOrNull;
    
    if (this.options.xTicks) {
        // we use use specified ticks with optional labels

        this.xticks = new Array();
        var makeTicks = function(tick) {
            var label = tick.label;
            if (isNil(label))
                label = tick.v.toString();
            var pos = this.xscale * (tick.v - this.minxval);
            this.xticks.push([pos, label]);
        };
        MochiKit.Iter.forEach(this.options.xTicks, bind(makeTicks, this));
    }
    else if (this.options.xNumberOfTicks) {
        // we use defined number of ticks as hint to auto generate
        var xvalues = this._uniqueXValues();
        var roughSeparation = this.xrange / this.options.xNumberOfTicks;
        var tickCount = 0;

        this.xticks = new Array();
        for (var i = 0; i <= xvalues.length; i++) {
            if (xvalues[i] >= (tickCount) * roughSeparation) {
                var pos = this.xscale * (xvalues[i] - this.minxval);
                if ((pos > 1.0) || (pos < 0.0))
                    return;
                this.xticks.push([pos, xvalues[i]]);
                tickCount++;
            }
            if (tickCount > this.options.xNumberOfTicks)
                break;
        }
    }
};

PlotKit.Layout.prototype._evaluateLineTicksForYAxis = function() {
    var isNil = MochiKit.Base.isUndefinedOrNull;


    if (this.options.yTicks) {
        this.yticks = new Array();
        var makeTicks = function(tick) {
            var label = tick.label;
            if (isNil(label))
                label = tick.v.toString();
            var pos = 1.0 - (this.yscale * (tick.v + this.minxval));
            if ((pos < 0.0) || (pos > 1.0))
                return;
            this.yticks.push([pos, label]);
        };
        MochiKit.Iter.forEach(this.options.yTicks, bind(makeTicks, this));
    }
    else if (this.options.yNumberOfTicks) {
        // We use the optionally defined number of ticks as a guide        
        this.yticks = new Array();

        // if we get this separation right, we'll have good looking graphs
        var roundInt = PlotKit.Base.roundInterval;
        var prec = this.options.yTickPrecision;
        var roughSeparation = roundInt(this.yrange, 
                                       this.options.yNumberOfTicks,
                                       this.options.yTickPrecision);

        for (var i = 0; i <= this.options.yNumberOfTicks; i++) {
            var yval = this.minyval + (i * roughSeparation);
            var pos = 1.0 - ((yval - this.minyval) * this.yscale);
            this.yticks.push([pos, MochiKit.Format.roundToFixed(yval, 1)]);
        }
    }
};

PlotKit.Layout.prototype._evaluateLineTicks = function() {
    this._evaluateLineTicksForXAxis();
    this._evaluateLineTicksForYAxis();
};

PlotKit.Layout.prototype._evaluateBarTicks = function() {
    this._evaluateLineTicks();
    var centerInBar = function(tick) {
        return [tick[0] + (this.minxdelta * this.xscale)/2, tick[1]];
    };
    this.xticks = MochiKit.Base.map(bind(centerInBar, this), this.xticks);
    
    if (this.options.barOrientation == "horizontal") {
        // swap scales
        var tempticks = this.xticks;
        this.xticks = this.yticks;
        this.yticks = tempticks;

        // we need to invert the "yaxis" (which is now the xaxis when drawn)
        var invert = function(tick) {
            return [1.0 - tick[0], tick[1]];
        }
        this.xticks = MochiKit.Base.map(invert, this.xticks);
    }
};

PlotKit.Layout.prototype._evaluatePieTicks = function() {
    var isNil = MochiKit.Base.isUndefinedOrNull;
    var formatter = MochiKit.Format.numberFormatter("#%");

    this.xticks = new Array();
    if (this.options.xTicks) {
        // make a lookup dict for x->slice values
        var lookup = new Array();
        for (var i = 0; i < this.slices.length; i++) {
            lookup[this.slices[i].xval] = this.slices[i];
        }
        
        for (var i =0; i < this.options.xTicks.length; i++) {
            var tick = this.options.xTicks[i];
            var slice = lookup[tick.v]; 
            var label = tick.label;
            if (slice) {
                if (isNil(label))
                    label = tick.v.toString();
                label += " (" + formatter(slice.fraction) + ")";
                this.xticks.push([tick.v, label]);
            }
        }
    }
    else {
        // we make our own labels from all the slices
        for (var i =0; i < this.slices.length; i++) {
            var slice = this.slices[i];
            var label = slice.xval + " (" + formatter(slice.fraction) + ")";
            this.xticks.push([slice.xval, label]);
        }
    }
};

PlotKit.Layout.prototype._regenerateHitTestCache = function() {
    this.hitTestCache.xvalues = this._uniqueXValues();
    this.hitTestCache.xlookup = new Array();
    this.hitTestCache.x2maxy = new Array();

    var listMax = MochiKit.Base.listMax;
    var itemgetter = MochiKit.Base.itemgetter;
    var map = MochiKit.Base.map;

    // generate a lookup table for x values to y values
    var setNames = keys(this.datasets);
    for (var i = 0; i < setNames.length; i++) {
        var dataset = this.datasets[setNames[i]];
        for (var j = 0; j < dataset.length; j++) {
            var xval = dataset[j][0];
            var yval = dataset[j][1];
            if (this.hitTestCache.xlookup[xval])
                this.hitTestCache.xlookup[xval].push([yval, setNames[i]]);
            else 
                this.hitTestCache.xlookup[xval] = [[yval, setNames[i]]];
        }
    }

    for (var x in this.hitTestCache.xlookup) {
        var yvals = this.hitTestCache.xlookup[x];
        this.hitTestCache.x2maxy[x] = listMax(map(itemgetter(0), yvals));
    }


};

// --------------------------------------------------------------------
// END Internal Functions
// --------------------------------------------------------------------


// Namespace Iniitialisation

PlotKit.LayoutModule = {};
PlotKit.LayoutModule.Layout = PlotKit.Layout;

PlotKit.LayoutModule.EXPORT = [
    "Layout"
];

PlotKit.LayoutModule.EXPORT_OK = [];

PlotKit.LayoutModule.__new__ = function() {
    var m = MochiKit.Base;
    
    m.nameFunctions(this);
    
    this.EXPORT_TAGS = {
        ":common": this.EXPORT,
        ":all": m.concat(this.EXPORT, this.EXPORT_OK)
    };
};

PlotKit.LayoutModule.__new__();
MochiKit.Base._exportSymbols(this, PlotKit.LayoutModule);
