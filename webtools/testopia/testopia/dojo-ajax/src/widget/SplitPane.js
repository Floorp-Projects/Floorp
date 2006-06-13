/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.SplitPane");
dojo.provide("dojo.widget.SplitPanePanel");
dojo.provide("dojo.widget.html.SplitPane");
dojo.provide("dojo.widget.html.SplitPanePanel");

//
// TODO
// make it prettier
// active dragging upwards doesn't always shift other bars (direction calculation is wrong in this case)
//

dojo.require("dojo.widget.*");
dojo.require("dojo.widget.LayoutPane");
dojo.require("dojo.widget.Container");
dojo.require("dojo.html");
dojo.require("dojo.style");
dojo.require("dojo.dom");

dojo.widget.html.SplitPane = function(){

	dojo.widget.html.Container.call(this);

	this.sizers = [];
}

dojo.inherits(dojo.widget.html.SplitPane, dojo.widget.html.Container);

dojo.lang.extend(dojo.widget.html.SplitPane, {
	widgetType: "SplitPane",
	virtualSizer: null,
	isHorizontal: 0,
	paneBefore: null,
	paneAfter: null,
	isSizing: false,
	dragOffset: null,
	startPoint: null,
	lastPoint: null,
	sizingSplitter: null,
	isActiveResize: 0,
	offsetX: 0,
	offsetY: 0,
	isDraggingLeft: 0,
	templateCssPath: dojo.uri.dojoUri("src/widget/templates/HtmlSplitPane.css"),
	originPos: null,

	activeSizing: '',
	sizerWidth: 15,
	orientation: 'horizontal',

	debugName: '',

	fillInTemplate: function(){

		dojo.style.insertCssFile(this.templateCssPath, null, true);
		dojo.html.addClass(this.domNode, "dojoHtmlSplitPane");
		this.domNode.style.overflow='hidden';	// workaround firefox bug

		this.paneWidth = dojo.style.getContentWidth(this.domNode);
		this.paneHeight = dojo.style.getContentHeight(this.domNode);

		this.isHorizontal = (this.orientation == 'horizontal') ? 1 : 0;
		this.isActiveResize = (this.activeSizing == '1') ? 1 : 0;

		//dojo.debug("fillInTemplate for "+this.debugName);
	},

	onResized: function(e){
		this.paneWidth = dojo.style.getContentWidth(this.domNode);
		this.paneHeight = dojo.style.getContentHeight(this.domNode);
		this.layoutPanels();
		this.notifyChildrenOfResize();	// notify children they've been moved/resized
	},

	postCreate: function(args, fragment, parentComp){

		// dojo.debug("post create for "+this.debugName);

		// attach the children

		for(var i=0; i<this.children.length; i++){
			with(this.children[i].domNode.style){
				position = "absolute";
			}
			dojo.html.addClass(this.children[i].domNode, 
				"dojoHtmlSplitterPanePanel");
		}

		// create the draggers

		for(var i=0; i<this.children.length-1; i++){

			// i still don't understand this closure black magic :) [CH]
			var self = this;
			var handler = (function(){ var sizer_i = i; return function(e){ self.beginSizing(e, sizer_i); } })();

			this.sizers[i] = document.createElement('div');
			this.sizers[i].style.position = 'absolute';
			this.sizers[i].onmousedown = handler;
			this.sizers[i].className = this.isHorizontal ? 'dojoHtmlSplitPaneSizerH' : 'dojoHtmlSplitPaneSizerV';
			this.domNode.appendChild(this.sizers[i]);

			dojo.html.disableSelection(this.sizers[i]);

		}

		// create the fake dragger

		this.virtualSizer = document.createElement('div');
		this.virtualSizer.style.position = 'absolute';
		this.virtualSizer.style.display = 'none';
		//this.virtualSizer.style.backgroundColor = 'lime';
		this.virtualSizer.style.zIndex = 10;
		this.virtualSizer.className = this.isHorizontal ? 'dojoHtmlSplitPaneVirtualSizerH' : 'dojoHtmlSplitPaneVirtualSizerV';
		this.domNode.appendChild(this.virtualSizer);

		dojo.html.disableSelection(this.virtualSizer);

		//
		// size the panels once the browser has caught up
		//
		this.resizeSoon();
	},

	layoutPanels: function(){

		//
		// calculate space
		//

		var space = this.isHorizontal ? this.paneWidth : this.paneHeight;

		if (this.children.length > 1){

			space -= this.sizerWidth * (this.children.length - 1);
		}


		//
		// calculate total of SizeShare values
		//

		var out_of = 0;

		for(var i=0; i<this.children.length; i++){

			out_of += this.children[i].sizeShare;
		}


		//
		// work out actual pixels per sizeshare unit
		//

		var pix_per_unit = space / out_of;


		//
		// set the SizeActual member of each pane
		//

		var total_size = 0;

		for(var i=0; i<this.children.length-1; i++){

			var size = Math.round(pix_per_unit * this.children[i].sizeShare);
			this.children[i].sizeActual = size;
			total_size += size;
		}
		this.children[this.children.length-1].sizeActual = space - total_size;

		//
		// make sure the sizes are ok
		//

		this.checkSizes();


		//
		// now loop, positioning each pane
		//

		var pos = 0;
		var size = this.children[0].sizeActual;
		this.movePanel(this.children[0].domNode, pos, size);
		this.children[0].position = pos;
		pos += size;

		for(var i=1; i<this.children.length; i++){

			// first we position the sizing handle before this pane
			this.movePanel(this.sizers[i-1], pos, this.sizerWidth);
			this.sizers[i-1].position = pos;
			pos += this.sizerWidth;

			size = this.children[i].sizeActual;
			this.movePanel(this.children[i].domNode, pos, size);
			this.children[i].position = pos;
			pos += size;
		}
		
		//
		// if children are widgets, then let them resize themselves (if they want to)
		//
		for(var i=0; i<this.children.length; i++){
			this.children[i].onResized();
		}
	},

	movePanel: function(panel, pos, size){
		if (this.isHorizontal){
			panel.style.left = pos + 'px';
			panel.style.top = 0;

			dojo.style.setOuterWidth(panel, size);
			dojo.style.setOuterHeight(panel, this.paneHeight);
		}else{
			panel.style.left = 0;
			panel.style.top = pos + 'px';

			dojo.style.setOuterWidth(panel, this.paneWidth);
			dojo.style.setOuterHeight(panel, size);
		}
	},

	growPane: function(growth, pane){

		if (growth > 0){
			if (pane.sizeActual > pane.sizeMin){
				if ((pane.sizeActual - pane.sizeMin) > growth){

					// stick all the growth in this pane
					pane.sizeActual = pane.sizeActual - growth;
					growth = 0;
				}else{
					// put as much growth in here as we can
					growth -= pane.sizeActual - pane.sizeMin;
					pane.sizeActual = pane.sizeMin;
				}
			}
		}
		return growth;
	},

	checkSizes: function(){

		var total_min_size = 0;
		var total_size = 0;

		for(var i=0; i<this.children.length; i++){

			total_size += this.children[i].sizeActual;
			total_min_size += this.children[i].sizeMin;
		}

		// only make adjustments if we have enough space for all the minimums

		if (total_min_size <= total_size){

			var growth = 0;

			for(var i=0; i<this.children.length; i++){

				if (this.children[i].sizeActual < this.children[i].sizeMin){

					growth += this.children[i].sizeMin - this.children[i].sizeActual;
					this.children[i].sizeActual = this.children[i].sizeMin;
				}
			}

			if (growth > 0){
				if (this.isDraggingLeft){
					for(var i=this.children.length-1; i>=0; i--){
						growth = this.growPane(growth, this.children[i]);
					}
				}else{
					for(var i=0; i<this.children.length; i++){
						growth = this.growPane(growth, this.children[i]);
					}
				}
			}
		}else{

			for(var i=0; i<this.children.length; i++){
				this.children[i].sizeActual = Math.round(total_size * (this.children[i].sizeMin / total_min_size));
			}
		}
	},

	beginSizing: function(e, i){
		var clientX = window.event ? window.event.offsetX : e.layerX;
		var clientY = window.event ? window.event.offsetY : e.layerY;
		var screenX = window.event ? window.event.clientX : e.pageX;
		var screenY = window.event ? window.event.clientY : e.pageY;

		this.paneBefore = this.children[i];
		this.paneAfter = this.children[i+1];

		this.isSizing = true;
		this.sizingSplitter = this.sizers[i];
		this.originPos = dojo.style.getAbsolutePosition(this.domNode, true);
		this.dragOffset = {'x':clientX, 'y':clientY};
		this.startPoint  = {'x':screenX, 'y':screenY};
		this.lastPoint  = {'x':screenX, 'y':screenY};

		this.offsetX = screenX - clientX;
		this.offsetY = screenY - clientY;

		if (!this.isActiveResize){
			this.showSizingLine();
		}
		
		//
		// attach mouse events
		//

		dojo.event.connect(document.documentElement, "onmousemove", this, "changeSizing");
		dojo.event.connect(document.documentElement, "onmouseup", this, "endSizing");
	},

	changeSizing: function(e){

		// FIXME: is this fixed in connect()?
		var screenX = window.event ? window.event.clientX : e.pageX;
		var screenY = window.event ? window.event.clientY : e.pageY;

		if (this.isActiveResize){
			this.lastPoint = {'x':screenX, 'y':screenY};
			this.movePoint();
			this.updateSize();
		}else{
			this.lastPoint = {'x':screenX, 'y':screenY};
			this.movePoint();
			this.moveSizingLine();
		}
	},

	endSizing: function(e){

		if (!this.isActiveResize){
			this.hideSizingLine();
		}

		this.updateSize();

		this.isSizing = false;

		dojo.event.disconnect(document.documentElement, "onmousemove", this, "changeSizing");
		dojo.event.disconnect(document.documentElement, "onmouseup", this, "endSizing");
	},

	movePoint: function(){

		// make sure FLastPoint is a legal point to drag to
		p = this.screenToMainClient(this.lastPoint);

		if (this.isHorizontal){

			var a = p.x - this.dragOffset.x;
			a = this.legaliseSplitPoint(a);
			p.x = a + this.dragOffset.x;
		}else{
			var a = p.y - this.dragOffset.y;
			a = this.legaliseSplitPoint(a);
			p.y = a + this.dragOffset.y;
		}

		this.lastPoint = this.mainClientToScreen(p);
	},

	screenToClient: function(pt){

		pt.x -= (this.offsetX + this.sizingSplitter.position);
		pt.y -= (this.offsetY + this.sizingSplitter.position);

		return pt;
	},

	clientToScreen: function(pt){

		pt.x += (this.offsetX + this.sizingSplitter.position);
		pt.y += (this.offsetY + this.sizingSplitter.position);

		return pt;
	},

	screenToMainClient: function(pt){

		pt.x -= this.offsetX;
		pt.y -= this.offsetY;

		return pt;
	},

	mainClientToScreen: function(pt){

		pt.x += this.offsetX;
		pt.y += this.offsetY;

		return pt;
	},

	legaliseSplitPoint: function(a){

		a += this.sizingSplitter.position;

		this.isDraggingLeft = (a > 0) ? 1 : 0;

		if (!this.isActiveResize){

			if (a < this.paneBefore.position + this.paneBefore.sizeMin){

				a = this.paneBefore.position + this.paneBefore.sizeMin;
			}

			if (a > this.paneAfter.position + (this.paneAfter.sizeActual - (this.sizerWidth + this.paneAfter.sizeMin))){

				a = this.paneAfter.position + (this.paneAfter.sizeActual - (this.sizerWidth + this.paneAfter.sizeMin));
			}
		}

		a -= this.sizingSplitter.position;

		this.checkSizes();

		return a;
	},

	updateSize: function(){

		var p = this.clientToScreen(this.lastPoint);
		var p = this.screenToClient(this.lastPoint);

		var pos = this.isHorizontal ? p.x - (this.dragOffset.x + this.originPos.x) : p.y - (this.dragOffset.y + this.originPos.y);

		var start_region = this.paneBefore.position;
		var end_region   = this.paneAfter.position + this.paneAfter.sizeActual;

		this.paneBefore.sizeActual = pos - start_region;
		this.paneAfter.position    = pos + this.sizerWidth;
		this.paneAfter.sizeActual  = end_region - this.paneAfter.position;

		for(var i=0; i<this.children.length; i++){

			this.children[i].sizeShare = this.children[i].sizeActual;
		}

		this.layoutPanels();
	},

	showSizingLine: function(){

		this.moveSizingLine();

		if (this.isHorizontal){
			dojo.style.setOuterWidth(this.virtualSizer, this.sizerWidth);
			dojo.style.setOuterHeight(this.virtualSizer, this.paneHeight);
		}else{
			dojo.style.setOuterWidth(this.virtualSizer, this.paneWidth);
			dojo.style.setOuterHeight(this.virtualSizer, this.sizerWidth);
		}

		this.virtualSizer.style.display = 'block';
	},

	hideSizingLine: function(){

		this.virtualSizer.style.display = 'none';
	},

	moveSizingLine: function(){

		var origin = {'x':0, 'y':0};

		if (this.isHorizontal){
			origin.x += (this.lastPoint.x - this.startPoint.x) + this.sizingSplitter.position;
		}else{
			origin.y += (this.lastPoint.y - this.startPoint.y) + this.sizingSplitter.position;
		}

		this.virtualSizer.style.left = origin.x + 'px';
		this.virtualSizer.style.top = origin.y + 'px';
	}
});

// These arguments can be specified for the children of a SplitPane.
// Since any widget can be specified as a SplitPane child, mix them
// into the base widget class.  (This is a hack, but it's effective.)
dojo.lang.extend(dojo.widget.Widget, {
	sizeMin: 10,
	sizeShare: 10
});

// Deprecated class for split pane children.
// Actually any widget can be the child of a split pane
dojo.widget.html.SplitPanePanel = function(){
	dojo.widget.html.LayoutPane.call(this);
}
dojo.inherits(dojo.widget.html.SplitPanePanel, dojo.widget.html.LayoutPane);
dojo.lang.extend(dojo.widget.html.SplitPanePanel, {
	widgetType: "SplitPanePanel"
});

dojo.widget.tags.addParseTreeHandler("dojo:SplitPane");
dojo.widget.tags.addParseTreeHandler("dojo:SplitPanePanel");
