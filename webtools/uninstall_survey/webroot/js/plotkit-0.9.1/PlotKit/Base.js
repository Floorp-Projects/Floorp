/*
    PlotKit
    =======
    PlotKit is a collection of Javascript classes that allows
    you to quickly visualise data using different types of charts.

    For license/info/documentation: http://www.liquidx.net/plotkit/

    Copyright
    ---------
    Copyright 2005,2006 (c) Alastair Tse <alastair^liquidx.net>
    For use under the BSD license. <http://www.liquidx.net/plotkit>
*/

// --------------------------------------------------------------------
// Check required components
// --------------------------------------------------------------------

try {    
    if (typeof(MochiKit.Base) == 'undefined'   ||
        typeof(MochiKit.DOM) == 'undefined'    ||
        typeof(MochiKit.Color) == 'undefined'  ||
        typeof(MochiKit.Format) == 'undefined')
    {
        throw "";    
    }
} 
catch (e) {    
    throw "PlotKit depends on MochiKit.{Base,Color,DOM,Format}"
}

// -------------------------------------------------------------------
// Inject Common Shortcuts we use into MochiKit.Color.Color
// -------------------------------------------------------------------

MochiKit.Base.update(MochiKit.Color.Color.prototype, {
    asFillColor: function() {
        return this.lighterColorWithLevel(0.3);
    },
        
    asStrokeColor: function() {
        return this.darkerColorWithLevel(0.1);
    },

    asPointColor: function() {
        return this.lighterColorWithLevel(0.1);
    }
});


// -------------------------------------------------------------------
// Define our own PlotKit namespace
// -------------------------------------------------------------------

if (typeof(PlotKit) == 'undefined') {
    PlotKit = {};
}

PlotKit.NAME = "PlotKit";
PlotKit.VERSION = "0.8";
PlotKit.__repr__ = function() {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

PlotKit.toString = function() {
    return this.__repr__();
}

// -------------------------------------------------------------------
//  Encapsulate all our utility function into it's own namespace.
// -------------------------------------------------------------------

if (typeof(PlotKit.Base) == 'undefined') {
    PlotKit.Base = {};
}

PlotKit.Base.NAME = 'PlotKit.Base';
PlotKit.Base.VERSION = PlotKit.VERSION;

PlotKit.Base.__repr__ = function() {
    return "[" + this.NAME + " " + this.VERSION + "]";
};

PlotKit.Base.toString = function() {
    return this.__repr__();
}


// Detect whether we are using prototype.js
PlotKit.Base.usingPrototype =  function() {
    try {
        return (typeof(Object.extend) == 'function');
    }
    catch (e) {
        return false;
    }
}


MochiKit.Base.update(PlotKit.Base, {
    roundInterval: function(range, intervals, precision) {
        // We want to make the interval look regular,
        var trunc = MochiKit.Format.roundToFixed;
        var sep = range/intervals;
        return parseFloat(trunc(sep, precision));
    },

    collapse: function(lst) {
        var m = MochiKit.Base;
        var biggerList = new Array();
        for (var i = 0; i < lst.length; i++) {
            biggerList = m.concat(biggerList, lst[i]);
        }
        if (PlotKit.Base.usingPrototype()) {
            delete biggerList.extend;
            delete biggerList.from;
            delete biggerList.inspect;
        }
        
        return biggerList;
    },
    
    uniq: function(sortedList) {
        // get unique elements in list, exactly the same as unix shell's uniq.
        var m = MochiKit.Base;
        
        if (!m.isArrayLike(sortedList) || (sortedList.length < 1))
            return new Array();

        var uniq = new Array();
        var lastElem = sortedList[0];    
        uniq.push(sortedList[0]);
        for (var i = 1; i < sortedList.length; i++) {
            if (m.compare(sortedList[i], lastElem) != 0) {
                lastElem = sortedList[i];
                uniq.push(sortedList[i]);            
            }
        }
        return uniq;
    },
    
    colorScheme: function() {
        var mb = MochiKit.Base;
        var mc = MochiKit.Color
        var scheme = ["red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"];
        
        var makeColor = function(name) {
            return mc.Color[name + "Color"]()
        };
        
        return mb.map(makeColor, scheme);
    },

    baseDarkPrimaryColors: function () {
        var hexColor = MochiKit.Color.Color.fromHexString;
        return [hexColor("#ad3f40"),
                hexColor("#ddac2c"),
                hexColor("#dfdd0c"),
                hexColor("#5276c4"),
                hexColor("#739c5a")];
    },

    basePrimaryColors: function () {
        var hexColor = MochiKit.Color.Color.fromHexString;
        return [hexColor("#d24c4d"),
                hexColor("#f2b32f"),
                hexColor("#ece90e"),
                hexColor("#5d83da"),
                hexColor("#78a15d")];
    },
    
    baseBlueColors: function () {
         var hexColor = MochiKit.Color.Color.fromHexString;
         return [hexColor("#4b6b94"), hexColor("#5d81b4"), hexColor("#acbad2")];
    },

    palette: function(baseColor, fromLevel, toLevel, increment) {
        var isNil = MochiKit.Base.isUndefinedOrNull;
        var fractions = new Array();
        if (isNil(increment))
            increment = 0.1;
        if (isNil(toLevel))
            toLevel = 0.4;
        if (isNil(fromLevel))
            fromLevel = -0.2;

        var level = fromLevel;
        while (level <= toLevel) {
            fractions.push(level);
            level += increment;
        }
            
        var makeColor = function(color, fraction) {
            return color.lighterColorWithLevel(fraction);
        };
        return MochiKit.Base.map(partial(makeColor, baseColor), fractions);
    },
    
    excanvasSupported: function() {
         if (/MSIE/.test(navigator.userAgent) && !window.opera) {
             return true;
         }
         return false;
    },

    // The following functions are from quirksmode.org
    // http://www.quirksmode.org/js/findpos.html

    findPosX: function(obj) {
        var curleft = 0;
        if (obj.offsetParent) {
            while (obj.offsetParent) {
                    curleft += obj.offsetLeft
                        obj = obj.offsetParent;
            }
        }
        else if (obj.x)
            curleft += obj.x;
        return curleft;
    },
                       
   findPosY: function(obj) {
       var curtop = 0;
       if (obj.offsetParent) {
           while (obj.offsetParent) {
               curtop += obj.offsetTop
               obj = obj.offsetParent;
           }
       }
       else if (obj.y)
           curtop += obj.y;
       return curtop;
   },
   
   isFuncLike: function(obj) {
       return (typeof(obj) == 'function');
   }
});    

//
// Prototype.js aware (crippled) versions of map and items.
//

PlotKit.Base.map = function(fn, lst) {
    if (PlotKit.Base.usingPrototype()) {
        var rval = [];
        for (var x in lst) {
            if (typeof(lst[x]) == 'function') continue;
            rval.push(fn(lst[x]));
        }
        return rval;
    }
    else {
        return MochiKit.Base.map(fn, lst);
    }
};

PlotKit.Base.items = function(lst) {
    if (PlotKit.Base.usingPrototype()) {
        var rval = [];
         for (var x in lst) {
             if (typeof(lst[x]) == 'function') continue;
             rval.push([x, lst[x]]);
         }
         return rval;
    }
    else {
        return MochiKit.Base.items(lst);
    }
};

PlotKit.Base.keys = function(lst) {
    if (PlotKit.Base.usingPrototype()) {
        var rval = [];
         for (var x in lst) {
             if (typeof(lst[x]) == 'function') continue;
             rval.push(x);
         }
         return rval;
    }
    else {
        return MochiKit.Base.keys(lst);
    }
};

// 
// colour schemes
//

PlotKit.Base.baseColors = function () {
   var hexColor = MochiKit.Color.Color.fromHexString;
   return [hexColor("#476fb2"),
           hexColor("#be2c2b"),
           hexColor("#85b730"),
           hexColor("#734a99"),
           hexColor("#26a1c5"),
           hexColor("#fb8707"),
           hexColor("#000000")];
};

PlotKit.Base.officeBaseStyle = {
    "axisLineWidth": 2.0,
    "axisLabelColor": Color.grayColor(),
    "axisLineColor": Color.whiteColor(),
    "padding": {top: 5, bottom: 10, left: 30, right: 30}
};    

MochiKit.Base.update(PlotKit.Base,{
    officeBlue: function() {
        var r = {
        "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[0]),
        "backgroundColor": PlotKit.Base.baseColors()[0].lighterColorWithLevel(0.45)
        };
        MochiKit.Base.update(r, PlotKit.Base.officeBaseStyle);
        return r;
    },
    officeRed: function() {
        var r = {
        "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[1]),
        "backgroundColor": PlotKit.Base.baseColors()[1].lighterColorWithLevel(0.5)
        };
        MochiKit.Base.update(r, PlotKit.Base.officeBaseStyle);
        return r;
    },
    officeGreen: function() {
        var r = {
        "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[2]),
        "backgroundColor": PlotKit.Base.baseColors()[2].lighterColorWithLevel(0.5)
        };
        MochiKit.Base.update(r, PlotKit.Base.officeBaseStyle);
        return r;
    },
    officePurple: function() {
        var r = {
        "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[3]),
        "backgroundColor": PlotKit.Base.baseColors()[3].lighterColorWithLevel(0.5)
        };
        MochiKit.Base.update(r, PlotKit.Base.officeBaseStyle);
        return r;
    },
    
    officeCyan: function() {
        var r = {
            "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[4]),
            "backgroundColor": PlotKit.Base.baseColors()[4].lighterColorWithLevel(0.5)
            };
        MochiKit.Base.update(r, PlotKit.Base.officeBaseStyle);
        return r;
    },
    
    officeOrange: function() {
        var r = {
            "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[5]),
            "backgroundColor": PlotKit.Base.baseColors()[5].lighterColorWithLevel(0.4)
            };
        MochiKit.Base.update(r, PlotKit.Base.officeBaseStyle);
        return r;
    },
    
    officeBlack: function() {
        var r = {
        "colorScheme": PlotKit.Base.palette(PlotKit.Base.baseColors()[6], 0.0, 0.6),
        "backgroundColor": PlotKit.Base.baseColors()[6].lighterColorWithLevel(0.9)
        };
        MochiKit.Base.update(r, PlotKit.Base.officeBaseStyle);
        return r;
    }
});


PlotKit.Base.EXPORT = [
   "baseColors",
   "collapse",
   "colorScheme",
   "findPosX",
   "findPosY",
   "officeBaseStyle",
   "officeBlue",
   "officeRed",
   "officeGreen",
   "officePurple",
   "officeCyan",
   "officeOrange",
   "officeBlack",
   "roundInterval",
   "uniq",
   "isFuncLike",
   "excanvasSupported"
];

PlotKit.Base.EXPORT_OK = [];

PlotKit.Base.__new__ = function() {
    var m = MochiKit.Base;
    
    m.nameFunctions(this);
    
    this.EXPORT_TAGS = {
        ":common": this.EXPORT,
        ":all": m.concat(this.EXPORT, this.EXPORT_OK)
    };
};

PlotKit.Base.__new__();
MochiKit.Base._exportSymbols(this, PlotKit.Base);

