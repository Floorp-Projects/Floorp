/*
moo.fx pack, effects extensions for moo.fx.
by Valerio Proietti (http://mad4milk.net) MIT-style LICENSE
for more info visit (http://moofx.mad4milk.net).
Friday, April 14, 2006
v 1.2.4
*/

//smooth scroll
fx.Scroll = Class.create();
fx.Scroll.prototype = Object.extend(new fx.Base(), {
	initialize: function(options) {
		this.setOptions(options);
	},

	scrollTo: function(el){
		var dest = Position.cumulativeOffset($(el))[1];
		var client = window.innerHeight || document.documentElement.clientHeight;
		var full = document.documentElement.scrollHeight;
		var top = window.pageYOffset || document.body.scrollTop || document.documentElement.scrollTop;
		if (dest+client > full) this.custom(top, dest - client + (full-dest));
		else this.custom(top, dest);
	},

	increase: function(){
		window.scrollTo(0, this.now);
	}
});

//text size modify, now works with pixels too.
fx.Text = Class.create();
fx.Text.prototype = Object.extend(new fx.Base(), {
	initialize: function(el, options) {
		this.el = $(el);
		this.setOptions(options);
		if (!this.options.unit) this.options.unit = "em";
	},

	increase: function() {
		this.el.style.fontSize = this.now + this.options.unit;
	}
});

//composition effect: widht/height/opacity
fx.Combo = Class.create();
fx.Combo.prototype = {
	setOptions: function(options) {
		this.options = {
			opacity: true,
			height: true,
			width: false
		}
		Object.extend(this.options, options || {});
	},

	initialize: function(el, options) {
		this.el = $(el);
		this.setOptions(options);
		if (this.options.opacity) {
			this.o = new fx.Opacity(el, options);
			options.onComplete = null;
		}
		if (this.options.height) {
			this.h = new fx.Height(el, options);
			options.onComplete = null;
		}
		if (this.options.width) this.w = new fx.Width(el, options);
	},
	
	toggle: function() { this.checkExec('toggle'); },

	hide: function(){ this.checkExec('hide'); },
	
	clearTimer: function(){ this.checkExec('clearTimer'); },
	
	checkExec: function(func){
		if (this.o) this.o[func]();
		if (this.h) this.h[func]();
		if (this.w) this.w[func]();
	},
	
	//only if width+height
	resizeTo: function(hto, wto) {
		if (this.h && this.w) {
			this.h.custom(this.el.offsetHeight, this.el.offsetHeight + hto);
			this.w.custom(this.el.offsetWidth, this.el.offsetWidth + wto);
		}
	},

	customSize: function(hto, wto) {
		if (this.h && this.w) {
			this.h.custom(this.el.offsetHeight, hto);
			this.w.custom(this.el.offsetWidth, wto);
		}
	}
}

fx.MultiFadeSize = Class.create();
fx.MultiFadeSize.prototype = {
	setOptions: function(options) {
		this.options = {
			delay: 100,
			opacity: false
		}
		Object.extend(this.options, options || {});
	},

	initialize: function(elements, options) {
		this.elements = elements;
		this.setOptions(options);
		var options = options || '';
		this.fxa = [];
		if (options && options.onComplete) options.onFinish = options.onComplete;
		elements.each(function(el, i){
			options.onComplete = function(){
				if (el.offsetHeight > 0) el.style.height = '1%';
				if (options.onFinish) options.onFinish(el);
			}
			this.fxa[i] = new fx.Combo(el, options);
			this.fxa[i].hide();
		}.bind(this));

	},

	showThisHideOpen: function(toShow){
		this.elements.each(function(el, j){
			if (el.offsetHeight > 0 && el != toShow) this.clearAndToggle(el, j);
			if (el == toShow && toShow.offsetHeight == 0) setTimeout(function(){this.clearAndToggle(toShow, j);}.bind(this), this.options.delay);
		}.bind(this));
	},

	clearAndToggle: function(el, i){
		this.fxa[i].clearTimer();
		this.fxa[i].toggle();
	},

	showAll: function() {
            for (i=0;i<this.elements.length;i++){
                if (this.elements[i].offsetHeight == 0) {
                    this.clearAndToggle(this.elements[i], i)
                }
            }
            showAll=1;
        },

	hideAll: function() {
            for (i=0;i<this.elements.length;i++){
                if (this.elements[i].offsetHeight > 0) {
                    this.clearAndToggle(this.elements[i], i)
                }
            }
            showAll=1;
        },

	toggle: function(el){
            el = $(el);
            for (i=0;i<this.elements.length;i++){
                if (this.elements[i] == el) {
                    this.clearAndToggle(this.elements[i], i)          
                }
	    }
        },      

        hide: function(el){
            el = $(el);
            for (i=0;i<this.elements.length;i++){
                if (this.elements[i] == el) {
                    this.fxa[i].hide();
                }
	    }
        }

}

fx.Accordion = Class.create();
fx.Accordion.prototype = {
	setOptions: function(options) {
		this.options = {
			delay: 100,
			opacity: false
		}
		Object.extend(this.options, options || {});
	},

	initialize: function(togglers, elements, options) {
		this.elements = elements;
		this.setOptions(options);
		var options = options || '';
		this.fxa = [];
		if (options && options.onComplete) options.onFinish = options.onComplete;
		elements.each(function(el, i){
			options.onComplete = function(){
				if (el.offsetHeight > 0) el.style.height = '1%';
				if (options.onFinish) options.onFinish(el);
			}
			this.fxa[i] = new fx.Combo(el, options);
			this.fxa[i].hide();
		}.bind(this));

		togglers.each(function(tog, i){
			if (typeof tog.onclick == 'function') var exClick = tog.onclick;
			tog.onclick = function(){
				if (exClick) exClick();
				this.showThisHideOpen(elements[i]);
			}.bind(this);
		}.bind(this));
	},

	showThisHideOpen: function(toShow){
		this.elements.each(function(el, j){
			if (el.offsetHeight > 0 && el != toShow) this.clearAndToggle(el, j);
			if (el == toShow && toShow.offsetHeight == 0) setTimeout(function(){this.clearAndToggle(toShow, j);}.bind(this), this.options.delay);
		}.bind(this));
	},

	clearAndToggle: function(el, i){
		this.fxa[i].clearTimer();
		this.fxa[i].toggle();
	}

}

var Remember = new Object();
Remember = function(){};
Remember.prototype = {
	initialize: function(el, options){
		this.el = $(el);
		this.days = 365;
		this.options = options;
		this.effect();
		var cookie = this.readCookie();
		if (cookie) {
			this.fx.now = cookie;
			this.fx.increase();
		}
	},

	//cookie functions based on code by Peter-Paul Koch
	setCookie: function(value) {
		var date = new Date();
		date.setTime(date.getTime()+(this.days*24*60*60*1000));
		var expires = "; expires="+date.toGMTString();
		document.cookie = this.el+this.el.id+this.prefix+"="+value+expires+"; path=/";
	},

	readCookie: function() {
		var nameEQ = this.el+this.el.id+this.prefix + "=";
		var ca = document.cookie.split(';');
		for(var i=0;c=ca[i];i++) {
			while (c.charAt(0)==' ') c = c.substring(1,c.length);
			if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
		}
		return false;
	},

	custom: function(from, to){
		if (this.fx.now != to) {
			this.setCookie(to);
			this.fx.custom(from, to);
		}
	}
}

fx.RememberHeight = Class.create();
fx.RememberHeight.prototype = Object.extend(new Remember(), {
	effect: function(){
		this.fx = new fx.Height(this.el, this.options);
		this.prefix = 'height';
	},
	
	toggle: function(){
		if (this.el.offsetHeight == 0) this.setCookie(this.el.scrollHeight);
		else this.setCookie(0);
		this.fx.toggle();
	},
	
	resize: function(to){
		this.setCookie(this.el.offsetHeight+to);
		this.fx.custom(this.el.offsetHeight,this.el.offsetHeight+to);
	},

	hide: function(){
		if (!this.readCookie()) {
			this.fx.hide();
		}
	}
});

fx.RememberText = Class.create();
fx.RememberText.prototype = Object.extend(new Remember(), {
	effect: function(){
		this.fx = new fx.Text(this.el, this.options);
		this.prefix = 'text';
	}
});

//useful for-replacement
Array.prototype.iterate = function(func){
	for(var i=0;i<this.length;i++) func(this[i], i);
}
if (!Array.prototype.each) Array.prototype.each = Array.prototype.iterate;

//Easing Equations (c) 2003 Robert Penner, all rights reserved.
//This work is subject to the terms in http://www.robertpenner.com/easing_terms_of_use.html.

//expo
fx.expoIn = function(pos){
	return Math.pow(2, 10 * (pos - 1));
}
fx.expoOut = function(pos){
	return (-Math.pow(2, -10 * pos) + 1);
}

//quad
fx.quadIn = function(pos){
	return Math.pow(pos, 2);
}
fx.quadOut = function(pos){
	return -(pos)*(pos-2);
}

//circ
fx.circOut = function(pos){
	return Math.sqrt(1 - Math.pow(pos-1,2));
}
fx.circIn = function(pos){
	return -(Math.sqrt(1 - Math.pow(pos, 2)) - 1);
}

//back
fx.backIn = function(pos){
	return (pos)*pos*((2.7)*pos - 1.7);
}
fx.backOut = function(pos){
	return ((pos-1)*(pos-1)*((2.7)*(pos-1) + 1.7) + 1);
}

//sine
fx.sineOut = function(pos){
	return Math.sin(pos * (Math.PI/2));
}
fx.sineIn = function(pos){
	return -Math.cos(pos * (Math.PI/2)) + 1;
}
fx.sineInOut = function(pos){
	return -(Math.cos(Math.PI*pos) - 1)/2;
}