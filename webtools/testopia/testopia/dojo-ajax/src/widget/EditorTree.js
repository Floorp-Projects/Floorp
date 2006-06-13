/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

/**
 * Tree model does all the drawing, visual node management etc.
 * Throws events about clicks on it, so someone may catch them and process
 * Tree knows nothing about DnD stuff, covered in TreeDragAndDrop and (if enabled) attached by controller
*/
dojo.provide("dojo.widget.EditorTree");

dojo.require("dojo.event.*");
dojo.require("dojo.dnd.*");
dojo.require("dojo.fx.html");
dojo.require("dojo.io.*");
dojo.require("dojo.widget.Container");
dojo.require("dojo.widget.Tree");
dojo.require("dojo.widget.EditorTreeNode");

// make it a tag
dojo.widget.tags.addParseTreeHandler("dojo:EditorTree");


dojo.widget.EditorTree = function() {
	dojo.widget.html.Container.call(this);
	this.acceptDropSources = [];

	this.eventNames = {
		// new node built.. Well, just built
		nodeCreate: "",
		// expand icon clicked
		treeClick: "",
		// node icon clicked
		iconClick: "",
		// node title clicked
		titleClick: ""
	};
}
dojo.inherits(dojo.widget.EditorTree, dojo.widget.html.Container);

/* extend DOES NOT copy recursively */
dojo.lang.extend(dojo.widget.EditorTree, {
	widgetType: "EditorTree",

	domNode: null,

	templateCssPath: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/EditorTree.css"),

	templateString: '<div class="dojoTree"></div>',

	/* Model events */
	eventNames: null,

	toggler: null,

	//
	// these icons control the grid and expando buttons for the whole tree
	//

	blankIconSrc: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_blank.gif").toString(),

	gridIconSrcT: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_t.gif").toString(), // for non-last child grid
	gridIconSrcL: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_l.gif").toString(), // for last child grid
	gridIconSrcV: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_v.gif").toString(), // vertical line
	gridIconSrcP: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_p.gif").toString(), // for under parent item child icons
	gridIconSrcC: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_c.gif").toString(), // for under child item child icons
	gridIconSrcX: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_x.gif").toString(), // grid for sole root item
	gridIconSrcY: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_y.gif").toString(), // grid for last rrot item
	gridIconSrcZ: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_grid_z.gif").toString(), // for under root parent item child icon

	expandIconSrcPlus: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_expand_plus.gif").toString(),
	expandIconSrcMinus: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_expand_minus.gif").toString(),
	expandIconSrcLoading: dojo.uri.dojoUri("src/widget/templates/images/EditorTree/treenode_loading.gif").toString(),


	iconWidth: 18,
	iconHeight: 18,


	//
	// tree options
	//

	showGrid: true,
	showRootGrid: true,

	toggle: "default",
	toggleDuration: 150,
	title: "My Tree", // used for debug only
	selector: null,

	initialize: function(args, frag){

		var sources;
		if ( (sources = args['acceptDropSources']) || (sources = args['acceptDropSources']) ) {
			this.acceptDropSources = sources.split(',');
		}

		switch (this.toggle) {

			case "fade": this.toggler = new dojo.widget.Tree.FadeToggle(); break;
			// buggy - try to open many nodes in FF (IE is ok)
			case "wipe": this.toggler = new dojo.widget.Tree.WipeToggle(); break;
			default    : this.toggler = new dojo.widget.Tree.DefaultToggle();
		}


		if (args['eventNaming'] == "default" || args['eventnaming'] == "default" ) { // IE || FF

			for (eventName in this.eventNames) {
				this.eventNames[eventName] = this.widgetId+"/"+eventName;
			}
			/*
			this.eventNames.watch("nodeCreate",
   				function (id,oldval,newval) {
      				alert("o." + id + " changed from " + oldval + " to " + newval);
      				return newval;
			   }
			  );
			 */

		//	alert(dojo.widget.manager.getWidgetById('firstTree').widgetId)
		//	alert(dojo.widget.manager.getWidgetById('firstTree').eventNames.nodeCreate);

		}


		if (args['controller']) {
			var controller = dojo.widget.manager.getWidgetById(args['controller']);

			controller.subscribeTree(this); // controller listens to my events
		}

		if (args['selector']) {
			this.selector = dojo.widget.manager.getWidgetById(args['selector']);
		}
		else {
			this.selector = new dojo.widget.createWidget("dojo.widget.EditorTreeSelector");
		}

	},



	postCreate: function() {
		this.buildTree();
	},

	buildTree: function() {

		dojo.html.disableSelection(this.domNode);

		for(var i=0; i<this.children.length; i++){

			this.children[i].isFirstNode = (i == 0) ? true : false;
			this.children[i].isLastNode = (i == this.children.length-1) ? true : false;
			this.children[i].parentNode = this; // root nodes have tree as parent

			var node = this.children[i].buildNode(this, 0);


			this.domNode.appendChild(node);
		}


		//
		// when we don't show root toggles, we need to auto-expand root nodes
		//

		if (!this.showRootGrid){
			for(var i=0; i<this.children.length; i++){
				this.children[i].expand();
			}
		}


		for(var i=0; i<this.children.length; i++){
			this.children[i].startMe();
		}
	},



	/**
	 * Move child to newParent as last child
	 * redraw tree and update icons
	*/
	changeParent: function(child, newParent) {
		//dojo.debug("Move "+child.title+" to "+newParent.title)

		/* do actual parent change here. Write remove child first */
		child.parentNode.removeChild(child);

		newParent.addChild(child, 0);

	},

	removeChild: function(child) {

		var parentNode = child.parentNode;

		var children = parentNode.children;

		for(var i=0; i<children.length; i++){
			if(children[i] === child){
				if (children.length>1) {
					if (i==0) {
						children[i+1].isFirstNode = true;
					}
					if (i==children.length-1) {
						children[i-1].isLastNode = true;
					}
				}
				children.splice(i, 1);
				break;
			}
		}

		child.domNode.parentNode.removeChild(child.domNode);


		//dojo.debug("removeChild: "+child.title+" from "+parentNode.title);

		if (children.length == 0) {
			// toggle empty container off
			if (parentNode.widgetType == 'EditorTreeNode') { // if has container
				parentNode.containerNode.style.display = 'none';
			}

		}

		parentNode.updateIconTree();

		return child;

	},



	// not called for initial tree building. See buildNode instead.
	// builds child html node if needed
	// index is "last node" by default
	addChild: function(child, index){
		//dojo.debug("This "+this.title+" Child "+child.title+" index "+index);

		if (dojo.lang.isUndefined(index)) {
			index = this.children.length;
		}

		//
		// this function gets called to add nodes to both trees and nodes, so it's a little confusing :)
		//

		if (child.widgetType != 'EditorTreeNode'){
			dojo.raise("You can only add EditorTreeNode widgets to a "+this.widgetType+" widget!");
			return;
		}

		// set/clean isFirstNode and isLastNode
		if (this.children.length){
			if (index == 0) {
				this.children[0].isFirstNode = false;
				child.isFirstNode = true;
			}
			else {
				child.isFirstNode = false;
			}
			if (index == this.children.length) {
				this.children[index-1].isLastNode = false;
				child.isLastNode = true;
			}
			else {
				child.isLastNode = false;
			}
		}
		else {
			child.isLastNode = true;
			child.isFirstNode = true;
		}



		// usually it is impossible to change "isFolder" state, but if anyone wants to add a child to leaf,
		// it is possible program-way.
		if (this.widgetType == 'EditorTreeNode'){
			this.isFolder = true;
			//this.isExpanded = false;
		}

		// adjust tree
		var tree = this.widgetType == 'EditorTreeNode' ? this.tree : this;
		dojo.lang.forEach(child.getDescendants(), function(elem) { elem.tree = tree; });

		/*
		var stack = [child];
		var elem;
		// change tree for all subnodes
		while (elem = stack.pop()) {
			//dojo.debug("Tree for "+elem.title);
			elem.tree = tree;
			dojo.lang.forEach(elem.children, function(elem) { stack.push(elem); });
		}
		*/

		// fix parent
		child.parentNode = this;


		// no dynamic loading for those who are parents already
		if (this.widgetType == 'EditorTreeNode') {
			this.loaded = this.loadStates.LOADED;
		}

		// if node exists - adjust its depth, otherwise build it
		if (child.domNodeInitialized) {
			//dojo.debug(this.widgetType)
			var d = (this.widgetType == 'EditorTreeNode') ? this.depth : 0;
			//dojo.debug('Depth is '+this.depth);
			child.adjustDepth(d-child.depth+1);
		}
		else {
			child.depth = this.widgetType == 'EditorTreeNode' ? this.depth+1 : 0;
			child.buildNode(child.tree, child.depth);
		}



		//dojo.debug(child.domNode.outerHTML)

		if (index < this.children.length) {

			//dojo.debug('insert '+index)
			//dojo.debugShallow(child);

			// insert
			this.children[index].domNode.parentNode.insertBefore(child.domNode, this.children[index].domNode);
		}
		else {
			if (this.widgetType == 'EditorTree') {
				this.domNode.appendChild(node);
			}
			else {

				//dojo.debug('append '+index)

			// enable container, cause I add first child into it
				this.containerNode.style.display = 'block';

				//this.domNode.insertBefore(child.domNode, this.containerNode);

				//dojo.debugShallow(this.containerNode);
				this.containerNode.appendChild(child.domNode);
				//dojo.debugShallow(this.containerNode);
			}
		}

		this.children.splice(index, 0, child);

		//dojo.lang.forEach(this.children, function(child) { dojo.debug("Child "+child.title); } );

		//dojo.debugShallow(child);

		this.updateIconTree();

		child.startMe();


	},

	makeBlankImg: function() {
		var img = document.createElement('img');

		img.style.width = this.iconWidth + 'px';
		img.style.height = this.iconHeight + 'px';
		img.src = this.blankIconSrc;
		img.style.verticalAlign = 'middle';

		return img;
	},


	/* Swap nodes with SAME parent */
	swapNodes: function(node1, node2) {
		if (node1 === node2) return;

		if (node1.parentNode !== node2.parentNode) {
			dojo.raise("You may only swap nodes with same parent")
		}

		var parentNode = node1.parentNode;

		var nodeIndex1 = node1.getParentIndex();
		var nodeIndex2 = node2.getParentIndex();

		/* Fix children order */
		parentNode.children[nodeIndex1] = node2;
		parentNode.children[nodeIndex2] = node1;

		/* swap isFirst/isLast flags */
		var a = node1.isLastNode;
		var b = node1.isFirstNode;
		node1.isLastNode = node2.isLastNode;
		node1.isFirstNode = node2.isFirstNode;
		node2.isLastNode = a;
		node2.isFirstNode = b;

		//dojo.debug(node1.title+" : "+node2.title)

		this.swapDomNodes(node1.domNode, node2.domNode);
		//parentNode.containerNode.insertBefore(node2.domNode, node1.domNode);
		//parentNode.containerNode.insertBefore(node1.domNode, node2.domNode);

		//parentNode.containerNode.removeChild(this.domNode);

		parentNode.updateIconTree();


	},


	/* Should go dojo.dom */
	swapDomNodes: function(node1, node2) {
		// replace node1 with node2
		//dojo.debug(node1.parentNode)

		// may have siblings only in n1 -> n2 order
		if (node2.nextSibling === node1) return this.swapDomNodes(node2, node1);

		var parentNode1 = node1.parentNode;
		var parentNode2 = node2.parentNode;

		var inserter1;
		var inserter2;
		var removed1;
		var removed2;

		if (node1.nextSibling === node2) {			// node1->node2
			if (node2.nextSibling) {
				var a2n = node2.nextSibling;
				inserter1 = function(newNode) { parentNode1.insertBefore(newNode, a2n); }
				inserter2 = function(newNode) { parentNode1.insertBefore(newNode, a2n); }
			}
			else {
				inserter1 = function(newNode) { parentNode1.appendChild(newNode); }
				inserter2 = function(newNode) { parentNode1.appendChild(newNode); }
			}
		}
		else {
			if (node1.nextSibling) {
				inserter1 = function(newNode) { parentNode1.insertBefore(newNode, node1.nextSibling); }
			}
			else {
				inserter1 = function(newNode) { parentNode1.appendChild(newNode); }
			}

			if (node2.nextSibling) {
				inserter2 = function(newNode) { parentNode2.insertBefore(newNode, node2.nextSibling); }
			}
			else {
				inserter2 = function(newNode) { parentNode2.appendChild(newNode); }
			}
		}


		removed1 = parentNode1.removeChild(node1);
		removed2 = parentNode2.removeChild(node2);

		// order is important cause of n1->n2 case
		inserter1.apply(this, [removed2]);
		inserter2.apply(this, [removed1]);


	},


	updateIconTree: function(){

		if (this.widgetType=='EditorTreeNode') {
			this.updateIcons();
		}

		for(var i=0; i<this.children.length; i++){
			this.children[i].updateIconTree();
		}

	}





});

