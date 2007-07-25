var qaMain = {
	htmlNS: "http://www.w3.org/1999/xhtml",
			
	openQATool : function() {
		window.open("chrome://qa/content/qa.xul", "_blank", "chrome,all,dialog=no,resizable=no");
	},
	onToolOpen : function() {
		if (qaPref.getPref(qaPref.prefBase+'.isFirstTime', 'bool') == true) {
			window.open("chrome://qa/content/setup.xul", "_blank", "chrome,all,dialog=yes");
        }
		
	},
};
qaMain.__defineGetter__("bundle", function(){return $("bundle_qa");});
qaMain.__defineGetter__("urlbundle", function(){return $("bundle_urls");});
function $() {
  var elements = new Array();

  for (var i = 0; i < arguments.length; i++) {
	var element = arguments[i];
	if (typeof element == 'string')
	  element = document.getElementById(element);

	if (arguments.length == 1)
	  return element;

	elements.push(element);
  }

  return elements;
}
