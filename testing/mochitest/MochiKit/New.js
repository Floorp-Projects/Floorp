
MochiKit.Base.update(MochiKit.DOM, {
    /** @id MochiKit.DOM.makeClipping */
    makeClipping: function (element) {
        element = MochiKit.DOM.getElement(element);
        var oldOverflow = element.style.overflow;
        if ((MochiKit.Style.getStyle(element, 'overflow') || 'visible') != 'hidden') {
            element.style.overflow = 'hidden';
        }
        return oldOverflow;
    },

    /** @id MochiKit.DOM.undoClipping */
    undoClipping: function (element, overflow) {
        element = MochiKit.DOM.getElement(element);
        if (!overflow) {
            return;
        }
        element.style.overflow = overflow;
    },

    /** @id MochiKit.DOM.makePositioned */
    makePositioned: function (element) {
        element = MochiKit.DOM.getElement(element);
        var pos = MochiKit.Style.getStyle(element, 'position');
        if (pos == 'static' || !pos) {
            element.style.position = 'relative';
            // Opera returns the offset relative to the positioning context,
            // when an element is position relative but top and left have
            // not been defined
            if (/Opera/.test(navigator.userAgent)) {
                element.style.top = 0;
                element.style.left = 0;
            }
        }
    },

    /** @id MochiKit.DOM.undoPositioned */
    undoPositioned: function (element) {
        element = MochiKit.DOM.getElement(element);
        if (element.style.position == 'relative') {
            element.style.position = element.style.top = element.style.left = element.style.bottom = element.style.right = '';
        }
    },

    /** @id MochiKit.DOM.getFirstElementByTagAndClassName */
    getFirstElementByTagAndClassName: function (tagName, className,
            /* optional */parent) {
        var self = MochiKit.DOM;
        if (typeof(tagName) == 'undefined' || tagName === null) {
            tagName = '*';
        }
        if (typeof(parent) == 'undefined' || parent === null) {
            parent = self._document;
        }
        parent = self.getElement(parent);
        var children = (parent.getElementsByTagName(tagName)
            || self._document.all);
        if (typeof(className) == 'undefined' || className === null) {
            return children[0];
        }

        for (var i = 0; i < children.length; i++) {
            var child = children[i];
            var classNames = child.className.split(' ');
            for (var j = 0; j < classNames.length; j++) {
                if (classNames[j] == className) {
                    return child;
                }
            }
        }
    },

    /** @id MochiKit.DOM.isParent */
    isParent: function (child, element) {
        if (!child.parentNode || child == element) {
            return false;
        }

        if (child.parentNode == element) {
            return true;
        }

        return MochiKit.DOM.isParent(child.parentNode, element);
    }
});

MochiKit.Position = {
    // set to true if needed, warning: firefox performance problems
    // NOT neeeded for page scrolling, only if draggable contained in
    // scrollable elements
    includeScrollOffsets: false,

    /** @id MochiKit.Position.prepare */
    prepare: function () {
        var deltaX =  window.pageXOffset
                   || document.documentElement.scrollLeft
                   || document.body.scrollLeft
                   || 0;
        var deltaY =  window.pageYOffset
                   || document.documentElement.scrollTop
                   || document.body.scrollTop
                   || 0;
        this.windowOffset = new MochiKit.Style.Coordinates(deltaX, deltaY);
    },

    /** @id MochiKit.Position.cumulativeOffset */
    cumulativeOffset: function (element) {
        var valueT = 0;
        var valueL = 0;
        do {
            valueT += element.offsetTop  || 0;
            valueL += element.offsetLeft || 0;
            element = element.offsetParent;
        } while (element);
        return new MochiKit.Style.Coordinates(valueL, valueT);
    },

    /** @id MochiKit.Position.realOffset */
    realOffset: function (element) {
        var valueT = 0;
        var valueL = 0;
        do {
            valueT += element.scrollTop  || 0;
            valueL += element.scrollLeft || 0;
            element = element.parentNode;
        } while (element);
        return new MochiKit.Style.Coordinates(valueL, valueT);
    },

    /** @id MochiKit.Position.within */
    within: function (element, x, y) {
        if (this.includeScrollOffsets) {
            return this.withinIncludingScrolloffsets(element, x, y);
        }
        this.xcomp = x;
        this.ycomp = y;
        this.offset = this.cumulativeOffset(element);
        if (element.style.position == "fixed") {
            this.offset.x += this.windowOffset.x;
            this.offset.y += this.windowOffset.y;
        }

        return (y >= this.offset.y &&
                y <  this.offset.y + element.offsetHeight &&
                x >= this.offset.x &&
                x <  this.offset.x + element.offsetWidth);
    },

    /** @id MochiKit.Position.withinIncludingScrolloffsets */
    withinIncludingScrolloffsets: function (element, x, y) {
        var offsetcache = this.realOffset(element);

        this.xcomp = x + offsetcache.x - this.windowOffset.x;
        this.ycomp = y + offsetcache.y - this.windowOffset.y;
        this.offset = this.cumulativeOffset(element);

        return (this.ycomp >= this.offset.y &&
                this.ycomp <  this.offset.y + element.offsetHeight &&
                this.xcomp >= this.offset.x &&
                this.xcomp <  this.offset.x + element.offsetWidth);
    },

    // within must be called directly before
    /** @id MochiKit.Position.overlap */
    overlap: function (mode, element) {
        if (!mode) {
            return 0;
        }
        if (mode == 'vertical') {
          return ((this.offset.y + element.offsetHeight) - this.ycomp) /
                 element.offsetHeight;
        }
        if (mode == 'horizontal') {
          return ((this.offset.x + element.offsetWidth) - this.xcomp) /
                 element.offsetWidth;
        }
    },

    /** @id MochiKit.Position.absolutize */
    absolutize: function (element) {
        element = MochiKit.DOM.getElement(element);
        if (element.style.position == 'absolute') {
            return;
        }
        MochiKit.Position.prepare();

        var offsets = MochiKit.Position.positionedOffset(element);
        var width = element.clientWidth;
        var height = element.clientHeight;

        var oldStyle = {
            'position': element.style.position,
            'left': offsets.x - parseFloat(element.style.left  || 0),
            'top': offsets.y - parseFloat(element.style.top || 0),
            'width': element.style.width,
            'height': element.style.height
        };

        element.style.position = 'absolute';
        element.style.top = offsets.y + 'px';
        element.style.left = offsets.x + 'px';
        element.style.width = width + 'px';
        element.style.height = height + 'px';

        return oldStyle;
    },

    /** @id MochiKit.Position.positionedOffset */
    positionedOffset: function (element) {
        var valueT = 0, valueL = 0;
        do {
            valueT += element.offsetTop  || 0;
            valueL += element.offsetLeft || 0;
            element = element.offsetParent;
            if (element) {
                p = MochiKit.Style.getStyle(element, 'position');
                if (p == 'relative' || p == 'absolute') {
                    break;
                }
            }
        } while (element);
        return new MochiKit.Style.Coordinates(valueL, valueT);
    },

    /** @id MochiKit.Position.relativize */
    relativize: function (element, oldPos) {
        element = MochiKit.DOM.getElement(element);
        if (element.style.position == 'relative') {
            return;
        }
        MochiKit.Position.prepare();

        var top = parseFloat(element.style.top || 0) -
                  (oldPos['top'] || 0);
        var left = parseFloat(element.style.left || 0) -
                   (oldPos['left'] || 0);

        element.style.position = oldPos['position'];
        element.style.top = top + 'px';
        element.style.left = left + 'px';
        element.style.width = oldPos['width'];
        element.style.height = oldPos['height'];
    },

    /** @id MochiKit.Position.clone */
    clone: function (source, target) {
        source = MochiKit.DOM.getElement(source);
        target = MochiKit.DOM.getElement(target);
        target.style.position = 'absolute';
        var offsets = this.cumulativeOffset(source);
        target.style.top = offsets.y + 'px';
        target.style.left = offsets.x + 'px';
        target.style.width = source.offsetWidth + 'px';
        target.style.height = source.offsetHeight + 'px';
    },

    /** @id MochiKit.Position.page */
    page: function (forElement) {
        var valueT = 0;
        var valueL = 0;

        var element = forElement;
        do {
            valueT += element.offsetTop  || 0;
            valueL += element.offsetLeft || 0;

            // Safari fix
            if (element.offsetParent == document.body && MochiKit.Style.getStyle(element, 'position') == 'absolute') {
                break;
            }
        } while (element = element.offsetParent);

        element = forElement;
        do {
            valueT -= element.scrollTop  || 0;
            valueL -= element.scrollLeft || 0;
        } while (element = element.parentNode);

        return new MochiKit.Style.Coordinates(valueL, valueT);
    }
};

