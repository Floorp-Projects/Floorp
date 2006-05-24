/*
    PlotKit SVG
    ===========
    SVG Renderer for PlotKit

    Copyright
    ---------
    Copyright 2005,2006 (c) Alastair Tse <alastair^liquidx.net>
    For use under the BSD license. <http://www.liquidx.net/plotkit>
*/

// -------------------------------------------------------------------------
// NOTES: - If you use XHTML1.1 strict, then you must include each MochiKit
//          file individuall.
//        - For IE support, you must include the AdobeSVG object hack.
//          See tests/svg.html for details.
// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
// Check required components
// -------------------------------------------------------------------------

try {    
    if (typeof(PlotKit.Layout) == 'undefined')
    {
        throw "";    
    }
} 
catch (e) {    
    throw "PlotKit depends on MochiKit.{Base,Color,DOM,Format} and PlotKit.Layout"
}


// ---------------------------------------------------------------------------
//  SVG Renderer
// ---------------------------------------------------------------------------

PlotKit.SVGRenderer = function(element, layout, options) {
    if (arguments.length > 0) 
        this.__init__(element, layout, options);
};

PlotKit.SVGRenderer.NAME = "PlotKit.SVGRenderer";
PlotKit.SVGRenderer.VERSION = PlotKit.VERSION;

PlotKit.SVGRenderer.__repr__ = function() {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

PlotKit.SVGRenderer.toString = function() {
    return this.__repr__();
}

PlotKit.SVGRenderer.isSupported = function() {
    // TODO
    return true;
};

PlotKit.SVGRenderer.prototype.__init__ = function(element, layout, options) {
    var isNil = MochiKit.Base.isUndefinedOrNull;

    // default options
    this.options = {
        "drawBackground": true,
        "backgroundColor": Color.whiteColor(),
        "padding": {left: 30, right: 30, top: 5, bottom: 10},
        "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[1]),
        "strokeColor": Color.whiteColor(),
        "strokeColorTransform": "asStrokeColor",
        "strokeWidth": 0.5,
        "shouldFill": true,
        "shouldStroke": true,
        "drawXAxis": true,
        "drawYAxis": true,
        "axisLineColor": Color.blackColor(),
        "axisLineWidth": 0.5,
        "axisTickSize": 3,
        "axisLabelColor": Color.blackColor(),
        "axisLabelFont": "Arial",
        "axisLabelFontSize": 9,
        "axisLabelWidth": 50,
        "axisLabelUseDiv": true,
        "pieRadius": 0.4,
        "enableEvents": true
    };

    MochiKit.Base.update(this.options, options ? options : {});
    this.layout = layout;
    this.style = layout.style;
    this.element = MochiKit.DOM.getElement(element);
    this.container = this.element.parentNode;
    this.height = parseInt(this.element.getAttribute("height"));
    this.width = parseInt(this.element.getAttribute("width"));
    this.document = document;
    this.root = this.element;

    // Adobe SVG Support:
    // - if an exception is thrown, then no Adobe SVG Plugin support.
    try {
        this.document = this.element.getSVGDocument();
        this.root = isNil(this.document.documentElement) ? this.element : this.document.documentElement;
    }
    catch (e) {
    }

    this.element.style.zIndex = 1;

    if (isNil(this.element))
        throw "SVGRenderer() - passed SVG object is not found";

    if (isNil(this.container) || this.container.nodeName.toLowerCase() != "div")
        throw "SVGRenderer() - No DIV's around the SVG.";

    // internal state
    this.xlabels = new Array();
    this.ylabels = new Array();

    // initialise some meta structures in SVG
    this.defs = this.createSVGElement("defs");

    this.area = {
        x: this.options.padding.left,
        y: this.options.padding.top,
        w: this.width - this.options.padding.left - this.options.padding.right,
        h: this.height - this.options.padding.top - this.options.padding.bottom
    };

    MochiKit.DOM.updateNodeAttributes(this.container, 
    {"style":{ "position": "relative", "width": this.width + "px"}});

    
};


PlotKit.SVGRenderer.prototype.render = function() {
    if (this.options.drawBackground)
        this._renderBackground();

    if (this.style == "bar") {
        this._renderBarChart();
        this._renderBarAxis();
    }
    else if (this.style == "pie") {
        this._renderPieChart();
        this._renderPieAxis();
    }
    else if (this.style == "line") {
        this._renderLineChart();
        this._renderLineAxis();
    }
};

PlotKit.SVGRenderer.prototype._renderBarOrLine = function(data, plotFunc, startFunc, endFunc) {
    
    var colorCount = this.options.colorScheme.length;
    var colorScheme = this.options.colorScheme;
    var setNames = MochiKit.Base.keys(this.layout.datasets);
    var setCount = setNames.length;

    for (var i = 0; i < setCount; i++) {
        var setName = setNames[i];
        var attrs = new Array();
        var color = colorScheme[i%colorCount];

        if (this.options.shouldFill)
            attrs["fill"] = color.toRGBString();
        else
            attrs["fill"] = "none";

        if (this.options.shouldStroke && 
            (this.options.strokeColor || this.options.strokeColorTransform)) {
            if (this.options.strokeColor)
                attrs["stroke"] = this.options.strokeColor.toRGBString();
            else if (this.options.strokeColorTransform)
                attrs["stroke"] = color[this.options.strokeColorTransform]().toRGBString();
            attrs["strokeWidth"] = this.options.strokeWidth;
        }

        if (startFunc)
            startFunc(attrs);

        var forEachFunc = function(obj) {
            if (obj.name == setName)
                plotFunc(attrs, obj);
        };                

        MochiKit.Iter.forEach(data, bind(forEachFunc, this));
        if (endFunc)
            endFunc(attrs);
    }
};

PlotKit.SVGRenderer.prototype._renderBarChart = function() {
    var bind = MochiKit.Base.bind;

    var drawRect = function(attrs, bar) {
        var x = this.area.w * bar.x + this.area.x;
        var y = this.area.h * bar.y + this.area.y;
        var w = this.area.w * bar.w;
        var h = this.area.h * bar.h;
        this._drawRect(x, y, w, h, attrs);
    };
    this._renderBarOrLine(this.layout.bars, bind(drawRect, this));
};

PlotKit.SVGRenderer.prototype._renderLineChart = function() {
    var bind = MochiKit.Base.bind;

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
        var elem = this.createSVGElement("polygon", attrs);
        this.root.appendChild(elem);
    };

    this._renderBarOrLine(this.layout.points, 
                          bind(addPoint, this), 
                          bind(startLine, this), 
                          bind(endLine, this));
};


PlotKit.SVGRenderer.prototype._renderPieChart = function() {
    var colorCount = this.options.colorScheme.length;
    var slices = this.layout.slices;

    var centerx = this.area.x + this.area.w * 0.5;
    var centery = this.area.y + this.area.h * 0.5;
    var radius = Math.min(this.area.w * this.options.pieRadius, 
                          this.area.h * this.options.pieRadius);

    // NOTE NOTE!! Canvas Tag draws the circle clockwise from the y = 0, x = 1
    // so we have to subtract 90 degrees to make it start at y = 1, x = 0

	// workaround if we only have 1 slice of 100%
	if (slices.length == 1 && (Math.abs(slices[0].startAngle) - Math.abs(slices[0].endAngle) < 0.1)) {
        var attrs = {"cx": centerx , "cy": centery , "r": radius };
        var color = this.options.colorScheme[0];
        if (this.options.shouldFill)
            attrs["fill"] = color.toRGBString();
        else
            attrs["fill"] = "none";

        if (this.options.shouldStroke && 
            (this.options.strokeColor || this.options.strokeColorTransform)) {
            if (this.options.strokeColor)
                attrs["stroke"] = this.options.strokeColor.toRGBString();
            else if (this.options.strokeColorTransform)
                attrs["stroke"] = color[this.options.strokeColorTransform]().toRGBString();
            attrs["style"] = "stroke-width: " + this.options.strokeWidth;
        }

        this.root.appendChild(this.createSVGElement("circle", attrs));
        return;
	}

    for (var i = 0; i < slices.length; i++) {
        var attrs = new Array();
        var color = this.options.colorScheme[i%colorCount];
        if (this.options.shouldFill)
            attrs["fill"] = color.toRGBString();
        else
            attrs["fill"] = "none";

        if (this.options.shouldStroke &&
            (this.options.strokeColor || this.options.strokeColorTransform)) {
            if (this.options.strokeColor)
                attrs["stroke"] = this.options.strokeColor.toRGBString();
            else if (this.options.strokeColorTransform)
                attrs["stroke"] = color[this.options.strokeColorTransform]().toRGBString();
            attrs["style"] = "stroke-width:" + this.options.strokeWidth;
        }

        var largearc = 0;
        if (Math.abs(slices[i].endAngle - slices[i].startAngle) > Math.PI)
            largearc = 1;
        var x1 = Math.cos(slices[i].startAngle - Math.PI/2) * radius;
        var y1 = Math.sin(slices[i].startAngle - Math.PI/2) * radius;
        var x2 = Math.cos(slices[i].endAngle - Math.PI/2) * radius;
        var y2 = Math.sin(slices[i].endAngle - Math.PI/2) * radius;
        var rx = x2 - x1;
        var ry = y2 - y1;

        var pathString = "M" + centerx + "," + centery + " ";       
        pathString += "l" + x1 + "," + y1 + " ";
        pathString += "a" + radius + "," + radius + " 0 " + largearc + ",1 " + rx + "," + ry + " z";

        attrs["d"] = pathString;

        var elem = this.createSVGElement("path", attrs);
        this.root.appendChild(elem);
    }
};

PlotKit.SVGRenderer.prototype._renderBarAxis = function() {
    this._renderAxis();
}

PlotKit.SVGRenderer.prototype._renderLineAxis = function() {
    this._renderAxis();
};


PlotKit.SVGRenderer.prototype._renderAxis = function() {

    if (!this.options.drawXAxis && !this.options.drawYAxis)
        return;

    var labelStyle = {"style":
         {"position": "absolute",
          "textAlign": "center",
          "fontSize": this.options.axisLabelFontSize + "px",
          "zIndex": 10,
          "color": this.options.axisLabelColor.toRGBString(),
          "width": this.options.axisLabelWidth + "px",
          "overflow": "hidden"
         }
    };

    // axis lines
    var lineAttrs = {
        "stroke": this.options.axisLineColor.toRGBString(),
        "strokeWidth": this.options.axisLineWidth
    };
    

    if (this.options.drawYAxis) {
        if (this.layout.yticks) {
            var drawTick = function(tick) {
                var x = this.area.x;
                var y = this.area.y + tick[0] * this.area.h;
                this._drawLine(x, y, x - 3, y, lineAttrs);
                
                if (this.options.axisLabelUseDiv) {
                    var label = DIV(labelStyle, tick[1]);
                    label.style.top = (y - this.options.axisLabelFontSize) + "px";
                    label.style.left = (x - this.options.padding.left + this.options.axisTickSize) + "px";
                    label.style.textAlign = "left";
                    label.style.width = (this.options.padding.left - 3) + "px";
                    MochiKit.DOM.appendChildNodes(this.container, label);
                    this.ylabels.push(label);
                }
                else {
                    var attrs = {
                        y: y + 3,
                        x: (x - this.options.padding.left + 3),
                        width: (this.options.padding.left - this.options.axisTickSize) + "px",
                        height: (this.options.axisLabelFontSize + 3) + "px",
                        fontFamily: "Arial",
                        fontSize: this.options.axisLabelFontSize + "px",
                        fill: this.options.axisLabelColor.toRGBString()
                    };
                    
                    /* we can do clipping just like DIVs
                    http://www.xml.com/pub/a/2004/06/02/svgtype.html */
                    /*
                    var mask = this.createSVGElement("mask", {id: "mask" + tick[0]});
                    var maskShape = this.createSVGElement("rect",
                        {y: y + 3,
                         x: (x - this.options.padding.left + 3),
                         width: (this.options.padding.left - this.options.axisTickSize) + "px",
                         height: (this.options.axisLabelFontSize + 3) + "px",
                         style: {"fill": "#ffffff", "stroke": "#000000"}});
                    mask.appendChild(maskShape);
                    this.defs.appendChild(mask);
                    
                    attrs["filter"] = "url(#mask" + tick[0] + ")";
                    */
                    
                    var label = this.createSVGElement("text", attrs);
                    label.appendChild(this.document.createTextNode(tick[1]));
                    this.root.appendChild(label);
                }
            };
            
            MochiKit.Iter.forEach(this.layout.yticks, bind(drawTick, this));
        }

        this._drawLine(this.area.x, this.area.y, this.area.x, this.area.y + this.area.h, lineAttrs);
    }

    if (this.options.drawXAxis) {
        if (this.layout.xticks) {
            var drawTick = function(tick) {
                var x = this.area.x + tick[0] * this.area.w;
                var y = this.area.y + this.area.h;
                this._drawLine(x, y, x, y + this.options.axisTickSize, lineAttrs);

                if (this.options.axisLabelUseDiv) {
                    var label = DIV(labelStyle, tick[1]);
                    label.style.top = (y + this.options.axisTickSize) + "px";
                    label.style.left = (x - this.options.axisLabelWidth/2) + "px";
                    label.style.textAlign = "center";
                    label.style.width = this.options.axisLabelWidth + "px";
                    MochiKit.DOM.appendChildNodes(this.container, label);
                    this.xlabels.push(label);
                }
                else {
                    var attrs = {
                        y: (y + this.options.axisTickSize + this.options.axisLabelFontSize),
                        x: x - 3,
                        width: this.options.axisLabelWidth + "px",
                        height: (this.options.axisLabelFontSize + 3) + "px",
                        fontFamily: "Arial",
                        fontSize: this.options.axisLabelFontSize + "px",
                        fill: this.options.axisLabelColor.toRGBString(),
                        textAnchor: "middle"
                    };
                    var label = this.createSVGElement("text", attrs);
                    label.appendChild(this.document.createTextNode(tick[1]));
                    this.root.appendChild(label);
                }
            };
            
            MochiKit.Iter.forEach(this.layout.xticks, bind(drawTick, this));
        }

        this._drawLine(this.area.x, this.area.y + this.area.h, this.area.x + this.area.w, this.area.y + this.area.h, lineAttrs)
    }
};

PlotKit.SVGRenderer.prototype._renderPieAxis = function() {

    if (this.layout.xticks) {
        // make a lookup dict for x->slice values
        var lookup = new Array();
        for (var i = 0; i < this.layout.slices.length; i++) {
            lookup[this.layout.slices[i].xval] = this.layout.slices[i];
        }
        
        var centerx = this.area.x + this.area.w * 0.5;
        var centery = this.area.y + this.area.h * 0.5;
        var radius = Math.min(this.area.w * this.options.pieRadius + 10, 
                              this.area.h * this.options.pieRadius + 10);
        var labelWidth = this.options.axisLabelWidth;
        
        for (var i = 0; i < this.layout.xticks.length; i++) {
            var slice = lookup[this.layout.xticks[i][0]];
            if (MochiKit.Base.isUndefinedOrNull(slice))
                continue;
                
                
            var angle = (slice.startAngle + slice.endAngle)/2;
            // normalize the angle
            var normalisedAngle = angle;
            if (normalisedAngle > Math.PI * 2)
                normalisedAngle = normalisedAngle - Math.PI * 2;
            else if (normalisedAngle < 0)
                normalisedAngle = normalisedAngle + Math.PI * 2;
                
            var labelx = centerx + Math.sin(normalisedAngle) * (radius + 10);
            var labely = centery - Math.cos(normalisedAngle) * (radius + 10);

            var attrib = {
                "position": "absolute",
                 "zIndex": 11,
                "width": labelWidth + "px",
                "fontSize": this.options.axisLabelFontSize + "px",
                "overflow": "hidden",
                "color": this.options.axisLabelColor.toHexString()
            };

            var svgattrib = {
                "width": labelWidth + "px",
                "fontSize": this.options.axisLabelFontSize + "px",
                "height": (this.options.axisLabelFontSize + 3) + "px",
                "fill": this.options.axisLabelColor.toRGBString()
            };

            if (normalisedAngle <= Math.PI * 0.5) {
                // text on top and align left
                MochiKit.Base.update(attrib, {
                    'textAlign': 'left', 'verticalAlign': 'top',
                    'left': labelx + 'px',
                    'top':  (labely - this.options.axisLabelFontSize) + "px"
                });
                MochiKit.Base.update(svgattrib, {
                    "x": labelx,
                    "y" :(labely - this.options.axisLabelFontSize),
                    "textAnchor": "left"
                        });
            }
            else if ((normalisedAngle > Math.PI * 0.5) && (normalisedAngle <= Math.PI)) {
                // text on bottom and align left
                MochiKit.Base.update(attrib, {
                    'textAlign': 'left', 'verticalAlign': 'bottom',
                    'left': labelx + 'px',
                    'top':  labely + "px"
                });
                MochiKit.Base.update(svgattrib, {
                    'textAnchor': 'left',
                    'x': labelx,
                    'y':  labely
                });
            }
            else if ((normalisedAngle > Math.PI) && (normalisedAngle <= Math.PI*1.5)) {
                // text on bottom and align right
                MochiKit.Base.update(attrib, {
                    'textAlign': 'right', 'verticalAlign': 'bottom',
                    'left': labelx + 'px',
                    'top':  labely + "px"
                });
                MochiKit.Base.update(svgattrib, {
                    'textAnchor': 'right',
                    'x': labelx - labelWidth,
                    'y':  labely
                });
            }
            else {
                // text on top and align right
                MochiKit.Base.update(attrib, {
                    'textAlign': 'left', 'verticalAlign': 'bottom',
                    'left': labelx + 'px',
                    'top':  labely + "px"
                });
                MochiKit.Base.update(svgattrib, {
                    'textAnchor': 'left',
                    'x': labelx - labelWidth,
                    'y':  labely - this.options.axisLabelFontSize
                });
            }

            if (this.options.axisLabelUseDiv) {
                var label = DIV({'style': attrib}, this.layout.xticks[i][1]);
                this.xlabels.push(label);
                MochiKit.DOM.appendChildNodes(this.container, label);
            }
            else {
                var label = this.createSVGElement("text", svgattrib);
                label.appendChild(this.document.createTextNode(this.layout.xticks[i][1]))
                this.root.appendChild(label);
            }
      }
        
    }
};

PlotKit.SVGRenderer.prototype._renderBackground = function() {
    var opts = {"stroke": "none",
                  "fill": this.options.backgroundColor.toRGBString()
    };
    this._drawRect(0, 0, this.width, this.height, opts);
};

PlotKit.SVGRenderer.prototype._drawRect = function(x, y, w, h, moreattrs) {
    var attrs = {x: x + "px", y: y + "px", width: w + "px", height: h + "px"};
    if (moreattrs)
        MochiKit.Base.update(attrs, moreattrs);

    var elem = this.createSVGElement("rect", attrs);
    this.root.appendChild(elem);
};

PlotKit.SVGRenderer.prototype._drawLine = function(x1, y1, x2, y2, moreattrs) {
    var attrs = {x1: x1 + "px", y1: y1 + "px", x2: x2 + "px", y2: y2 + "px"};
    if (moreattrs)
        MochiKit.Base.update(attrs, moreattrs);

    var elem = this.createSVGElement("line", attrs);
    this.root.appendChild(elem);
}

PlotKit.SVGRenderer.prototype.clear = function() {
    while(this.element.firstChild) {
        this.element.removeChild(this.element.firstChild);
    }
    
    if (this.options.axisLabelUseDiv) {
        for (var i = 0; i < this.xlabels.length; i++) {
            MochiKit.DOM.removeElement(this.xlabels[i]);
        }        
        for (var i = 0; i < this.ylabels.length; i++) {
            MochiKit.DOM.removeElement(this.ylabels[i]);
        }            
    }
    this.xlabels = new Array();
    this.ylabels = new Array();
};

PlotKit.SVGRenderer.prototype.createSVGElement = function(name, attrs) {
    var isNil = MochiKit.Base.isUndefinedOrNull;
    var elem;
    var doc = isNil(this.document) ? document : this.document;

    try {
        elem = doc.createElementNS("http://www.w3.org/2000/svg", name);
    }
    catch (e) {
        elem = doc.createElement(name);
        elem.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    }

    if (attrs)
        MochiKit.DOM.updateNodeAttributes(elem, attrs);

    // TODO: we don't completely emulate the MochiKit.DOM.createElement
    //       as we don't care about nodes contained. We really should though.

    return elem;

};

PlotKit.SVGRenderer.SVGNS = 'http://www.w3.org/2000/svg';

PlotKit.SVGRenderer.SVG = function(attrs) {
    // we have to do things differently for IE+AdobeSVG.
    // My guess this works (via trial and error) is that we need to
    // have an SVG object in order to use SVGDocument.createElementNS
    // but IE doesn't allow us to that.

    var ie = navigator.appVersion.match(/MSIE (\d\.\d)/);
    var opera = (navigator.userAgent.toLowerCase().indexOf("opera") != -1);
    if (ie && (ie[1] >= 6) && (!opera)) {
        var width = attrs["width"] ? attrs["width"] : "100";
        var height = attrs["height"] ? attrs["height"] : "100";
        var eid = attrs["id"] ? attrs["id"] : "notunique";
        
        var html = '<svg:svg width="' + width + '" height="' + height + '" ';
        html += 'id="' + eid + '" version="1.1" baseProfile="full">';

        var canvas = document.createElement(html);

        // create embedded SVG inside SVG.
        var group = canvas.getSVGDocument().createElementNS(PlotKit.SVGRenderer.SVGNS, "svg");
        group.setAttribute("width", width);
        group.setAttribute("height", height);
        canvas.getSVGDocument().appendChild(group);

        return canvas;
    }
    else {
        return PlotKit.SVGRenderer.prototype.createSVGElement("svg", attrs);
    }
};

PlotKit.SVGRenderer.isSupported = function() {
    var isOpera = (navigator.userAgent.toLowerCase().indexOf("opera") != -1);
    var ieVersion = navigator.appVersion.match(/MSIE (\d\.\d)/);
    var safariVersion = navigator.userAgent.match(/AppleWebKit\/(\d+)/);
    var operaVersion = navigator.userAgent.match(/Opera\/(\d*\.\d*)/);
    var mozillaVersion = navigator.userAgent.match(/rv:(\d*\.\d*).*Gecko/);
    

    if (ieVersion && (ieVersion[1] >= 6) && !isOpera) {
        var dummysvg = document.createElement('<svg:svg width="1" height="1" baseProfile="full" version="1.1" id="dummy">');
        try {
            dummysvg.getSVGDocument();
            dummysvg = null;
            return true;
        }
        catch (e) {
            return false;
        }
    }
    
    /* support not really there yet. no text and paths are buggy
    if (safariVersion && (safariVersion[1] > 419))
        return true;
    */

    if (operaVersion && (operaVersion[1] > 8.9))
        return true
    
    if (mozillaVersion && (mozillaVersion > 1.7))
        return true;
    
    return false;
};
