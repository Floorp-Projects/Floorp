/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.lfx.html");
dojo.require("dojo.lfx.Animation");

dojo.require("dojo.html");
dojo.require("dojo.event");
dojo.require("dojo.lang.func");

dojo.lfx.html._byId = function(nodes){
	if(dojo.lang.isArrayLike(nodes)){
		if(!nodes.alreadyChecked){
			var n = [];
			dojo.lang.forEach(nodes, function(node){
				n.push(dojo.byId(node));
			});
			n.alreadyChecked = true;
			return n;
		}else{
			return nodes;
		}
	}else{
		return [dojo.byId(nodes)];
	}
}

dojo.lfx.html.propertyAnimation = function(	/*DOMNode*/ nodes, 
											/*Array*/ propertyMap, 
											/*int*/ duration,
											/*function*/ easing){
	nodes = dojo.lfx.html._byId(nodes);

	if(nodes.length==1){
		// FIXME: we're only supporting start-value filling when one node is
		// passed

		dojo.lang.forEach(propertyMap, function(prop){
			if(typeof prop["start"] == "undefined"){
				prop.start = parseInt(dojo.style.getComputedStyle(nodes[0], prop.property));
				if(isNaN(prop.start) && (prop.property == "opacity")){
					prop.start = 1;
				}
			}
		});
	}

	var coordsAsInts = function(coords){
		var cints = new Array(coords.length);
		for(var i = 0; i < coords.length; i++){
			cints[i] = Math.round(coords[i]);
		}
		return cints;
	}
	var setStyle = function(n, style){
		n = dojo.byId(n);
		if(!n || !n.style){ return; }
		for(s in style){
			if(s == "opacity"){
				dojo.style.setOpacity(n, style[s]);
			}else{
				n.style[dojo.style.toCamelCase(s)] = style[s];
			}
		}
	}
	var propLine = function(properties){
		this._properties = properties;
		this.diffs = new Array(properties.length);
		dojo.lang.forEach(properties, function(prop, i){
			// calculate the end - start to optimize a bit
			if(dojo.lang.isArray(prop.start)){
				// don't loop through the arrays
				this.diffs[i] = null;
			}else{
				this.diffs[i] = prop.end - prop.start;
			}
		}, this);
		this.getValue = function(n){
			var ret = {};
			dojo.lang.forEach(this._properties, function(prop, i){
				var value = null;
				if(dojo.lang.isArray(prop.start)){
					value = (prop.units||"rgb") + "(";
					for(var j = 0 ; j < prop.start.length ; j++){
						value += Math.round(((prop.end[j] - prop.start[j]) * n) + prop.start[j]) + (j < prop.start.length - 1 ? "," : "");
					}
					value += ")";
				}else{
					value = ((this.diffs[i]) * n) + prop.start + (prop.property != "opacity" ? prop.units||"px" : "");
				}
				ret[prop.property] = value;
			}, this);
			return ret;
		}
	}
	
	var anim = new dojo.lfx.Animation(duration, new propLine(propertyMap), easing);
	
	dojo.event.connect(anim, "onAnimate", function(propValues){
		dojo.lang.forEach(nodes, function(node){
			setStyle(node, propValues); 
		});
	});
	
	return anim;
}

dojo.lfx.html._makeFadeable = function(nodes){
	var makeFade = function(node){
		if(dojo.render.html.ie){
			// only set the zoom if the "tickle" value would be the same as the
			// default
			if( (node.style.zoom.length == 0) &&
				(dojo.style.getStyle(node, "zoom") == "normal") ){
				// make sure the node "hasLayout"
				// NOTE: this has been tested with larger and smaller user-set text
				// sizes and works fine
				node.style.zoom = "1";
				// node.style.zoom = "normal";
			}
			// don't set the width to auto if it didn't already cascade that way.
			// We don't want to f anyones designs
			if(	(node.style.width.length == 0) &&
				(dojo.style.getStyle(node, "width") == "auto") ){
				node.style.width = "auto";
			}
		}
	}
	if(dojo.lang.isArrayLike(nodes)){
		dojo.lang.forEach(nodes, makeFade);
	}else{
		makeFade(nodes);
	}
}

dojo.lfx.html.fadeIn = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	dojo.lfx.html._makeFadeable(nodes);
	var anim = dojo.lfx.propertyAnimation(nodes, [
		{	property: "opacity",
			start: dojo.style.getOpacity(nodes[0]),
			end: 1 } ], duration, easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(nodes, anim);
		});
	}

	return anim;
}

dojo.lfx.html.fadeOut = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	dojo.lfx.html._makeFadeable(nodes);
	var anim = dojo.lfx.propertyAnimation(nodes, [
		{	property: "opacity",
			start: dojo.style.getOpacity(nodes[0]),
			end: 0 } ], duration, easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(nodes, anim);
		});
	}

	return anim;
}

dojo.lfx.html.fadeShow = function(nodes, duration, easing, callback){
	var anim = dojo.lfx.html.fadeIn(nodes, duration, easing, callback);
	dojo.event.connect(anim, "beforeBegin", function(){
		if(dojo.lang.isArrayLike(nodes)){
			dojo.lang.forEach(nodes, dojo.style.show);
		}else{
			dojo.style.show(nodes);
		}
	});
	
	return anim;
}

dojo.lfx.html.fadeHide = function(nodes, duration, easing, callback){
	var anim = dojo.lfx.html.fadeOut(nodes, duration, easing, function(){
		if(dojo.lang.isArrayLike(nodes)){
			dojo.lang.forEach(nodes, dojo.style.hide);
		}else{
			dojo.style.hide(nodes);
		}
		if(callback){ callback(nodes, anim); }
	});
	
	return anim;
}

dojo.lfx.html.wipeIn = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	var init = function(node, overflow){
		if(overflow == "visible") {
			node.style.overflow = "hidden";
		}
		dojo.style.show(node);
		node.style.height = 0;
	}

	dojo.lang.forEach(nodes, function(node){
		var overflow = dojo.style.getStyle(node, "overflow");
		var initialize = function(){
			init(node, overflow);
		}
		initialize();
		
		var anim = dojo.lfx.propertyAnimation(node,
			[{	property: "height",
				start: 0,
				end: node.scrollHeight }], duration, easing);
		
		dojo.event.connect(anim, "beforeBegin", initialize);
		dojo.event.connect(anim, "onEnd", function(){
			node.style.overflow = overflow;
			node.style.height = "auto";
			if(callback){ callback(node, anim); }
		});
		anims.push(anim);
	});
	
	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lfx.html.wipeOut = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];
	
	var init = function(node, overflow){
		dojo.style.show(node);
		if(overflow == "visible") {
			node.style.overflow = "hidden";
		}
	}
	dojo.lang.forEach(nodes, function(node){
		var overflow = dojo.style.getStyle(node, "overflow");
		var initialize = function(){
			init(node, overflow);
		}
		initialize();

		var anim = dojo.lfx.propertyAnimation(node,
			[{	property: "height",
				start: node.offsetHeight,
				end: 0 } ], duration, easing);
		
		dojo.event.connect(anim, "beforeBegin", initialize);
		dojo.event.connect(anim, "onEnd", function(){
			dojo.style.hide(node);
			node.style.overflow = overflow;
			if(callback){ callback(node, anim); }
		});
		anims.push(anim);
	});

	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else { return anims[0]; }
}

dojo.lfx.html.slideTo = function(nodes, coords, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	dojo.lang.forEach(nodes, function(node){
		var top = null;
		var left = null;
		var pos = null;
		
		var init = (function(){
			var innerNode = node;
			return function(){
				top = node.offsetTop;
				left = node.offsetLeft;
				pos = dojo.style.getComputedStyle(node, 'position');

				if (pos == 'relative' || pos == 'static') {
					top = parseInt(dojo.style.getComputedStyle(node, 'top')) || 0;
					left = parseInt(dojo.style.getComputedStyle(node, 'left')) || 0;
				}
			}
		})();
		init();
		
		var anim = dojo.lfx.propertyAnimation(node,
			[{	property: "top",
				start: top,
				end: coords[0] },
			{	property: "left",
				start: left,
				end: coords[1] }], duration, easing);
		
		dojo.event.connect(anim, "beforeBegin", init);
		if(callback){
			dojo.event.connect(anim, "onEnd", function(){
				callback(node, anim);
			});
		}

		anims.push(anim);
	});
	
	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lfx.html.explode = function(start, endNode, duration, easing, callback){
	var startCoords = dojo.style.toCoordinateArray(start);
	var outline = document.createElement("div");
	with(outline.style){
		position = "absolute";
		border = "1px solid black";
		display = "none";
	}
	document.body.appendChild(outline);

	endNode = dojo.byId(endNode);
	with(endNode.style){
		visibility = "hidden";
		display = "block";
	}
	var endCoords = dojo.style.toCoordinateArray(endNode);
	with(endNode.style){
		display = "none";
		visibility = "visible";
	}

	var anim = new dojo.lfx.Animation({
		beforeBegin: function(){
			dojo.style.show(outline);
		},
		onAnimate: function(value){
			with(outline.style){
				left = value[0] + "px";
				top = value[1] + "px";
				width = value[2] + "px";
				height = value[3] + "px";
			}
		},
		onEnd: function(){
			dojo.style.show(endNode);
			outline.parentNode.removeChild(outline);
		}
	}, duration, new dojo.lfx.Line(startCoords, endCoords), easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(endNode, anim);
		});
	}
	return anim;
}

dojo.lfx.html.implode = function(startNode, end, duration, easing, callback){
	var startCoords = dojo.style.toCoordinateArray(startNode);
	var endCoords = dojo.style.toCoordinateArray(end);

	startNode = dojo.byId(startNode);
	var outline = document.createElement("div");
	with(outline.style){
		position = "absolute";
		border = "1px solid black";
		display = "none";
	}
	document.body.appendChild(outline);

	var anim = new dojo.lfx.Animation({
		beforeBegin: function(){
			dojo.style.hide(startNode);
			dojo.style.show(outline);
		},
		onAnimate: function(value){
			with(outline.style){
				left = value[0] + "px";
				top = value[1] + "px";
				width = value[2] + "px";
				height = value[3] + "px";
			}
		},
		onEnd: function(){
			outline.parentNode.removeChild(outline);
		}
	}, duration, new dojo.lfx.Line(startCoords, endCoords), easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(startNode, anim);
		});
	}
	return anim;
}

dojo.lfx.html.highlight = function(nodes, startColor, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	dojo.lang.forEach(nodes, function(node){
		var color = dojo.style.getBackgroundColor(node);
		var bg = dojo.style.getStyle(node, "background-color").toLowerCase();
		var wasTransparent = (bg == "transparent" || bg == "rgba(0, 0, 0, 0)");
		while(color.length > 3) { color.pop(); }

		var rgb = new dojo.graphics.color.Color(startColor).toRgb();
		var endRgb = new dojo.graphics.color.Color(color).toRgb();

		var anim = dojo.lfx.propertyAnimation(node, [{
			property: "background-color",
			start: rgb,
			end: endRgb
		}], duration, easing);

		dojo.event.connect(anim, "beforeBegin", function(){
			node.style.backgroundColor = "rgb(" + rgb.join(",") + ")";
		});

		dojo.event.connect(anim, "onEnd", function(){
			if(wasTransparent){
				node.style.backgroundColor = "transparent";
			}
			if(callback){
				callback(node, anim);
			}
		});

		anims.push(anim);
	});

	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lfx.html.unhighlight = function(nodes, endColor, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	dojo.lang.forEach(nodes, function(node){
		var color = new dojo.graphics.color.Color(dojo.style.getBackgroundColor(node)).toRgb();
		var rgb = new dojo.graphics.color.Color(endColor).toRgb();
		
		var anim = dojo.lfx.propertyAnimation(node, [{
			property: "background-color",
			start: color,
			end: rgb
		}], duration, easing);

		dojo.event.connect(anim, "beforeBegin", function(){
			node.style.backgroundColor = "rgb(" + color.join(",") + ")";
		});
		if(callback){
			dojo.event.connect(anim, "onEnd", function(){
				callback(node, anim);
			});
		}

		anims.push(anim);
	});

	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lang.mixin(dojo.lfx, dojo.lfx.html);
