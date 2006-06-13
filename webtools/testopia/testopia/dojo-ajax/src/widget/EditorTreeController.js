/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/


dojo.provide("dojo.widget.EditorTreeController");

dojo.require("dojo.event.*");
dojo.require("dojo.dnd.*");
dojo.require("dojo.dnd.TreeDragAndDrop");
dojo.require("dojo.fx.html");
dojo.require("dojo.json")
dojo.require("dojo.io.*");
dojo.require("dojo.widget.Container");
dojo.require("dojo.widget.Tree");


dojo.widget.tags.addParseTreeHandler("dojo:EditorTreeController");


dojo.widget.EditorTreeController = function() {
	dojo.widget.HtmlWidget.call(this);

	this.eventNames = {
		select : "",
		collapse: "",
		expand: "",
		dblselect: "", // select already selected node.. Edit or whatever

		swap: "", // nodes of same parent were swapped
		move: "", // a child was moved from one place to another with parent change
		remove: ""
	};


	this.dragSources = {};
	this.dropTargets = {};
}

dojo.inherits(dojo.widget.EditorTreeController, dojo.widget.HtmlWidget);


dojo.lang.extend(dojo.widget.EditorTreeController, {
	widgetType: "EditorTreeController",

	//RPCUrl: "http://test.country-info.ru/json_tree.php",
	//RPCUrl: "http://tmp.x/jstree/www/json_tree.php",
	RPCUrl: "local",

	initialize: function(args, frag){

		if (args['eventNaming'] == "default" || args['eventnaming'] == "default" ) { // IE || FF
			for (eventName in this.eventNames) {
				this.eventNames[eventName] = this.widgetId+"/"+eventName;
			}
		}

	},


	enabledDND: true,

	/**
	 * Binds controller to tree events
	*/
	subscribeTree: function(tree) {

		if (!tree.eventNames.nodeCreate) dojo.raise("Can't subscribe controller to empty nodeCreate")
		if (!tree.eventNames.treeClick) dojo.raise("Can't subscribe controller to empty treeClick")
		if (!tree.eventNames.iconClick) dojo.raise("Can't subscribe controller to empty iconClick")
		if (!tree.eventNames.titleClick) dojo.raise("Can't subscribe controller to empty titleClick")

		dojo.event.topic.subscribe(tree.eventNames.nodeCreate, this, "onNodeCreate");
		dojo.event.topic.subscribe(tree.eventNames.treeClick, this, "onTreeClick");
		dojo.event.topic.subscribe(tree.eventNames.iconClick, this, "onIconClick");
		dojo.event.topic.subscribe(tree.eventNames.titleClick, this, "onTitleClick");
	},


	/**
	 * Checks whether it is ok to change parent of child to newParent
	 * May incur type checks etc
	 */
	canChangeParent: function(child, newParent){

//		dojo.debug('check for '+child+' '+newParent)

		// Can't move parent under child. check whether new parent is child of "child".
		var node = newParent;
		while(node.widgetType !== 'EditorTree') {
			//dojo.debugShallow(node.title)


			if (node === child) {
				// parent of newParent is child
				return false;
			}
			node = node.parentNode;
		}

		// check for newParent being a folder
		if (!newParent.isFolder) {
			return false;
		}

		// check if newParent is parent for child already
		for(var i=0;i<newParent.children.length; i++) {
			if (newParent.children[i] == child) return false;
		}

		return true;
	},


	getRPCUrl: function(action) {

		if (this.RPCUrl == "local") { // for demo and tests. May lead to widgetId collisions
			var dir = document.location.href.substr(0, document.location.href.lastIndexOf('/'));
			var localUrl = dir+"/"+action;
			//dojo.debug(localUrl);
			return localUrl;
		}


		if (!this.RPCUrl) {
			dojo.raise("Empty RPCUrl: can't load");
		}

		return this.RPCUrl + ( this.RPCUrl.indexOf("?") > -1 ? "&" : "?") + "action="+action;
	},

	/**
	 * Make request to server about moving children.
	 *
	 * Request returns "true" if move succeeded,
	 * object with error field if failed
	 *
	 * I can't leave DragObject floating until async request returns, need to return false/true
	 * so making it sync way...
	 *
	 * Also, "loading" icon is not shown until function finishes execution, so no indication for remote request.
	*/
	changeParentRemote: function(child, newParent){

			newParent.markLoading();

			var params = {
				// where from
				childId: child.widgetId,
				childTreeId: child.tree.widgetId,
				childOldParentId: child.parentNode.widgetId,
				// where to
				newParentId: newParent.widgetId,
				newParentTreeId: newParent.tree.widgetId
			}

			var query = dojo.io.argsFromMap(params);
			var requestUrl = this.getRPCUrl('changeParent');
			if(query != "") {
				requestUrl += (requestUrl.indexOf("?") > -1 ? "&" : "?") + query;
			}

			var response;
			var result;

			try{
				response = dojo.hostenv.getText(requestUrl);
				result = dj_eval("("+response+")");
			}catch(e){
				dojo.debug(e);
				dojo.debug(response);
				dojo.raise("Failed to load node");
			}


			//dojo.debugShallow(result)

			if (result == true) {
				/* change parent succeeded */
				child.tree.changeParent(child, newParent)
				this.updateDND(child);
				return true;
			}
			else if (dojo.lang.isObject(result)) {
				dojo.raise(result.error);
			}
			else {
				dojo.raise("Invalid response "+response)
			}


	},


	/**
	 * return true on success, false on failure
	*/
	changeParent: function(child, newParent) {
		// dojo.debug("Drop registered")
		/* move sourceTreeNode to new parent */
		if (!this.canChangeParent(child, newParent)) {
			return false;
		}

		/* load nodes into newParent in sync mode, if needed, first */
		if (newParent.state == newParent.loadStates.UNCHECKED) {
			this.loadRemote(newParent, true);
		}

		var oldParent = child.parentNode;

		var result = this.changeParentRemote(child, newParent);

		if (!result) return result;


		/* publish many events here about structural changes */
		dojo.event.topic.publish(this.eventNames.move,
			{ oldParent: oldParent, newParent: child.parentNode, child: child }
		);

		this.expand(newParent);

		return result;
	},



	/**
	 * Common RPC error handler (dies)
	*/
	RPCErrorHandler: function(type, obj) {
		dojo.raise("RPC error occured! Application restart/tree refresh recommended.");
	},



	/**
	 * Add all loaded nodes from array obj as node children and expand it
	*/
	loadProcessResponse: function(node, newChildren) {
		if (!dojo.lang.isArray(newChildren)) {
			dojo.raise('Not array loaded: '+newChildren);
		}
		for(var i=0; i<newChildren.length; i++) {
			// looks like dojo.widget.manager needs no special "add" command
			var newChild = dojo.widget.createWidget(node.widgetType, newChildren[i]);
			node.addChild(newChild);

			//dojo.debug(dojo.widget.manager.getWidgetById(newChild.widgetId))
		}
		node.state = node.loadStates.LOADED;
		this.expand(node);
	},


	/**
	 * Load children of the node from server
	 * Synchroneous loading doesn't break control flow
	 * I need sync mode for DnD
	*/
	loadRemote: function(node, sync){
			node.markLoading();


			var params = {
				treeId: node.tree.widgetId,
				nodeId: node.widgetId
			};

			var requestUrl = this.getRPCUrl('getChildren');
			//dojo.debug(requestUrl)

			if (!sync) {
				dojo.io.bind({
					url: requestUrl,
					/* I hitch to get this.loadOkHandler */
					load: dojo.lang.hitch(this, function(type, obj) { this.loadProcessResponse(node, obj) } ),
					error: dojo.lang.hitch(this, this.RPCErrorHandler),
					mimetype: "text/json",
					preventCache: true,
					sync: sync,
					content: params
				});

				return;
			}
			else {
				var query = dojo.io.argsFromMap(params);
				if(query != "") {
					requestUrl += (requestUrl.indexOf("?") > -1 ? "&" : "?") + query;
				}

				var newChildren;

				try{
					var response = dojo.hostenv.getText(requestUrl);
					newChildren = dj_eval("("+response+")");
				}catch(e){
					dojo.debug(e);
					dojo.debug(response);
					dojo.raise("Failed to load node");
				}

				this.loadProcessResponse(node, newChildren);

			}
	},

	onTreeClick: function(message){

		//dojo.debug("EXPAND")

		var node = message.source;

		if (node.state == node.loadStates.UNCHECKED) {
			this.loadRemote(node);
		}
		else if (node.isExpanded){
			this.collapse(node);
		}
		else {
			this.expand(node);
		}
	},


	onIconClick: function(message){
		this.onTitleClick(message);
	},

	onTitleClick: function(message){
		var node = message.source;
		var e = message.event;

		if (node.tree.selector.selectedNode === node){
			dojo.event.topic.publish(this.eventNames.dblselect, { source: node });
			return;
		}

		/* deselect old node */
		if (node.tree.selector.selectedNode) {
			this.deselect(node.tree.selector.selectedNode);
		}

		node.tree.selector.selectedNode = node;
		this.select(node);
	},


	/**
	 * Process drag'n'drop -> drop
	 * NOT event-driven, because its result is important (success/false)
	 * in event system subscriber can't return a result into _current function control-flow_
	 * @return true on success, false on failure
	*/
	processDrop: function(sourceNode, targetNode){
		//dojo.debug('drop')
		return this.changeParent(sourceNode, targetNode)
	},


	onNodeCreate: function(message) {
		this.registerDNDNode(message.source);
	},

	select: function(node){
		node.markSelected();

		dojo.event.topic.publish(this.eventNames.select, {source: node} );
	},

	deselect: function(node){
		node.unMarkSelected();

		node.tree.selector.selectedNode = null;
	},

	expand: function(node) {
		if (node.isExpanded) return;
		node.expand();
		dojo.event.topic.publish(this.eventNames.expand, {source: node} );
	},

	collapse: function(node) {
		if (!node.isExpanded) return;

		node.collapse();
		dojo.event.topic.publish(this.eventNames.collapse, {source: node} );
	},


	/**
	 * Controller(node model) creates DNDNodes because it passes itself to node for synchroneous drops processing
	 * I can't process DnD with events cause an event can't return result success/false
	*/
	registerDNDNode: function(node) {


		//dojo.debug("registerDNDNode node "+node.title+" tree "+node.tree+" accepted sources "+node.tree.acceptDropSources);

		/* I drag label, not domNode, because large domNodes are very slow to copy and large to drag */

		var source = null;
		var target = null;

		var source = new dojo.dnd.TreeDragSource(node.labelNode, this, node.tree.widgetId, node);
		this.dragSources[node.widgetId] = source;

		//dojo.debugShallow(node.tree.widgetId)
		var target = new dojo.dnd.TreeDropTarget(node.labelNode, this, node.tree.acceptDropSources, node);
		this.dropTargets[node.widgetId] = target;


		if (!this.enabledDND) {
			if (source) dojo.dnd.dragManager.unregisterDragSource(source);
			if (target) dojo.dnd.dragManager.unregisterDropTarget(target);
		}

		//dojo.debug("registerDNDNode "+this.dragSources[node.widgetId].treeNode.title)


	},

	unregisterDNDNode: function(node) {

		//dojo.debug("unregisterDNDNode "+node.title)
		//dojo.debug("unregisterDNDNode "+this.dragSources[node.widgetId].treeNode.title)

		if (this.dragSources[node]) {
			dojo.dnd.dragManager.unregisterDragSource(this.dragSources[node.widgetId]);
			delete this.dragSources[node.widgetId];
		}

		if (this.dropTargets[node]) {
			dojo.dnd.dragManager.unregisterDropTarget(this.dropTargets[node.widgetId]);
			delete this.dropTargets[node.widgetId];
		}
	},

	// update types for DND right accept
	// E.g when move from one tree to another tree
	updateDND: function(node) {

		this.unregisterDNDNode(node);

		this.registerDNDNode(node);


		//dojo.debug("!!!"+this.dropTargets[node].acceptedTypes)

		for(var i=0; i<node.children.length; i++) {
			// dojo.debug(node.children[i].title);
			this.updateDND(node.children[i]);
		}

	},




	// -----------------------------------------------------------------------------
	//                             Swap & move nodes stuff
	// -----------------------------------------------------------------------------


	/* after all local checks I run remote call for swap */
	swapNodesRemote: function(node1, node2, callback) {

			var params = {
				treeId: this.widgetId,
				node1Id: node1.widgetId,
				node1Idx: node1.getParentIndex(),
				node2Id: node2.widgetId,
				node2Idx: node2.getParentIndex()
			}

			dojo.io.bind({
					url: this.getRPCUrl('swapNodes'),
					/* I hitch to get this.loadOkHandler */
					load: dojo.lang.hitch(this, function(type, obj) {
						this.swapNodesProcessResponse(node1, node2, callback, obj) }
					),
					error: dojo.lang.hitch(this, this.RPCErrorHandler),
					mimetype: "text/json",
					preventCache: true,
					content: params
			});

	},

	swapNodesProcessResponse: function(node1, node2, callback, result) {
		if (result == true) {
			/* change parent succeeded */
			this.doSwapNodes(node1, node2);
			if (callback) {
				// provide context manually e.g with dojo.lang.hitch.
				callback.apply(this, [node1, node2]);
			}

			return true;
		}
		else if (dojo.lang.isObject(result)) {
			dojo.raise(result.error);
		}
		else {
			dojo.raise("Invalid response "+obj)
		}


	},

	/* node swapping requires remote checks. This function does the real job only w/o any checks */
	doSwapNodes: function(node1, node2) {
		/* publish many events here about structural changes */

		node1.tree.swapNodes(node1, node2);

		// nodes AFTER swap
		dojo.event.topic.publish(this.eventNames.swap,
			{ node1: node1, node2: node2 }
		);

	},

	canSwapNodes: function(node1, node2) {
		return true;
	},

	/* main command for node swaps, with remote call */
	swapNodes: function(node1, node2, callback) {
		if (!this.canSwapNodes(node1, node2)) {
			return false;
		}

		return this.swapNodesRemote(node1, node2, callback);

	},

	// return false if local check failed
	moveUp: function(node, callback) {

		var prevNode = node.getLeftSibling();

		if (!prevNode) return false;

		return this.swapNodes(prevNode, node, callback);
	},


	// return false if local check failed
	moveDown: function(node, callback) {

		var nextNode = node.getRightSibling();

		if (!nextNode) return false;

		return this.swapNodes(nextNode, node, callback);
	},


	// -----------------------------------------------------------------------------
	//                             Remove nodes stuff
	// -----------------------------------------------------------------------------



	canRemoveNode: function(node) {
		return true;
	},

	removeNode: function(node, callback) {
		if (!this.canRemoveNode(node)) {
			return false;
		}

		return this.removeNodeRemote(node, callback);

	},


	removeNodeRemote: function(node, callback) {

			var params = {
				treeId: node.tree.widgetId,
				nodeId: node.widgetId
			}

			dojo.io.bind({
					url: this.getRPCUrl('removeNode'),
					/* I hitch to get this.loadOkHandler */
					load: dojo.lang.hitch(this, function(type, obj) {
						this.removeNodeProcessResponse(node, callback, obj) }
					),
					error: dojo.lang.hitch(this, this.RPCErrorHandler),
					mimetype: "text/json",
					preventCache: true,
					content: params
			});

	},

	removeNodeProcessResponse: function(node, callback, result) {
		if (result == true) {
			/* change parent succeeded */
			this.doRemoveNode(node, node);
			if (callback) {
				// provide context manually e.g with dojo.lang.hitch.
				callback.apply(this, [node]);
			}

			return true;
		}
		else if (dojo.lang.isObject(result)) {
			dojo.raise(result.error);
		}
		else {
			dojo.raise("Invalid response "+obj)
		}


	},


	/* node swapping requires remote checks. This function does the real job only w/o any checks */
	doRemoveNode: function(node) {
		/* publish many events here about structural changes */

		if (node.tree.selector.selectedNode === node) {
			this.deselect(node);
		}

		this.unregisterDNDNode(node);

		removed_node = node.tree.removeChild(node);

		// nodes AFTER swap
		dojo.event.topic.publish(this.eventNames.remove,
			{ node: removed_node }
		);

	},


	// -----------------------------------------------------------------------------
	//                             Create node stuff
	// -----------------------------------------------------------------------------




	canCreateNode: function(parent, index, data) {
		if (!parent.isFolder) return false;

		return true;
	},


	/* send data to server and add child from server */
	/* data may contain an almost ready child, or anything else, suggested to server */
	/* server responds with child data to be inserted */
	createNode: function(parent, index, data, callback) {
		if (!this.canCreateNode(parent, index, data)) {
			return false;
		}


		/* load nodes into newParent in sync mode, if needed, first */
		if (parent.state == parent.loadStates.UNCHECKED) {
			this.loadRemote(parent, true);
		}


		return this.createNodeRemote(parent, index, data, callback);

	},


	createNodeRemote: function(parent, index, data, callback) {

			var params = {
				treeId: parent.tree.widgetId,
				parentId: parent.widgetId,
				index: index,
				data: dojo.json.serialize(data)
			}

			dojo.io.bind({
					url: this.getRPCUrl('createNode'),
					/* I hitch to get this.loadOkHandler */
					load: dojo.lang.hitch(this, function(type, obj) {
						this.createNodeProcessResponse(parent, index, data, callback, obj) }
					),
					error: dojo.lang.hitch(this, this.RPCErrorHandler),
					mimetype: "text/json",
					preventCache: true,
					content: params
			});

	},

	createNodeProcessResponse: function(parent, index, data, callback, response) {

		if (parent.widgetType != 'EditorTreeNode') {
			dojo.raise("Can only add children to EditorTreeNode")
		}

		if (!dojo.lang.isObject(response)) {
			dojo.raise("Invalid response "+response)
		}
		if (!dojo.lang.isUndefined(response.error)) {
			dojo.raise(response.error);
		}


		this.doCreateNode(parent, index, response);

		if (callback) {
			// provide context manually e.g with dojo.lang.hitch.
			callback.apply(this, [parent, index, response]);
		}

	},


	doCreateNode: function(parent, index, child) {

		var newChild = dojo.widget.createWidget("EditorTreeNode", child);

		parent.addChild(newChild, index);

		this.expand(parent);
	}


});