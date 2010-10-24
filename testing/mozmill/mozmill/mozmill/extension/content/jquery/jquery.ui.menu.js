/*
 * jQuery UI Menu @VERSION
 *
 * Copyright (c) 2009 AUTHORS.txt (http://jqueryui.com/about)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * http://docs.jquery.com/UI/Menu
 *
 * Depends:
 *	jquery.ui.core.js
 */
(function($) {

$.widget("ui.menu", {
	_init: function() {
		var self = this;
		this.element.attr("role", "menu")
		.attr("aria-activedescendant", "ui-active-menuitem")
		.addClass("ui-menu ui-widget ui-widget-content ui-corner-bottom")
		.click(function(e) {
			// temporary
			e.preventDefault();
			self.select();
		});
		var items = this.element.children("li");
		items.addClass("ui-menu-item").attr("role", "menuitem")
		.children("a").attr("tabindex", "-1").addClass("ui-corner-all")
		// mouseenter doesn't work with event delegation
		.mouseenter(function() {
			self.activate($(this).parent());
		});
	},
	
	activate: function(item) {
		this.deactivate();
		this.active = item.children("a").addClass("ui-state-hover").attr("id", "ui-active-menuitem").end();
		this._trigger("focus", null, { item: item });
		if (this.hasScroll()) {
			var offset = item.offset().top - this.element.offset().top,
				scroll = this.element.attr("scrollTop"),
				elementHeight = this.element.height();
			if (offset < 0) {
				this.element.attr("scrollTop", scroll + offset);
			} else if (offset > elementHeight) {
				this.element.attr("scrollTop", scroll + offset - elementHeight + item.height());
			}
		}
	},
	
	deactivate: function() {
		if (!this.active)
			return;
		this.active.children("a").removeClass("ui-state-hover").removeAttr("id", "ui-active-menuitem");
		this.active = null;
	},
	
	next: function() {
		this.move("next", "li:first");
	},
	
	previous: function() {
		this.move("prev", "li:last");
	},
	
	first: function() {
		return this.active && !this.active.prev().length;
	},
	
	last: function() {
		return this.active && !this.active.next().length;
	},
	
	move: function(direction, edge) {
		if (!this.active) {
			this.activate(this.element.children(edge));
			return;
		}
		var next = this.active[direction]();
		if (next.length) {
			this.activate(next);
		} else {
			this.activate(this.element.children(edge));
		}
	},
	
	// TODO merge with previousPage
	nextPage: function() {
		if (this.hasScroll()) {
			// TODO merge with no-scroll-else
			if (!this.active || this.last()) {
				this.activate(this.element.children(":first"));
				return;
			}
			// last item on page, then scroll one page down, otherwise select last item on page
			if (this.active && this.active.offset().top - this.element.offset().top + this.active.height() > this.element.height()) {
				// last
				var offsetBase = this.element.offset().top,
					height = this.element.height();
				var result = this.element.children("li").filter(function() {
					var close = $(this).offset().top - offsetBase - height * 1.5 - $(this).height() / 2;
					// TODO improve approximation
					return close < 10 && close > -10;
				})
				// TODO try to catch this earlier when scrollTop indicates the last page anyway
				if (!result.length)
					result = this.element.children(":last")
				this.activate(result);
			} else {
				// not last
				var offsetBase = this.element.offset().top,
					height = this.element.height();
				var result = this.element.children("li").filter(function() {
					var close = $(this).offset().top - offsetBase + $(this).height() - height;
					// TODO improve approximation
					return close < 10 && close > -10;
				})
				this.activate(result);
			}
		} else {
			this.activate(this.element.children(!this.active || this.last() ? ":first" : ":last"));
		}
	},
	
	// TODO merge with nextPage
	previousPage: function() {
		if (this.hasScroll()) {
			// TODO merge with no-scroll-else
			if (!this.active || this.first()) {
				this.activate(this.element.children(":last"));
				return;
			}
			// first item on page, then scroll one page up, otherwise select first item on page
			if (this.active && this.active.offset().top - this.element.offset().top <= 1) {
				// first
				var offsetBase = this.element.offset().top,
					height = this.element.height();
				var result = this.element.children("li").filter(function() {
					var close = $(this).offset().top - offsetBase + height / 2 + $(this).height() / 2;
					// TODO improve approximation
					return close < 10 && close > -10;
				})
				// TODO try to catch this earlier when scrollTop indicates the first page anyway
				if (!result.length)
					result = this.element.children(":first")
				this.activate(result);
			} else {
				// not first
				var offsetBase = this.element.offset().top,
					height = this.element.height();
				var result = this.element.children("li").filter(function() {
					var close = $(this).offset().top - offsetBase;
					// TODO improve approximation
					return close < 10 && close > -10;
				})
				this.activate(result);
			}
		} else {
			this.activate(this.element.children(!this.active || this.first() ? ":last" : ":first"));
		}
	},
	
	hasScroll: function() {
		return this.element.height() < this.element.attr("scrollHeight");
	},
	
	select: function() {
		this._trigger("selected", null, { item: this.active });
	}
	
});

})(jQuery);
