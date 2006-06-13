/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.html.DemoEngine");
dojo.require("dojo.widget.*");
dojo.require("dojo.widget.HtmlWidget");
dojo.require("dojo.lfx.*");
dojo.require("dojo.style");
dojo.require("dojo.io.*");
dojo.require("dojo.widget.Button");
dojo.require("dojo.widget.TabContainer");
dojo.require("dojo.widget.ContentPane");

dojo.widget.html.DemoEngine = function(){
	dojo.widget.HtmlWidget.call(this);
	this.widgetType = "DemoEngine";

	this.templatePath = dojo.uri.dojoUri("src/widget/templates/DemoEngine.html");

	this.demoEngineClass="demoEngine";

	this.navigationNode="";
	this.navigationClass="demoEngineNavigation";

	this.collapseToNode="";
	this.collapseToClass="collapseTo";

	this.menuNavigationNode="";
	this.menuNavigationClass="demoEngineMenuNavigation";

	this.demoNavigationNode="";
	this.demoNavigationClass="demoEngineDemoNavigation";

	this.demoListWrapperClass="demoListWrapper";
	this.demoListContainerClass="demoListContainer";
	this.demoSummaryClass = "demoSummary";
	this.demoSummaryBoxClass="demoSummaryBox";
	this.demoListScreenshotClass="screenshot";
	this.demoListSummaryContainerClass="summaryContainer";
	this.demoSummaryClass="summary";
	this.demoViewLinkClass="view";	

	this.demoContainerNode="";
	this.demoContainerClass="demoEngineDemoContainer";

	this.demoHeaderNode="";
	this.demoHeaderClass="demoEngineDemoHeader";

	this.collapsedMenuNode="";
	this.collapsedMenuClass="demoEngineCollapsedMenu";
	this.collapsedMenuButton="";

	this.aboutNode="";
	this.aboutClass="demoEngineAbout";

	this.demoPaneNode="";	
	this.demoTabContainer="";

	this.viewLinkImage="viewDemo.png";
	this.dojoDemosImage = "dojoDemos.gif";
	this.dojoDemosImageNode="";

	this.registry = function() {};	
}

dojo.inherits(dojo.widget.html.DemoEngine, dojo.widget.HtmlWidget);

dojo.lang.extend(dojo.widget.html.DemoEngine, {
	postCreate: function() {
		dojo.html.addClass(this.domNode, this.demoEngineClass);
		dojo.html.addClass(this.navigationNode, this.navigationClass);
		dojo.html.addClass(this.collapseToNode, this.collapseToClass);
		dojo.html.addClass(this.menuNavigationNode, this.menuNavigationClass);
		dojo.html.addClass(this.demoNavigationNode, this.demoListWrapperClass);
		dojo.html.addClass(this.collapsedMenuNode, this.collapsedMenuClass);
		dojo.html.addClass(this.demoHeaderNode, this.demoHeaderClass);
		dojo.html.addClass(this.demoContainerNode, this.demoContainerClass);

		// Make sure navigation node is hidden and opaque
		//dojo.style.hide(this.navigationNode);
		//dojo.style.setOpacity(this.navigationNode, 0);

		//Make sure demoNavigationNode is hidden and opaque;
		dojo.style.hide(this.demoNavigationNode);
		dojo.style.setOpacity(this.demoNavigationNode,0);

		//Make sure demoContainerNode is hidden and opaque
		dojo.style.hide(this.demoContainerNode);
		dojo.style.setOpacity(this.demoContainerNode,0);

		//Populate the menu
		this.buildMenu();

		//show navigationNode
		//dojo.lfx.html.fadeShow(this.navigationNode, 500).play();

		//turn demoPaneNode into a tabset
		this.demoTabContainer = dojo.widget.createWidget("TabContainer",{},this.demoPaneNode);	

	},

	buildMenu: function() {
		dojo.html.removeChildren(this.menuNavigationNode);

		dojo.io.bind({
			url: "demoRegistry.json",
			load: dojo.lang.hitch(this, "_buildMenu"),
			mimetype: "text/json"
		});
	},

	_buildMenu: function(type, data) {
		this.registry = data;
		dojo.debug("_buildMenu");

		dojo.lang.forEach(this.registry.navigation, dojo.lang.hitch(this,function(category) {
			this._addMenuItem(category);
		}));
	},

	_addMenuItem: function(category) {
		dojo.debug("Adding button for " + category.name);
		var newCat = dojo.widget.createWidget("Button");
		newCat.containerNode.innerHTML=category.name;
		this.menuNavigationNode.appendChild(newCat.domNode);
		dojo.event.connect(newCat,"onClick", this, "selectCategory");
	},

	selectCategory: function(e) {
		dojo.debug("Selecting: " + e.target.innerHTML);
		var showDemoNav = dojo.lfx.html.fadeShow(this.demoNavigationNode, 600);
		var moveMenuNav = dojo.lfx.html.slideTo(this.menuNavigationNode,[0,0], 250);

		dojo.html.removeChildren(this.demoNavigationNode);
	
		dojo.lfx.combine(showDemoNav, moveMenuNav).play()	

		for (var x = 0 ; x< this.registry.navigation.length; x++) {
			if (this.registry.navigation[x].name == e.target.innerHTML) {
				for (var y=0; y< this.registry.navigation[x].demos.length; y++) {
					dojo.debug("demo: " + this.registry.navigation[x].demos[y]);
					var d = this.registry.definitions[this.registry.navigation[x].demos[y]];

					//var demoListWrapper = document.createElement("div");
					//dojo.html.addClass(demoListWrapper,this.demoListWrapperClass);
					//this.demoNavigationNode.appendChild(demoListWrapper);

					var demoListContainer=document.createElement("div");
					dojo.html.addClass(demoListContainer, this.demoListContainerClass);
					this.demoNavigationNode.appendChild(demoListContainer);

					var demoSummary = document.createElement("div");
					dojo.html.addClass(demoSummary,this.demoSummaryClass);
					demoListContainer.appendChild(demoSummary);

					var demoSummaryBox = document.createElement("div");
					dojo.html.addClass(demoSummaryBox, this.demoSummaryBoxClass);
					demoSummary.appendChild(demoSummaryBox);

					var table = document.createElement("table");
					table.width="100%";
					table.cellSpacing="0";
					table.cellPadding="0";
					table.border="0";
					demoSummaryBox.appendChild(table);

					var tbody = document.createElement("tbody");
					table.appendChild(tbody);

					var tr= document.createElement("tr");
					tbody.appendChild(tr);

					var screenshotTd = document.createElement("td");
					dojo.html.addClass(screenshotTd,this.demoListScreenshotClass);
					screenshotTd.valign="top";
					screenshotTd.demoName = this.registry.navigation[x].demos[y];
					tr.appendChild(screenshotTd);

					var ss = document.createElement("img");
					ss.src=d.thumbnail;
					screenshotTd.appendChild(ss);

					var summaryTd = document.createElement("td");
					dojo.html.addClass(summaryTd,this.demoListSummaryContainerClass);
					summaryTd.vAlign="top";
					tr.appendChild(summaryTd);

					var name = document.createElement("h1");
					name.appendChild(document.createTextNode(this.registry.navigation[x].demos[y]));
					summaryTd.appendChild(name);

					var summary = document.createElement("div");
					dojo.html.addClass(summary, this.demoSummaryClass);		
					summaryTd.appendChild(summary);

					var desc = document.createElement("p");
					desc.appendChild(document.createTextNode(d.description));
					summary.appendChild(desc);
					
					var viewDiv = document.createElement("div");
					dojo.html.addClass(viewDiv, this.demoViewLinkClass);
					summary.appendChild(viewDiv);
				
					var viewLink = document.createElement("img");
					viewLink.src = this.viewLinkImage;
					viewLink.demoName = this.registry.navigation[x].demos[y];
					viewDiv.appendChild(viewLink);	
							
					dojo.event.connect(viewLink, "onclick", this, "launchDemo");
					dojo.event.connect(screenshotTd, "onclick", this, "launchDemo");
				}
			}
		}
	},

	showIframe: function(e) {
		dojo.lfx.html.fadeShow(e.currentTarget,250).play();
	},

	launchDemo: function(e) {
		dojo.debug("Launching Demo: " + e.currentTarget.parentNode.parentNode.parentNode.firstChild.innerHTML);
		var demo = e.currentTarget.demoName;

		//implode = dojo.lfx.html.implode(this.navigationNode, this.collapsedMenuNode,1500);
		//show = dojo.lfx.html.fadeShow(this.demoContainerNode,1500);
		dojo.style.setOpacity(this.demoContainerNode, 0);
		hide = dojo.lfx.html.fadeHide(this.navigationNode, 500);
		show = dojo.lfx.html.fadeShow(this.demoContainerNode,500);
		//dojo.style.setOpacity(this.demoContainerNode, 0);
		//dojo.style.show(this.demoContainerNode);
		dojo.lfx.combine(hide,show).play();

		this.demoTabContainer.destroyChildren();

		demoIframe = document.createElement("iframe");
		demoIframe.src=this.registry.definitions[demo].url;

		dojo.html.removeChildren(this.aboutNode);
		var name = document.createElement("h1");
		var about= document.createElement("h2");
		name.appendChild(document.createTextNode(demo));
		about.appendChild(document.createTextNode(this.registry.definitions[demo].description));
		this.aboutNode.appendChild(name);
		this.aboutNode.appendChild(about);

		liveDemo = dojo.widget.createWidget("ContentPane",{label: "Live Demo"});
		liveDemo.domNode.appendChild(demoIframe);

		this.demoTabContainer.addChild(liveDemo);
		demoIframe.parentNode.style.display="inline";
		demoIframe.parentNode.parentNode.style.overflow="hidden";
		dojo.io.bind({
			url: this.registry.definitions[demo].url,
			mimetype: "text/plain",
			load: dojo.lang.hitch(this, function(type,data,e) {
				source = document.createElement("textarea");
				source.appendChild(document.createTextNode(data));
				var sourcePane = dojo.widget.createWidget("ContentPane",{label: "Source"});
				source.rows="20";
				sourcePane.domNode.appendChild(source);
				this.demoTabContainer.addChild(sourcePane);
				dojo.style.show(sourcePane.domNode);

				//let the text area take care of the scrolling 
				sourcePane.domNode.style.overflow="hidden";
				
			})
		});

		this.demoTabContainer.selectTab(liveDemo);
	},

	expandDemoNavigation: function(e) {
		dojo.debug("re expanding navigation");
		//dojo.style.hide(this.demoContainerNode);
		//explode = dojo.lfx.html.explode(this.navigationNode,this.collapseToNode,1000);
		//dojo.style.show(this.navigationNode);
		//hide = dojo.lfx.html.fadeHide(this.demoContainerNode,250);

		show = dojo.lfx.html.fadeShow(this.navigationNode, 1000);
		hide = dojo.lfx.html.fadeHide(this.demoContainerNode, 1000);
		dojo.lfx.combine(show,hide).play();
	}
});

dojo.widget.tags.addParseTreeHandler("dojo:DemoEngine");
