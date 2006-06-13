/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.Tabs");
dojo.provide("dojo.widget.html.Tabs");

dojo.deprecated("dojo.widget.Tabs",  "use dojo.widget.TabPane", "0.3");

dojo.require("dojo.io.*");
dojo.require("dojo.widget.*");
dojo.require("dojo.dom");
dojo.require("dojo.html");

dojo.widget.tags.addParseTreeHandler("dojo:tabs");

dojo.widget.html.Tabs = function() {
	dojo.widget.HtmlWidget.call(this);
	this.tabs = [];
	this.panels = [];
}
dojo.inherits(dojo.widget.html.Tabs, dojo.widget.HtmlWidget);

dojo.lang.extend(dojo.widget.html.Tabs, {

	widgetType: "Tabs",
	isContainer: true,

	templatePath: null, // prolly not
	templateCssPath: dojo.uri.dojoUri("src/widget/templates/HtmlTabs.css"),

	domNode: null,
	containerNode: null,

	selected: -1,

	tabTarget: "",
	extractContent: false, // find the bits inside <body>
	parseContent: false, // parse externally loaded pages for widgets

	preventCache: false,

	buildRendering: function(args, frag) {
		dojo.style.insertCssFile(this.templateCssPath);
		this.domNode = frag["dojo:"+this.widgetType.toLowerCase()]["nodeRef"];
		if(!this.domNode) { dj_error("html.Tabs: No node reference"); }

		if(args["tabtarget"]) {
			this.tabtarget = args["tabtarget"];
			this.containerNode = document.getElementById(args["tabtarget"]);
		} else {
			this.containerNode = document.createElement("div");
			var next = this.domNode.nextSibling;
			if(next) {
				this.domNode.parentNode.insertBefore(this.containerNode, next);
			} else {
				this.domNode.parentNode.appendChild(this.containerNode);
			}
		}
		dojo.html.addClass(this.containerNode, "dojoTabPanelContainer");

		var li = dojo.dom.getFirstChildElement(this.domNode);
		while(li) {
			var a = li.getElementsByTagName("a").item(0);
			this.addTab(a);
			li = dojo.dom.getNextSiblingElement(li);
		}

		if(this.selected == -1) { this.selected = 0; }
		this.selectTab(null, this.tabs[this.selected]);
	},

	addTab: function(title, url, tabId, tabHandler) {
		// TODO: make this an object proper
		var panel = {
			url: null,
			title: null,
			isLoaded: false,
			id: null,
			isLocal: false
		};

		function isLocal(a) {
			var url = a.getAttribute("href");
			var hash = url.indexOf("#");
			if(hash == 0) {
				return true;
			}
			var loc = location.href.split("#")[0];
			var url2 = url.split("#")[0];
			if(loc == url2) {
				return true;
			}
			if(unescape(loc) == url2) {
				return true;
			}
			var outer = a.outerHTML;
			if(outer && /href=["']?#/i.test(outer)) {
				return true;
			}
			return false;
		}

		if(title && title.tagName && title.tagName.toLowerCase() == "a") {
			// init case
			var a = title;
			var li = a.parentNode;
			title = a.innerHTML;
			url = a.getAttribute("href");
			var id = null;
			var hash = url.indexOf("#");
			if(isLocal(a)) {
				id = url.split("#")[1];
				dj_debug("setting local id:", id);
				url = "#" + id;
				panel.isLocal = true;
			} else {
				id = a.getAttribute("tabid");
			}

			panel.url = url;
			panel.title = title;
			panel.id = id || dojo.html.getUniqueId();
			dj_debug("panel id:", panel.id, "url:", panel.url);
		} else {
			// programmatically adding
			var li = document.createElement("li");
			var a = document.createElement("a");
			a.innerHTML = title;
			a.href = url;
			li.appendChild(a);
			this.domNode.appendChild(li);

			panel.url = url;
			panel.title = title;
			panel.id = tabId || dojo.html.getUniqueId();
			dj_debug("prg tab:", panel.id, "url:", panel.url);
		}

		if(panel.isLocal) {
			var node = document.getElementById(id);
			node.style.display = "none";
			this.containerNode.appendChild(node);
		} else {
			var node = document.createElement("div");
			node.style.display = "none";
			node.id = panel.id;
			this.containerNode.appendChild(node);
		}

		var handler = a.getAttribute("tabhandler") || tabHandler;
		if(handler) {
			this.setPanelHandler(handler, panel);
		}

		dojo.event.connect(a, "onclick", this, "selectTab");

		this.tabs.push(li);
		this.panels.push(panel);

		if(this.selected == -1 && dojo.html.hasClass(li, "current")) {
			this.selected = this.tabs.length-1;
		}

		return { "tab": li, "panel": panel };
	},

	selectTab: function(e, target) {
		if(dojo.lang.isNumber(e)) {
			target = this.tabs[e];
		}
		else if(e) {
			if(e.target) {
				target = e.target;
				while(target && (target.tagName||"").toLowerCase() != "li") {
					target = target.parentNode;
				}
			}
			if(e.preventDefault) { e.preventDefault(); }
		}

		dojo.html.removeClass(this.tabs[this.selected], "current");

		for(var i = 0; i < this.tabs.length; i++) {
			if(this.tabs[i] == target) {
				dojo.html.addClass(this.tabs[i], "current");
				this.selected = i;
				break;
			}
		}

		var panel = this.panels[this.selected];
		if(panel) {
			this.getPanel(panel);
			this.hidePanels(panel);
			document.getElementById(panel.id).style.display = "";
		}
	},

	setPanelHandler: function(handler, panel) {
		var fcn = dojo.lang.isFunction(handler) ? handler : window[handler];
		if(!dojo.lang.isFunction(fcn)) {
			throw new Error("Unable to set panel handler, '" + handler + "' not a function.");
			return;
		}
		this["tabHandler" + panel.id] = function() {
			return fcn.apply(this, arguments);
		}
	},

	runPanelHandler: function(panel) {
		if(dojo.lang.isFunction(this["tabHandler" + panel.id])) {
			this["tabHandler" + panel.id](panel, document.getElementById(panel.id));
			return false;
		}
		return true;
	},

	getPanel: function(panel) {
		if(this.runPanelHandler(panel)) {
			if(panel.isLocal) {
				// do nothing?
			} else {
				if(!panel.isLoaded || !this.useCache) {
					this.setExternalContent(panel, panel.url, this.useCache, this.preventCache);
				}
			}
		}
	},

	setExternalContent: function(panel, url, useCache, preventCache) {
		var node = document.getElementById(panel.id);
		node.innerHTML = "Loading...";

		var extract = this.extractContent;
		var parse = this.parseContent;

		dojo.io.bind({
			url: url,
			useCache: useCache,
			preventCache: preventCache,
			mimetype: "text/html",
			handler: function(type, data, e) {
				if(type == "load") {
					if(extract) {
						var matches = data.match(/<body[^>]*>\s*([\s\S]+)\s*<\/body>/im);
						if(matches) { data = matches[1]; }
					}
					node.innerHTML = data;
					panel.isLoaded = true;
					if(parse) {
						var parser = new dojo.xml.Parse();
						var frag = parser.parseElement(node, null, true);
						dojo.widget.getParser().createComponents(frag);
					}
				} else {
					node.innerHTML = "Error loading '" + panel.url + "' (" + e.status + " " + e.statusText + ")";
				}
			}
		});
	},

	hidePanels: function(except) {
		for(var i = 0; i < this.panels.length; i++) {
			if(this.panels[i] != except && this.panels[i].id) {
				var p = document.getElementById(this.panels[i].id);
				if(p) {
					p.style.display = "none";
				}
			}
		}
	}
});
