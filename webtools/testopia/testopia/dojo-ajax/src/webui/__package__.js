/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.hostenv.conditionalLoadModule({
	common: ["dojo.xml.Parse", 
			 "dojo.webui.Widget", 
			 "dojo.webui.widgets.Parse", 
			 // "dojo.webui.DragAndDrop", 
			 "dojo.webui.WidgetManager"],
	browser: ["dojo.webui.DomWidget",
			  "dojo.webui.HtmlWidget"],
	svg: 	 ["dojo.webui.SvgWidget"]
});
dojo.hostenv.moduleLoaded("dojo.webui.*");
