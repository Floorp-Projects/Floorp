/*
    PlotKit Sweet SVG Renderer
    ==========================
    SVG Renderer for PlotKit which looks pretty!

    Copyright
    ---------
    Copyright 2005,2006 (c) Alastair Tse <alastair^liquidx.net>
    For use under the BSD license. <http://www.liquidx.net/plotkit>
*/


// -------------------------------------------------------------------------
// Check required components
// -------------------------------------------------------------------------

try {    
    if (typeof(PlotKit.SVGRenderer) == 'undefined')
    {
        throw "";    
    }
} 
catch (e) {    
    throw "SweetSVG depends on MochiKit.{Base,Color,DOM,Format} and PlotKit.{Layout, SVG}"
}


if (typeof(PlotKit.SweetSVGRenderer) == 'undefined') {
    PlotKit.SweetSVGRenderer = {};
}

PlotKit.SweetSVGRenderer = function(element, layout, options) {
    if (arguments.length > 0) {
        this.__init__(element, layout, options);
    }
};

PlotKit.SweetSVGRenderer.NAME = "PlotKit.SweetSVGRenderer";
PlotKit.SweetSVGRenderer.VERSION = PlotKit.VERSION;

PlotKit.SweetSVGRenderer.__repr__ = function() {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

PlotKit.SweetSVGRenderer.toString = function() {
    return this.__repr__();
};

// ---------------------------------------------------------------------
// Subclassing Magic
// ---------------------------------------------------------------------

PlotKit.SweetSVGRenderer.prototype = new PlotKit.SVGRenderer();
PlotKit.SweetSVGRenderer.prototype.constructor = PlotKit.SweetSVGRenderer;
PlotKit.SweetSVGRenderer.__super__ = PlotKit.SVGRenderer.prototype;

// ---------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------

PlotKit.SweetSVGRenderer.prototype.__init__ = function(element, layout, options) { 
    var moreOpts = PlotKit.Base.officeBlue();
    MochiKit.Base.update(moreOpts, options);
    PlotKit.SweetSVGRenderer.__super__.__init__.call(this, element, layout, moreOpts);
    //this._addDropShadowFilter();
};

PlotKit.SweetSVGRenderer.prototype._addDropShadowFilter = function() {
    var filter = this.createSVGElement("filter", {x: 0, y: 0, "id":"dropShadow"});
    var goffset = this.createSVGElement("feOffset",
        {"in": "SourceGraphic", "dx": 0, "dy": 0, "result": "topCopy"});
    var blur = this.createSVGElement("feGaussianBlur",
        {"in": "SourceAlpha", "StdDeviation": 2, "result": "shadow"});
    var soffset = this.createSVGElement("feOffset",
        {"in": "shadow", "dx": -1, "dy": -2, "result":"movedShadow"});
    var merge = this.createSVGElement("feMerge");
    var gmerge = this.createSVGElement("feMergeNode", {"in":"topCopy"});
    var smerge = this.createSVGElement("feMergeNode", {"in":"movedShadow"});
    
    merge.appendChild(gmerge);
    merge.appendChild(smerge);
    filter.appendChild(goffset);
    filter.appendChild(blur);
    filter.appendChild(soffset);
    filter.appendChild(merge);
    this.defs.appendChild(filter);
};

// ---------------------------------------------------------------------
// Extended Plotting Functions
// ---------------------------------------------------------------------

PlotKit.SweetSVGRenderer.prototype._renderBarChart = function() {
    var bind = MochiKit.Base.bind;
    var shadowColor = Color.blackColor().toRGBString();
    var shadowStyle = "fill:" + shadowColor + ";fill-opacity:0.15";
    var strokeStyle = "stroke-width: 2.0; stroke:" + Color.whiteColor().toRGBString();
    
    var drawRect = function(attrs, bar) {
        var x = this.area.w * bar.x + this.area.x;
        var y = this.area.h * bar.y + this.area.y;
        var w = this.area.w * bar.w;
        var h = this.area.h * bar.h;

        if ((w < 1) || (h < 1))
            return;        

        //attrs["filter"] = "url(#dropShadow)";
        attrs["style"] = strokeStyle;
        this._drawRect(x - 2, y - 1, w+4, h+2, {"style":shadowStyle});
        this._drawRect(x, y, w, h, attrs);
    };
    this._renderBarOrLine(this.layout.bars, bind(drawRect, this));

};

PlotKit.SweetSVGRenderer.prototype._renderLineChart = function() {
    var bind = MochiKit.Base.bind;
    var shadowColor = Color.blackColor().toRGBString();
    var shadowStyle = "fill:" + shadowColor + ";fill-opacity:0.15";
    var strokeStyle = "stroke-width: 2.0; stroke:" + Color.whiteColor().toRGBString();

    var addPoint = function(attrs, point) {
        this._tempPointsBuffer += (this.area.w * point.x + this.area.x) + "," +
                                 (this.area.h * point.y + this.area.y) + " ";
    };

    var startLine = function(attrs) {
        this._tempPointsBuffer = "";
        this._tempPointsBuffer += (this.area.x) + "," + (this.area.y+this.area.h) + " ";
    };

    var endLine = function(attrs) {
        this._tempPointsBuffer += (this.area.w + this.area.x) + ","  +(this.area.h + this.area.y);
        attrs["points"] = this._tempPointsBuffer;    
            
        attrs["stroke"] = "none";
        attrs["transform"] = "translate(-2, -1)";
        attrs["style"] = shadowStyle;
        var shadow = this.createSVGElement("polygon", attrs);
        this.root.appendChild(shadow);
        
        attrs["transform"] = "";
        attrs["style"] = strokeStyle;
        var elem = this.createSVGElement("polygon", attrs);
        this.root.appendChild(elem);
        
       
    };

    this._renderBarOrLine(this.layout.points, 
                             bind(addPoint, this), 
                             bind(startLine, this), 
                             bind(endLine, this));
};

PlotKit.SweetSVGRenderer.prototype._renderPieChart = function() {
    var centerx = this.area.x + this.area.w * 0.5;
    var centery = this.area.y + this.area.h * 0.5;
    var shadowColor = Color.blackColor().toRGBString();
    var radius = Math.min(this.area.w * this.options.pieRadius, 
                          this.area.h * this.options.pieRadius);
    var shadowStyle = "fill:" + shadowColor + ";fill-opacity:0.15";
    
    var shadow = this.createSVGElement("circle", 
        {"style": shadowStyle, "cx": centerx + 1, "cy": centery + 1, "r": radius + 1});
    this.root.appendChild(shadow);
                             
    PlotKit.SweetSVGRenderer.__super__._renderPieChart.call(this);
};
    

PlotKit.SweetSVGRenderer.prototype._renderBackground = function() {
    var attrs = {
        "fill": this.options.backgroundColor.toRGBString(),
        "stroke": "none"
    };
    

    if (this.layout.style == "bar" || this.layout.style == "line") {
        this._drawRect(this.area.x, this.area.y, 
                       this.area.w, this.area.h, attrs);
                       
        var ticks = this.layout.yticks;
        var horiz = false;
        if (this.layout.style == "bar" && 
            this.layout.options.barOrientation == "horizontal") {
                ticks = this.layout.xticks;
                horiz = true;
        }
        
        for (var i = 0; i < ticks.length; i++) {
            var x = 0;
            var y = 0;
            var w = 0;
            var h = 0;
            
            if (horiz) {
                x = ticks[i][0] * this.area.w + this.area.x;
                y = this.area.y;
                w = 1;
                h = this.area.w;
            }
            else {
                x = this.area.x;
                y = ticks[i][0] * this.area.h + this.area.y;
                w = this.area.w;
                h = 1;
            }
            
            this._drawRect(x, y, w, h,
                           {"fill": this.options.axisLineColor.toRGBString()});
        }
    }
    else {
        PlotKit.SweetSVGRenderer.__super__._renderBackground.call(this);
        
    }
    
};

// Namespace Iniitialisation

PlotKit.SweetSVG = {}
PlotKit.SweetSVG.SweetSVGRenderer = PlotKit.SweetSVGRenderer;

PlotKit.SweetSVG.EXPORT = [
    "SweetSVGRenderer"
];

PlotKit.SweetSVG.EXPORT_OK = [
    "SweetSVGRenderer"
];

PlotKit.SweetSVG.__new__ = function() {
    var m = MochiKit.Base;
    
    m.nameFunctions(this);
    
    this.EXPORT_TAGS = {
        ":common": this.EXPORT,
        ":all": m.concat(this.EXPORT, this.EXPORT_OK)
    };
};

PlotKit.SweetSVG.__new__();
MochiKit.Base._exportSymbols(this, PlotKit.SweetSVG);
