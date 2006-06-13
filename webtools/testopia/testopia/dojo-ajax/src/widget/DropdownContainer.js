/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.DropdownContainer");
dojo.require("dojo.widget.*");
dojo.require("dojo.widget.HtmlWidget");
dojo.require("dojo.event.*");
dojo.require("dojo.html");

dojo.widget.defineWidget(
	"dojo.widget.DropdownContainer",
	dojo.widget.HtmlWidget,
	{
		initializer: function(){
		},

		isContainer: true,
		snarfChildDomOutput: true,
		
		inputWidth: "7em",
		inputId: "",
		inputName: "",
		iconURL: null,
		iconAlt: null,

		inputNode: null,
		buttonNode: null,
		containerNode: null,
		subWidgetNode: null,

		templateString: '<div><span style="white-space:nowrap"><input type="text" value="" style="vertical-align:middle;" dojoAttachPoint="inputNode" autocomplete="off" /> <img src="" alt="" dojoAttachPoint="buttonNode" dojoAttachEvent="onclick: onIconClick;" style="vertical-align:middle; cursor:pointer; cursor:hand;" /></span><br /><div dojoAttachPoint="containerNode" style="display:none;position:absolute;width:12em;background-color:#fff;"></div></div>',
		templateCssPath: "",

		fillInTemplate: function(args, frag){
			var source = this.getFragNodeRef(frag);
			
			if(args.inputId){ this.inputId = args.inputId; }
			if(args.inputName){ this.inputName = args.inputName; }
			if(args.iconURL){ this.iconURL = args.iconURL; }
			if(args.iconAlt){ this.iconAlt = args.iconAlt; }

			this.containerNode.style.left = "";
			this.containerNode.style.top = "";

			if(this.inputId){ this.inputNode.id = this.inputId; }
			if(this.inputName){ this.inputNode.name = this.inputName; }
			this.inputNode.style.width = this.inputWidth;

			if(this.iconURL){ this.buttonNode.src = this.iconURL; }
			if(this.iconAlt){ this.buttonNode.alt = this.iconAlt; }

			dojo.event.connect(this.inputNode, "onchange", this, "onInputChange");
			
			this.containerIframe = new dojo.html.BackgroundIframe(this.containerNode);
			this.containerIframe.size([0,0,0,0]);
		},

		onIconClick: function(evt){
			this.toggleContainerShow();
		},

		toggleContainerShow: function(){
			if(dojo.html.isShowing(this.containerNode)){
				dojo.html.hide(this.containerNode);
			}else{
				this.showContainer();
			}
		},
		
		showContainer: function(){
			dojo.html.show(this.containerNode);
			this.sizeBackgroundIframe();
		},
		
		onHide: function(evt){
			dojo.html.hide(this.containerNode);
		},
		
		sizeBackgroundIframe: function(){
			var w = dojo.style.getOuterWidth(this.containerNode);
			var h = dojo.style.getOuterHeight(this.containerNode);
			if(w==0||h==0){
				// need more time to calculate size
				dojo.lang.setTimeout(this, "sizeBackgroundIframe", 100);
				return;
			}
			if(dojo.html.isShowing(this.containerNode)){
				this.containerIframe.size([0,0,w,h]);
			}
		},

		onInputChange: function(){}
	},
	"html"
);

dojo.widget.tags.addParseTreeHandler("dojo:dropdowncontainer");