/*
    PlotKit Sweet Canvas Renderer
    =============================
    Canvas Renderer for PlotKit which looks pretty!

    Copyright
    ---------
    Copyright 2005,2006 (c) Alastair Tse <alastair^liquidx.net>
    For use under the BSD license. <http://www.liquidx.net/plotkit>
*/

// -------------------------------------------------------------------------
// Check required components
// -------------------------------------------------------------------------

try {    
    if (typeof(PlotKit.CanvasRenderer) == 'undefined')
    {
        throw "";    
    }
} 
catch (e) {    
    throw "SweetCanvas depends on MochiKit.{Base,Color,DOM,Format} and PlotKit.{Layout, Canvas}"
}


if (typeof(PlotKit.SweetCanvasRenderer) == 'undefined') {
    PlotKit.SweetCanvasRenderer = {};
}

PlotKit.SweetCanvasRenderer = function(element, layout, options) {
    if (arguments.length > 0) {
        this.__init__(element, layout, options);
    }
};

PlotKit.SweetCanvasRenderer.NAME = "PlotKit.SweetCanvasRenderer";
PlotKit.SweetCanvasRenderer.VERSION = PlotKit.VERSION;

PlotKit.SweetCanvasRenderer.__repr__ = function() {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

PlotKit.SweetCanvasRenderer.toString = function() {
    return this.__repr__();
};

// ---------------------------------------------------------------------
// Subclassing Magic
// ---------------------------------------------------------------------

PlotKit.SweetCanvasRenderer.prototype = new PlotKit.CanvasRenderer();
PlotKit.SweetCanvasRenderer.prototype.constructor = PlotKit.SweetCanvasRenderer;
PlotKit.SweetCanvasRenderer.__super__ = PlotKit.CanvasRenderer.prototype;

// ---------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------

PlotKit.SweetCanvasRenderer.prototype.__init__ = function(el, layout, opts) { 
    var moreOpts = PlotKit.Base.officeBlue();
    MochiKit.Base.update(moreOpts, opts);
    PlotKit.SweetCanvasRenderer.__super__.__init__.call(this, el, layout, moreOpts);
};

// ---------------------------------------------------------------------
// Extended Plotting Functions
// ---------------------------------------------------------------------

PlotKit.SweetCanvasRenderer.prototype._renderBarChart = function() {
    var bind = MochiKit.Base.bind;
    var shadowColor = Color.blackColor().colorWithAlpha(0.1).toRGBString();

    var prepareFakeShadow = function(context, x, y, w, h) {
        context.fillStyle = shadowColor;
        context.fillRect(x-2, y-2, w+4, h+2); 
        context.fillStyle = shadowColor;
        context.fillRect(x-1, y-1, w+2, h+1); 
    };

    var colorCount = this.options.colorScheme.length;
    var colorScheme =  this.options.colorScheme;
    var setNames = MochiKit.Base.keys(this.layout.datasets);
    var setCount = setNames.length;

    var chooseColor = function(name) {
        for (var i = 0; i < setCount; i++) {
            if (name == setNames[i])
                return colorScheme[i%colorCount];
        }
        return colorScheme[0];
    };

    var drawRect = function(context, bar) {
        var x = this.area.w * bar.x + this.area.x;
        var y = this.area.h * bar.y + this.area.y;
        var w = this.area.w * bar.w;
        var h = this.area.h * bar.h;

        if ((w < 1) || (h < 1))
            return;        

        context.save();

        context.shadowBlur = 5.0;
        context.shadowColor = Color.fromHexString("#888888").toRGBString();

        if (this.isIE) {
            context.save();
            context.fillStyle = "#cccccc";
            context.fillRect(x-2, y-2, w+4, h+2); 
            context.restore();
        }
        else {
            prepareFakeShadow(context, x, y, w, h);
        }

        context.fillStyle = chooseColor(bar.name).toRGBString();
        context.fillRect(x, y, w, h);

        context.shadowBlur = 0;

        // We want to be able to set these...
            //context.strokeStyle = Color.whiteColor().toRGBString();
            //context.lineWidth = 2.0;
            context.strokeStyle = this.options.strokeColor.toRGBString();
            context.lineWidth = this.options.strokeWidth;

        context.strokeRect(x, y, w, h);                

        context.restore();

    };

    var drawLabel = function(context, bar) {
            var x = this.area.w * bar.x + this.area.x;
            var y = this.area.h * bar.y + this.area.y;
            var w = this.area.w * bar.w;
            var h = this.area.h * bar.h;

        //if ((w < 1) || (h < 1))
            //return;        

        context.save();

        var label = DIV({"class":"bar_chart_label"}, bar.label);

        // todo 7 is arbitrary, assuming around a 12pt font
        // this should be a config var (or preferably automatic)
        var top = y+(h/2)-7;
        label.style.top = top + 'px';

        var left = x + 20;
        label.style.left = left + 'px';

        MochiKit.DOM.appendChildNodes(this.element.parentNode, label);
        context.restore();

    };
    this._renderBarChartWrap(this.layout.bars, bind(drawRect, this), bind(drawLabel, this));
};

PlotKit.CanvasRenderer.prototype._renderLineChart = function() {
    var context = this.element.getContext("2d");
    var colorCount = this.options.colorScheme.length;
    var colorScheme = this.options.colorScheme;
    var setNames = MochiKit.Base.keys(this.layout.datasets);
    var setCount = setNames.length;
    var bind = MochiKit.Base.bind;


    for (var i = 0; i < setCount; i++) {
        var setName = setNames[i];
        var color = colorScheme[i%colorCount];
        var strokeX = this.options.strokeColorTransform;

        // setup graphics context
        context.save();
        
        // create paths
        var makePath = function() {
            context.beginPath();
            context.moveTo(this.area.x, this.area.y + this.area.h);
            var addPoint = function(context, point) {
            if (point.name == setName)
                context.lineTo(this.area.w * point.x + this.area.x,
                               this.area.h * point.y + this.area.y);
            };
            MochiKit.Iter.forEach(this.layout.points, partial(addPoint, context), this);
            context.lineTo(this.area.w + this.area.x,
                           this.area.h + this.area.y);
            context.lineTo(this.area.x, this.area.y + this.area.h);
            context.closePath();
        };

        // faux shadow for firefox
        context.save();
        if (this.isIE) {
            context.fillStyle = "#cccccc";
        }
        else {
            context.fillStyle = Color.blackColor().colorWithAlpha(0.2).toRGBString();
        }

        context.translate(-1, -2);
        bind(makePath, this)();        
        context.fill();
        context.restore();

        context.shadowBlur = 5.0;
        context.shadowColor = Color.fromHexString("#888888").toRGBString();
        context.fillStyle = color.toRGBString();
        context.lineWidth = 2.0;
        context.strokeStyle = Color.whiteColor().toRGBString();

        bind(makePath, this)();
        context.fill();
        bind(makePath, this)();
        context.stroke();

        context.restore();
    }
};

PlotKit.CanvasRenderer.prototype._renderPieChart = function() {
    var context = this.element.getContext("2d");

    var colorCount = this.options.colorScheme.length;
    var slices = this.layout.slices;

    var centerx = this.area.x + this.area.w * 0.5;
    var centery = this.area.y + this.area.h * 0.5;
    var radius = Math.min(this.area.w * this.options.pieRadius, 
                          this.area.h * this.options.pieRadius);

    if (this.isIE) {
        centerx = parseInt(centerx);
        centery = parseInt(centery);
        radius = parseInt(radius);
    }

	// NOTE NOTE!! Canvas Tag draws the circle clockwise from the y = 0, x = 1
	// so we have to subtract 90 degrees to make it start at y = 1, x = 0

    if (!this.isIE) {
        context.save();
        var shadowColor = Color.blackColor().colorWithAlpha(0.2);
        context.fillStyle = shadowColor.toRGBString();
        context.shadowBlur = 5.0;
        context.shadowColor = Color.fromHexString("#888888").toRGBString();
        context.translate(1, 1);
        context.beginPath();
        context.moveTo(centerx, centery);
        context.arc(centerx, centery, radius + 2, 0, Math.PI*2, false);
        context.closePath();
        context.fill();
        context.restore();
    }

    context.save();
    context.strokeStyle = Color.whiteColor().toRGBString();
    context.lineWidth = 2.0;    
    for (var i = 0; i < slices.length; i++) {
        var color = this.options.colorScheme[i%colorCount];
        context.fillStyle = color.toRGBString();

        var makePath = function() {
            context.beginPath();
            context.moveTo(centerx, centery);
            context.arc(centerx, centery, radius, 
                        slices[i].startAngle - Math.PI/2,
                        slices[i].endAngle - Math.PI/2,
                        false);
            context.lineTo(centerx, centery);
            context.closePath();
        };

        if (Math.abs(slices[i].startAngle - slices[i].endAngle) > 0.0001) {
            makePath();
            context.fill();
            makePath();
            context.stroke();
        }
    }
    context.restore();
};

PlotKit.SweetCanvasRenderer.prototype._renderBackground = function() {
    var context = this.element.getContext("2d");
   
    if (this.layout.style == "bar" || this.layout.style == "line") {
        context.save();
        context.fillStyle = this.options.backgroundColor.toRGBString();
        context.fillRect(this.area.x, this.area.y, this.area.w, this.area.h);
        // We want to be able to set these...
            //context.strokeStyle = Color.whiteColor().toRGBString();
            //context.lineWidth = 1.0;
            context.strokeStyle = this.options.strokeColor.toRGBString();
            context.lineWidth = this.options.strokeWidth;
        for (var i = 0; i < this.layout.yticks.length; i++) {
            var y = this.layout.yticks[i][0] * this.area.h + this.area.y;
            var x = this.area.x;
            context.beginPath();
            context.moveTo(x, y);
            context.lineTo(x + this.area.w, y);
            context.closePath();
            context.stroke();
        }
        context.restore();
    }
    else {
        PlotKit.SweetCanvasRenderer.__super__._renderBackground.call(this);
    }
};
