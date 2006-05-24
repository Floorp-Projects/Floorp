/*

On page load, the SortableManager:

- Rips out all of the elements with the mochi-example class.
- Finds the elements with the mochi-template class and saves them for
  later parsing with "MochiTAL".
- Finds the anchor tags with the mochi:dataformat attribute and gives them
  onclick behvaiors to load new data, using their href as the data source.
  This makes your XML or JSON look like a normal link to a search engine
  (or javascript-disabled browser).
- Clones the thead element from the table because it will be replaced on each
  sort.
- Sets up a default sort key of "domain_name" and queues a load of the json
  document.


On data load, the SortableManager:

- Parses the table data from the document (columns list, rows list-of-lists)
  and turns them into a list of [{column:value, ...}] objects for easy sorting
  and column order stability.
- Chooses the default (or previous) sort state and triggers a sort request


On sort request:

- Replaces the cloned thead element with a copy of it that has the sort
  indicator (&uarr; or &darr;) for the most recently sorted column (matched up
  to the first field in the th's mochi:sortcolumn attribute), and attaches
  onclick, onmousedown, onmouseover, onmouseout behaviors to them. The second
  field of mochi:sortcolumn attribute is used to perform a non-string sort.
- Performs the sort on the domains list.  If the second field of
  mochi:sortcolumn was not "str", then a custom function is used and the
  results are stored away in a __sort__ key, which is then used to perform the
  sort (read: shwartzian transform).
- Calls processMochiTAL on the page, which finds the mochi-template sections 
  and then looks for mochi:repeat and mochi:content attributes on them, using
  the data object.

*/

processMochiTAL = function (dom, data) {
    /***

        A TAL-esque template attribute language processor,
        including content replacement and repeat

    ***/

    // nodeType == 1 is an element, we're leaving
    // text nodes alone.
    if (dom.nodeType != 1) {
        return;
    }
    var attr;
    // duplicate this element for each item in the
    // given list, and then process the duplicated
    // element again (sans mochi:repeat tag)
    attr = getAttribute(dom, "mochi:repeat");
    if (attr) {
        dom.removeAttribute("mochi:repeat");
        var parent = dom.parentNode;
        attr = attr.split(" ");
        var name = attr[0];
        var lst = valueForKeyPath(data, attr[1]);
        if (!lst) {
            return;
        }
        for (var i = 0; i < lst.length; i++) {
            data[name] = lst[i];
            var newDOM = dom.cloneNode(true);
            processMochiTAL(newDOM, data);
            parent.insertBefore(newDOM, dom);
        }
        parent.removeChild(dom);
        return;
    }
    // do content replacement if there's a mochi:content attribute
    // on the element
    attr = getAttribute(dom, "mochi:content");
    if (attr) {
        dom.removeAttribute("mochi:content");
        replaceChildNodes(dom, valueForKeyPath(data, attr));
        return;
    }
    // we make a shallow copy of the current list of child nodes
    // because it *will* change if there's a mochi:repeat in there!
    var nodes = list(dom.childNodes);
    for (var i = 0; i < nodes.length; i++) {
        processMochiTAL(nodes[i], data);
    }
};

mouseOverFunc = function () {
    addElementClass(this, "over");
};

mouseOutFunc = function () {
    removeElementClass(this, "over");
};

ignoreEvent = function (ev) {
    if (ev && ev.preventDefault) {
        ev.preventDefault();
        ev.stopPropagation();
    } else if (typeof(event) != 'undefined') {
        event.cancelBubble = false;
        event.returnValue = false;
    }
};

SortTransforms = {
    "str": operator.identity,
    "istr": function (s) { return s.toLowerCase(); },
    "isoDate": isoDate
};

getAttribute = function (dom, key) {
    try {
        return dom.getAttribute(key);
    } catch (e) {
        return null;
    }
};

datatableFromXMLRequest = function (req) {
    /***

        This effectively converts domains.xml to the
        same form as domains.json

    ***/
    var xml = req.responseXML;
    var nodes = xml.getElementsByTagName("column");
    var rval = {"columns": map(scrapeText, nodes)};
    var rows = [];
    nodes = xml.getElementsByTagName("row") 
    for (var i = 0; i < nodes.length; i++) {
        var cells = nodes[i].getElementsByTagName("cell");
        rows.push(map(scrapeText, cells));
    }
    rval.rows = rows;
    return rval;
};

loadFromDataAnchor = function (ev) {
    ignoreEvent(ev);
    var format = this.getAttribute("mochi:dataformat");
    var href = this.href;
    sortableManager.loadFromURL(format, href);
};

valueForKeyPath = function (data, keyPath) {
    var chunks = keyPath.split(".");
    while (chunks.length && data) {
        data = data[chunks.shift()];
    }
    return data;
};


SortableManager = function () {
    this.thead = null;
    this.thead_proto = null;
    this.tbody = null;
    this.deferred = null;
    this.columns = [];
    this.rows = [];
    this.templates = [];
    this.sortState = {};
    bindMethods(this);
};

SortableManager.prototype = {

    "initialize": function () {
        // just rip all mochi-examples out of the DOM
        var examples = getElementsByTagAndClassName(null, "mochi-example");
        while (examples.length) {
            swapDOM(examples.pop(), null);
        }
        // make a template list
        var templates = getElementsByTagAndClassName(null, "mochi-template");
        for (var i = 0; i < templates.length; i++) {
            var template = templates[i];
            var proto = template.cloneNode(true);
            removeElementClass(proto, "mochi-template");
            this.templates.push({
                "template": proto,
                "node": template
            });
        }
        // set up the data anchors to do loads
        var anchors = getElementsByTagAndClassName("a", null);
        for (var i = 0; i < anchors.length; i++) {
            var node = anchors[i];
            var format = getAttribute(node, "mochi:dataformat");
            if (format) {
                node.onclick = loadFromDataAnchor;
            }
        }

        // to find sort columns
        this.thead = getElementsByTagAndClassName("thead", null)[0];
        this.thead_proto = this.thead.cloneNode(true);

        this.sortkey = "domain_name";
        this.loadFromURL("json", "domains.json");
    },

    "loadFromURL": function (format, url) {
        log('loadFromURL', format, url);
        var d;
        if (this.deferred) {
            this.deferred.cancel();
        }
        if (format == "xml") {
            var req = getXMLHttpRequest();
            if (req.overrideMimeType) {
                req.overrideMimeType("text/xml");
            }
            req.open("GET", url, true);
            d = sendXMLHttpRequest(req).addCallback(datatableFromXMLRequest);
        } else if (format == "json") {
            d = loadJSONDoc(url);
        } else {
            throw new TypeError("format " + repr(format) + " not supported");
        }
        // keep track of the current deferred, so that we can cancel it
        this.deferred = d;
        var self = this;
        // on success or error, remove the current deferred because it has
        // completed, and pass through the result or error
        d.addBoth(function (res) {
            self.deferred = null; 
            log('loadFromURL success');
            return res;
        });
        // on success, tag the result with the format used so we can display
        // it
        d.addCallback(function (res) {
            res.format = format;
            return res;
        });
        // call this.initWithData(data) once it's ready
        d.addCallback(this.initWithData);
        // if anything goes wrong, except for a simple cancellation,
        // then log the error and show the logger
        d.addErrback(function (err) {
            if (err instanceof CancelledError) {
                return;
            }
            logError(err);
            logger.debuggingBookmarklet();
        });
        return d;
    },

    "initWithData": function (data) {
        /***

            Initialize the SortableManager with a table object
        
        ***/

        // reformat to [{column:value, ...}, ...] style as the domains key
        var domains = [];
        var rows = data.rows;
        var cols = data.columns;
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i];
            var domain = {};
            for (var j = 0; j < cols.length; j++) {
                domain[cols[j]] = row[j];
            }
            domains.push(domain);
        }
        data.domains = domains;
        this.data = data;
        // perform a sort and display based upon the previous sort state,
        // defaulting to an ascending sort if this is the first sort
        var order = this.sortState[this.sortkey];
        if (typeof(order) == 'undefined') {
            order = true;
        }
        this.drawSortedRows(this.sortkey, order, false);

    },

    "onSortClick": function (name) {
        /***

            Return a sort function for click events

        ***/
        // save ourselves from doing a bind
        var self = this;
        // on click, flip the last sort order of that column and sort
        return function () {
            log('onSortClick', name);
            var order = self.sortState[name];
            if (typeof(order) == 'undefined') {
                // if it's never been sorted by this column, sort ascending
                order = true;
            } else if (self.sortkey == name) {
                // if this column was sorted most recently, flip the sort order
                order = !((typeof(order) == 'undefined') ? false : order);
            }
            self.drawSortedRows(name, order, true);
        };
    },

    "drawSortedRows": function (key, forward, clicked) {
        /***

            Draw the new sorted table body, and modify the column headers
            if appropriate

        ***/
        log('drawSortedRows', key, forward);

        // save it so we can flip next time
        this.sortState[key] = forward;
        this.sortkey = key;
        var sortstyle;

        // setup the sort columns   
        var thead = this.thead_proto.cloneNode(true);
        var cols = thead.getElementsByTagName("th");
        for (var i = 0; i < cols.length; i++) {
            var col = cols[i];
            var sortinfo = getAttribute(col, "mochi:sortcolumn").split(" ");
            var sortkey = sortinfo[0];
            col.onclick = this.onSortClick(sortkey);
            col.onmousedown = ignoreEvent;
            col.onmouseover = mouseOverFunc;
            col.onmouseout = mouseOutFunc;
            // if this is the sorted column
            if (sortkey == key) {
                sortstyle = sortinfo[1];
                // \u2193 is down arrow, \u2191 is up arrow
                // forward sorts mean the rows get bigger going down
                var arrow = (forward ? "\u2193" : "\u2191");
                // add the character to the column header
                col.appendChild(SPAN(null, arrow));
                if (clicked) {
                    col.onmouseover();
                }
            }
        }
        this.thead = swapDOM(this.thead, thead);

        // apply a sort transform to a temporary column named __sort__,
        // and do the sort based on that column
        if (!sortstyle) {
            sortstyle = "str";
        }
        var sortfunc = SortTransforms[sortstyle];
        if (!sortfunc) {
            throw new TypeError("unsupported sort style " + repr(sortstyle));
        }
        var domains = this.data.domains;
        for (var i = 0; i < domains.length; i++) {
            var domain = domains[i];
            domain.__sort__ = sortfunc(domain[key]);
        }

        // perform the sort based on the state given (forward or reverse)
        var cmp = (forward ? keyComparator : reverseKeyComparator);
        domains.sort(cmp("__sort__"));

        // process every template with the given data
        // and put the processed templates in the DOM
        for (var i = 0; i < this.templates.length; i++) {
            log('template', i, template);
            var template = this.templates[i];
            var dom = template.template.cloneNode(true);
            processMochiTAL(dom, this.data);
            template.node = swapDOM(template.node, dom);
        }
 

    }

};

// create the global SortableManager and initialize it on page load
sortableManager = new SortableManager();
addLoadEvent(sortableManager.initialize);

// rewrite the view-source links
addLoadEvent(function () {
    var elems = getElementsByTagAndClassName("A", "view-source");
    var page = "ajax_tables/";
    for (var i = 0; i < elems.length; i++) {
        var elem = elems[i];
        var href = elem.href.split(/\//).pop();
        elem.target = "_blank";
        elem.href = "../view-source/view-source.html#" + page + href;
    }
});
