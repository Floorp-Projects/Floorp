/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.Accordion");

//
// TODO
// make it prettier
// active dragging upwards doesn't always shift other bars (direction calculation is wrong in this case)
//

// pull in widget infrastructure
dojo.require("dojo.widget.*");
// pull in our superclass
dojo.require("dojo.widget.SplitPane");
// pull in animation libraries
dojo.require("dojo.animation.Animation");

dojo.widget.Accordion = function(){

	dojo.widget.html.SplitPane.call(this);
	this.widgetType = "Accordion";
	this._super = dojo.widget.AccordionPanel.superclass;
	dojo.event.connect(this, "postCreate", this, "myPostCreate");
	
}
dojo.inherits(dojo.widget.Accordion, dojo.widget.html.SplitPane);
dojo.lang.extend(dojo.widget.Accordion, {
	sizerWidth: 1,
	activeSizing: 1,
	animationInterval: 250,
	openPanel: null,
	myPostCreate: function(args, frag){
		for(var i=0; i<this.sizers.length; i++){
			var sn = this.sizers[i];
			if(sn){
				sn.style.border = "0px";
			}
		}
		for(var i=0; i<this.children.length; i++){
			this.children[i].setMinHeight();
			if(this.children[i].open){
				this.openPanel = this.children[i];
			}
		}
		this.onResized();
	},

	setOpenPanel: function(panel){
		if(!panel){ return; }
		if(!this.openPanel){
			this.openPanel = panel; 
			panel.open = true;
		}else if(panel === this.openPanel){
			// no-op
		}else{
			var closingPanel = this.openPanel;
			var openingPanel = panel;
			this.openPanel.sizeShare = 0;
			this.openPanel.open = false;
			this.openPanel.setMinHeight(true);
			this.openPanel = panel;
			this.openPanel.sizeShare = 100;
			this.openPanel.open = true;
			this.onResized();
			// Don't animate if there is no interval
			if (this.animationInterval == 0){
				openingPanel.sizeShare = 100;
				closingPanel.sizeShare = 0;
				e.animation.accordion.onResized();
			}else{
				var line = new dojo.math.curves.Line([0,0], [0,100]);
				var anim = new dojo.animation.Animation(
					line,
					this.animationInterval,
					new dojo.math.curves.Bezier([[0],[0.05],[0.1],[0.9],[0.95],[1]])
				);
				
				var accordion = this;
	
				anim.handler = function(e) {
					switch(e.type) {
						case "animate":
							openingPanel.sizeShare = parseInt(e.y);
							closingPanel.sizeShare = parseInt(100 - e.y);
							accordion.onResized();
							break;
						case "end":
						break;
					}
				}
				anim.play();
			}
		}
	}
});
dojo.widget.tags.addParseTreeHandler("dojo:Accordion");

dojo.widget.AccordionPanel = function(){
	dojo.widget.html.SplitPanePanel.call(this);
	this.widgetType = "AccordionPanel";
	dojo.event.connect(this, "fillInTemplate", this, "myFillInTemplate");
	dojo.event.connect(this, "postCreate", this, "myPostCreate");
}

dojo.inherits(dojo.widget.AccordionPanel, dojo.widget.html.SplitPanePanel);

dojo.lang.extend(dojo.widget.AccordionPanel, {
	sizeMin:0,
	initialSizeMin: null,
	sizeShare: 0,
	open: false,
	label: "",
	initialContent: "",
	labelNode: null,
	scrollContent: true,
	initalContentNode: null,
	contentNode: null,
	templatePath: dojo.uri.dojoUri("src/widget/templates/AccordionPanel.html"),
	templateCssPath: dojo.uri.dojoUri("src/widget/templates/AccordionPanel.css"),

	setMinHeight: function(ignoreIC){
		// now handle our setup
		var lh = dojo.style.getContentHeight(this.labelNode);
		if(!ignoreIC){
			lh += dojo.style.getContentHeight(this.initialContentNode);
			this.initialSizeMin = lh;
		}
		this.sizeMin = lh;
	},

	myFillInTemplate: function(args, frag){
		var sn;
		if(this.label.length > 0){
			this.labelNode.innerHTML = this.label;
		}else{
			try{
				sn = frag["dojo:label"][0]["dojo:label"].nodeRef;
				while(sn.firstChild){
					this.labelNode.firstChild.appendChild(sn.firstChild);
				}
			}catch(e){ }
		}
		if(this.initialContent.length > 0){
			this.initialContentNode.innerHTML = this.initialContent;
		}else{
			try{
				sn = frag["dojo:initialcontent"][0]["dojo:initialcontent"].nodeRef;
				while(sn.firstChild){
					this.initialContentNode.firstChild.appendChild(sn.firstChild);
				}
			}catch(e){ }
		}
		sn = frag["dojo:"+this.widgetType.toLowerCase()]["nodeRef"];
		while(sn.firstChild){
			this.contentNode.appendChild(sn.firstChild);
		}
		if(this.open){
			this.sizeShare = 100;
		}
	},

	myPostCreate: function(){
		// this.domNode.style.overflow = "auto";
		// this.domNode.style.position = "relative";
	},

	sizeSet: function(size){
		if(!this.scrollContent){
			return;
		}
		if(size <= this.sizeMin){
			this.contentNode.style.display = "none";
		}else{
			// this.domNode.style.overflow = "hidden";
			this.contentNode.style.display = "block";
			this.contentNode.style.overflow = "auto";
			var scrollSize = (size-((this.initialSizeMin) ? this.initialSizeMin : this.sizeMin));
			if(dojo.render.html.ie){
				this.contentNode.style.height =  scrollSize+"px";
			}else{
				dojo.style.setOuterHeight(this.contentNode, scrollSize);
			}
		}
	},

	toggleOpen: function(evt){
		this.parent.setOpenPanel(this);
	}
});
dojo.widget.tags.addParseTreeHandler("dojo:AccordionPanel");


