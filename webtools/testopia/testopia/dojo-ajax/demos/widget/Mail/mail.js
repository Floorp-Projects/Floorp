/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

// Mail demo javascript code

// Display list of messages (upper right pane)
function displayList(){
    this.update = function(message) {
        var clickedTreeNode =     
            message.node;

		var listPane = dojo.widget.getWidgetById("listPane");
		var url = "Mail/"+clickedTreeNode.title.replace(" ","") + ".html";
		
		listPane.setUrl(url);
    };
}
var displayer = new displayList();
var nodeSelectionTopic = dojo.event.topic.getTopic("listSelected");
nodeSelectionTopic.subscribe(displayer, "update");

// Display a single message (in bottom right pane)
function displayMessage(name){
		var contentPane = dojo.widget.getWidgetById("contentPane");
		var url = "Mail/"+name.replace(" ","") + ".html";
		contentPane.setUrl(url);
}