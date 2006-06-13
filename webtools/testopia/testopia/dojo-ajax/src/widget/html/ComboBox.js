/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.html.ComboBox");
dojo.require("dojo.widget.ComboBox");
dojo.require("dojo.widget.*");
dojo.require("dojo.io.*");
dojo.require("dojo.lfx.*");
dojo.require("dojo.dom");
dojo.require("dojo.html");
dojo.require("dojo.string");
dojo.require("dojo.widget.html.stabile");

dojo.widget.html.ComboBox = function(){
	dojo.widget.ComboBox.call(this);
	dojo.widget.HtmlWidget.call(this);

	this.autoComplete = true;
	this.formInputName = "";
	this.name = ""; // clone in the name from the DOM node
	this.textInputNode = null;
	this.comboBoxValue = null;
	this.comboBoxSelectionValue = null;
	this.optionsListWrapper = null;
	this.optionsListNode = null;
	this.downArrowNode = null;
	this.cbTableNode = null;
	this.searchTimer = null;
	this.searchDelay = 100;
	this.dataUrl = "";
	// maxListLength limits list to X visible rows, scroll on rest 
	this.maxListLength = 8; 
	// mode can also be "remote" for JSON-returning live search or "html" for
	// dumber live search
	this.mode = "local"; 
	this.selectedResult = null;
	this._highlighted_option = null;
	this._prev_key_backspace = false;
	this._prev_key_esc = false;
	this._result_list_open = false;
	this._gotFocus = false;
	this._mouseover_list = false;

}

dojo.inherits(dojo.widget.html.ComboBox, dojo.widget.HtmlWidget);

// copied from superclass since we can't really over-ride via prototype
dojo.lang.extend(dojo.widget.html.ComboBox, dojo.widget.ComboBox.defaults);

dojo.lang.extend(dojo.widget.html.ComboBox, {


	templatePath: dojo.uri.dojoUri("src/widget/templates/HtmlComboBox.html"),
	templateCssPath: dojo.uri.dojoUri("src/widget/templates/HtmlComboBox.css"),

	setValue: function(value) {
		this.comboBoxValue.value = value;
		if (this.textInputNode.value != value) { // prevent mucking up of selection
			this.textInputNode.value = value;
		}
		dojo.widget.html.stabile.setState(this.widgetId, this.getState(), true);
	},

	getValue: function() {
		return this.comboBoxValue.value;
	},

	getState: function() {
		return {value: this.getValue()};
	},

	setState: function(state) {
		this.setValue(state.value);
	},

	getCaretPos: function(element){
		// khtml 3.5.2 has selection* methods as does webkit nightlies from 2005-06-22
		if(dojo.lang.isNumber(element.selectionStart)){
			// FIXME: this is totally borked on Moz < 1.3. Any recourse?
			return element.selectionStart;
		}else if(dojo.render.html.ie){
			// in the case of a mouse click in a popup being handled,
			// then the document.selection is not the textarea, but the popup
			// var r = document.selection.createRange();
			// hack to get IE 6 to play nice. What a POS browser.
			var tr = document.selection.createRange().duplicate();
			var ntr = element.createTextRange();
			tr.move("character",0);
			ntr.move("character",0);
			try {
				// If control doesnt have focus, you get an exception.
				// Seems to happen on reverse-tab, but can also happen on tab (seems to be a race condition - only happens sometimes).
				// There appears to be no workaround for this - googled for quite a while.
				ntr.setEndPoint("EndToEnd", tr);
				return String(ntr.text).replace(/\r/g,"").length;
			} catch (e) {
				return 0; // If focus has shifted, 0 is fine for caret pos.
			}
			
		}
	},

	setCaretPos: function(element, location){
		location = parseInt(location);
		this.setSelectedRange(element, location, location);
	},

	setSelectedRange: function(element, start, end){
		if(!end){ end = element.value.length; }  // NOTE: Strange - should be able to put caret at start of text?
		// Mozilla
		// parts borrowed from http://www.faqts.com/knowledge_base/view.phtml/aid/13562/fid/130
		if(element.setSelectionRange){
			element.focus();
			element.setSelectionRange(start, end);
		}else if(element.createTextRange){ // IE
			var range = element.createTextRange();
			with(range){
				collapse(true);
				moveEnd('character', end);
				moveStart('character', start);
				select();
			}
		}else{ //otherwise try the event-creation hack (our own invention)
			// do we need these?
			element.value = element.value;
			element.blur();
			element.focus();
			// figure out how far back to go
			var dist = parseInt(element.value.length)-end;
			var tchar = String.fromCharCode(37);
			var tcc = tchar.charCodeAt(0);
			for(var x = 0; x < dist; x++){
				var te = document.createEvent("KeyEvents");
				te.initKeyEvent("keypress", true, true, null, false, false, false, false, tcc, tcc);
				twe.dispatchEvent(te);
			}
		}
	},

	// does the keyboard related stuff
	_handleKeyEvents: function(evt){
		if(evt.ctrlKey || evt.altKey){ return; }

		// reset these
		this._prev_key_backspace = false;
		this._prev_key_esc = false;

		var k = dojo.event.browser.keys;
		var doSearch = true;

		// mozilla quirk 
		// space has no keyCode in mozilla
		var keyCode = evt.keyCode;
		if(keyCode==0 && evt.charCode==k.KEY_SPACE){
			keyCode = k.KEY_SPACE;
		}
		switch(keyCode){
 			case k.KEY_DOWN_ARROW:
				if(!this._result_list_open){
					this.startSearchFromInput();
				}
				this.highlightNextOption();
				dojo.event.browser.stopEvent(evt);
				return;
			case k.KEY_UP_ARROW:
				this.highlightPrevOption();
				dojo.event.browser.stopEvent(evt);
				return;
			case k.KEY_ENTER:
				// prevent to send form if we press enter with list open
				if(this._result_list_open){
					dojo.event.browser.stopEvent(evt);
				}
				// falltrough
			case k.KEY_TAB:
				// using linux alike tab for autocomplete
				if(!this.autoComplete && this._result_list_open && this._highlighted_option){
					dojo.event.browser.stopEvent(evt);
					this.selectOption({ 'target': this._highlighted_option, 'noHide': true });

					// put caret last
					this.setSelectedRange(this.textInputNode, this.textInputNode.value.length, null);
				}else{
					this.selectOption();
					return;
				}
				break;
			case k.KEY_SPACE:
				if(this._result_list_open && this._highlighted_option){
					dojo.event.browser.stopEvent(evt);
					this.selectOption();
					this.hideResultList();
					return;
				}
				break;
			case k.KEY_ESCAPE:
				this.hideResultList();
				this._prev_key_esc = true;
				return;
			case k.KEY_BACKSPACE:
				this._prev_key_backspace = true;
				if(!this.textInputNode.value.length){
					this.hideResultList();
					doSearch = false;
				}
				break;
			case k.KEY_RIGHT_ARROW: // falltrough
			case k.KEY_LEFT_ARROW: // falltrough
			case k.KEY_SHIFT:
				doSearch = false;
				break;
			default:// non char keys (F1-F12 etc..)  shouldnt open list
				if(evt.charCode==0){
					doSearch = false;
				}
		}

// 		this.setValue(this.textInputNode.value);
		if(this.searchTimer){
			clearTimeout(this.searchTimer);
		}
		if(doSearch){
			// if we have gotten this far we dont want to keep our highlight
			this.blurOptionNode();

			// need to wait a tad before start search so that the event bubbels through DOM and we have value visible
			this.searchTimer = setTimeout(dojo.lang.hitch(this, this.startSearchFromInput), this.searchDelay);
		}
	},

	onKeyDown: function(evt){
		// IE needs to stop keyDown others need to stop keyPress
		if(!document.createEvent){ // only IE
			this._handleKeyEvents(evt);
		}
		// FIXME: What about ESC ??
	},

	onKeyPress: function(evt){
		if(document.createEvent){ // never IE
			this._handleKeyEvents(evt);
		}
	},

	onKeyUp: function(evt){
		this.setValue(this.textInputNode.value);
	},

	setSelectedValue: function(value){
		// FIXME, not sure what to do here!
		this.comboBoxSelectionValue.value = value;
		this.hideResultList();
	},

	// opera, khtml, safari doesnt support node.scrollIntoView(), workaround
	scrollIntoView: function(){
		var node = this._highlighted_option;
		var parent = this.optionsListNode;
		// dont rely on that node.scrollIntoView works just because the function is there
		// it doesnt work in Konqueror or Opera even though the function is there and probably
		// not safari either
		// dont like browser sniffs implementations but sometimes you have to use it
		if(dojo.render.html.ie || dojo.render.html.mozilla){
			// IE, mozilla
			node.scrollIntoView(false);	
		}else{
			var parentBottom = parent.scrollTop + dojo.style.getInnerHeight(parent);
			var nodeBottom = node.offsetTop + dojo.style.getOuterHeight(node);
			if(parentBottom < nodeBottom){
				parent.scrollTop += (nodeBottom - parentBottom);
			}else if(parent.scrollTop > node.offsetTop){
				parent.scrollTop -= (parent.scrollTop - node.offsetTop);
			}
		}
	},

	// does the actual highlight
	focusOptionNode: function(node){
		if(this._highlighted_option != node){
			this.blurOptionNode();
			this._highlighted_option = node;
			dojo.html.addClass(this._highlighted_option, "dojoComboBoxItemHighlight");
		}
	},

	// removes highlight on highlighted
	blurOptionNode: function(){
		if(this._highlighted_option){
			dojo.html.removeClass(this._highlighted_option, "dojoComboBoxItemHighlight");
			this._highlighted_option = null;
		}
	},

	highlightNextOption: function(){
		if((!this._highlighted_option) || !this._highlighted_option.parentNode){
			this.focusOptionNode(this.optionsListNode.firstChild);
		}else if(this._highlighted_option.nextSibling){
			this.focusOptionNode(this._highlighted_option.nextSibling);
		}
		this.scrollIntoView();
	},

	highlightPrevOption: function(){
		if(this._highlighted_option && this._highlighted_option.previousSibling){
			this.focusOptionNode(this._highlighted_option.previousSibling);
		}else{
			this._highlighted_option = null;
			this.hideResultList();
			return;
		}
		this.scrollIntoView();
	},

	itemMouseOver: function(evt){
		this.focusOptionNode(evt.target);
		dojo.html.addClass(this._highlighted_option, "dojoComboBoxItemHighlight");
	},

	itemMouseOut: function(evt){
		this.blurOptionNode();
	},

	fillInTemplate: function(args, frag){
		// FIXME: need to get/assign DOM node names for form participation here.
		this.comboBoxValue.name = this.name;
		this.comboBoxSelectionValue.name = this.name+"_selected";

		// NOTE: this doesn't copy style info inherited from classes;
		// it's just primitive support for direct style setting
		var source = this.getFragNodeRef(frag);
		if ( source.style ){
			// get around opera wich doesnt have cssText, and IE wich bugs on setAttribute 
			if(dojo.lang.isUndefined(source.style.cssText)){ 
				this.domNode.setAttribute("style", source.getAttribute("style")); 
			}else{
				this.domNode.style.cssText = source.style.cssText; 
			}
		}

		// FIXME: add logic
		this.dataProvider = new dojo.widget.ComboBoxDataProvider();

		if(!dojo.string.isBlank(this.dataUrl)){
			if("local" == this.mode){
				var _this = this;
				dojo.io.bind({
					url: this.dataUrl,
					load: function(type, data, evt){ 
						if(type=="load"){
							if(!dojo.lang.isArray(data)){
								var arrData = [];
								for(var key in data){
									arrData.push([data[key], key]);
								}
								data = arrData;
							}
							_this.dataProvider.setData(data);
						}
					},
					mimetype: "text/json"
				});
			}else if("remote" == this.mode){
				this.dataProvider = new dojo.widget.incrementalComboBoxDataProvider(this.dataUrl);
			}
		}else{
			// check to see if we can populate the list from <option> elements
			var node = frag["dojo:"+this.widgetType.toLowerCase()]["nodeRef"];
			if((node)&&(node.nodeName.toLowerCase() == "select")){
				// NOTE: we're not handling <optgroup> here yet
				var opts = node.getElementsByTagName("option");
				var ol = opts.length;
				var data = [];
				for(var x=0; x<ol; x++){
					var keyValArr = [new String(opts[x].innerHTML), new String(opts[x].value)];
					data.push(keyValArr);
					if(opts[x].selected){ 
						this.setValue(keyValArr[0]); 
						this.comboBoxSelectionValue.value = keyValArr[1];
					}
				}
				this.dataProvider.setData(data);
			}
		}

		// Prevent IE bleed-through problem
		this.optionsIframe = new dojo.html.BackgroundIframe(this.optionsListWrapper);
		this.optionsIframe.size([0,0,0,0]);
	},


	focus: function(){
		// summary
		//	set focus to input node from code
		this.tryFocus();
	},

	openResultList: function(results){
		this.clearResultList();
		if(!results.length){
			this.hideResultList();
		}

		if(	(this.autoComplete)&&
			(results.length)&&
			(!this._prev_key_backspace)&&
			(this.textInputNode.value.length > 0)){
			var cpos = this.getCaretPos(this.textInputNode);
			// only try to extend if we added the last charachter at the end of the input
			if((cpos+1) > this.textInputNode.value.length){
				// only add to input node as we would overwrite Capitalisation of chars
				this.textInputNode.value += results[0][0].substr(cpos);
				// build a new range that has the distance from the earlier
				// caret position to the end of the first string selected
				this.setSelectedRange(this.textInputNode, cpos, this.textInputNode.value.length);
			}
		}

		var even = true;
		while(results.length){
			var tr = results.shift();
			if(tr){
				var td = document.createElement("div");
				td.appendChild(document.createTextNode(tr[0]));
				td.setAttribute("resultName", tr[0]);
				td.setAttribute("resultValue", tr[1]);
				td.className = "dojoComboBoxItem "+((even) ? "dojoComboBoxItemEven" : "dojoComboBoxItemOdd");
				even = (!even);
				this.optionsListNode.appendChild(td);
				dojo.event.connect(td, "onmouseover", this, "itemMouseOver");
				dojo.event.connect(td, "onmouseout", this, "itemMouseOut");
			}
		}

		// show our list (only if we have content, else nothing)
		this.showResultList();
	},

	onFocusInput: function(){
		this._hasFocus = true;
	},

	onBlurInput: function(){
		this._hasFocus = false;
		this._handleBlurTimer(true, 100);
	},

	// collect all blur timers issues here
	_handleBlurTimer: function(/*Boolean*/clear, /*Number*/ millisec){
		if(this.blurTimer && (clear || millisec)){
			clearTimeout(this.blurTimer);
		}
		if(millisec){ // we ignore that zero is false and never sets as that never happens in this widget
			this.blurTimer = dojo.lang.setTimeout(this, "checkBlurred", millisec);
		}
	},

	// these 2 are needed in IE and Safari as inputTextNode looses focus when scrolling optionslist
	_onMouseOver: function(evt){
		if(!this._mouseover_list){
			this._handleBlurTimer(true, 0);
			this._mouseover_list = true;
		}
	},

	_onMouseOut:function(evt){
		var relTarget = evt.relatedTarget;
		if(!relTarget || relTarget.parentNode!=this.optionsListNode){
			this._mouseover_list = false;
			this._handleBlurTimer(true, 100);
			this.tryFocus();
		}
	},

	checkBlurred: function(){
		if(!this._hasFocus && !this._mouseover_list){
			this.hideResultList();
		}
	},

	sizeBackgroundIframe: function(){
		var w = dojo.style.getOuterWidth(this.optionsListNode);
		var h = dojo.style.getOuterHeight(this.optionsListNode);
		if( w==0 || h==0 ){
			// need more time to calculate size
			dojo.lang.setTimeout(this, "sizeBackgroundIframe", 100);
			return;
		}
		if(this._result_list_open){
			this.optionsIframe.size([0,0,w,h]);
		}
	},

	selectOption: function(evt){
		if(!evt){
			evt = { target: this._highlighted_option };
		}

		if(!dojo.dom.isDescendantOf(evt.target, this.optionsListNode)){
			return;
		}

		var tgt = evt.target;
		while((tgt.nodeType!=1)||(!tgt.getAttribute("resultName"))){
			tgt = tgt.parentNode;
			if(tgt === document.body){
				return false;
			}
		}

		this.textInputNode.value = tgt.getAttribute("resultName");
		this.selectedResult = [tgt.getAttribute("resultName"), tgt.getAttribute("resultValue")];
		this.setValue(tgt.getAttribute("resultName"));
		this.comboBoxSelectionValue.value = tgt.getAttribute("resultValue");
		if(!evt.noHide){
			this.hideResultList();
			this.setSelectedRange(this.textInputNode, 0, null);
		}
		this.tryFocus();
	},

	clearResultList: function(){
		var oln = this.optionsListNode;
		while(oln.firstChild){
			dojo.event.disconnect(oln.firstChild, "onmouseover", this, "itemMouseOver");
			dojo.event.disconnect(oln.firstChild, "onmouseout", this, "itemMouseOut");
			oln.removeChild(oln.firstChild);
		}
	},

	hideResultList: function(){
		if(this._result_list_open){
			this._result_list_open = false;
			this.optionsIframe.size([0,0,0,0]);
			dojo.lfx.fadeHide(this.optionsListNode, 200).play();
		}
	},

	showResultList: function(){
		// Our dear friend IE doesnt take max-height so we need to calculate that on our own every time
		var childs = this.optionsListNode.childNodes;
		if(childs.length){
			var visibleCount = this.maxListLength;
			if(childs.length < visibleCount){
				visibleCount = childs.length;
			}

			with(this.optionsListNode.style){
				display = "";
				height = ((visibleCount) ? (dojo.style.getOuterHeight(childs[0]) * visibleCount) : 0)+"px";
				width = dojo.html.getOuterWidth(this.cbTableNode)+"px";
			}
			// only fadein once (flicker)
			if(!this._result_list_open){
				dojo.html.setOpacity(this.optionsListNode, 0);
				dojo.lfx.fadeIn(this.optionsListNode, 200).play();
			}
			
			// prevent IE bleed through
			this._iframeTimer = dojo.lang.setTimeout(this, "sizeBackgroundIframe", 200);
			this._result_list_open = true;
		}else{
			this.hideResultList();
		}
	},

	handleArrowClick: function(){
		this._handleBlurTimer(true, 0);
		this.tryFocus();
		if(this._result_list_open){
			this.hideResultList();
		}else{
			this.startSearchFromInput();
		}
	},

	tryFocus: function(){
		try {    
			this.textInputNode.focus();
		} catch (e) {
			// element isnt focusable if disabled, or not visible etc - not easy to test for.
 		};
	},
	
	startSearchFromInput: function(){
		this.startSearch(this.textInputNode.value);
	},

	postCreate: function(){
		dojo.event.connect(this, "startSearch", this.dataProvider, "startSearch");
		dojo.event.connect(this.dataProvider, "provideSearchResults", this, "openResultList");
		dojo.event.connect(this.textInputNode, "onblur", this, "onBlurInput");
		dojo.event.connect(this.textInputNode, "onfocus", this, "onFocusInput");

		var s = dojo.widget.html.stabile.getState(this.widgetId);
		if (s) {
			this.setState(s);
		}
	}
});
