/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.webui.DomWidget");
dojo.require("dojo.widget.DomWidget");

dj_deprecated("dojo.webui.DomWidget is deprecated, use dojo.widget.DomWidget");

dojo.webui.DomWidget = dojo.widget.DomWidget;
dojo.webui._cssFiles = dojo.widget._cssFiles;

dojo.webui.buildFromTemplate = dojo.widget.buildFromTemplate;

dojo.webui.attachProperty = dojo.widget.attachProperty;
dojo.webui.eventAttachProperty = dojo.widget.eventAttachProperty;
dojo.webui.subTemplateProperty = dojo.widget.subTemplateProperty;
dojo.webui.onBuildProperty = dojo.widget.onBuildProperty;

dojo.webui.attachTemplateNodes = dojo.widget.attachTemplateNodes;
dojo.webui.getDojoEventsFromStr = dojo.widget.getDojoEventsFromStr;
dojo.webui.buildAndAttachTemplate = dojo.widget.buildAndAttachTemplate;
