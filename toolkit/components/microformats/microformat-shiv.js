/*
   Modern
   microformat-shiv - v1.4.0
   Built: 2016-03-02 10:03 - http://microformat-shiv.com
   Copyright (c) 2016 Glenn Jones
   Licensed MIT
*/


var Microformats; // jshint ignore:line

(function (root, factory) {
    if (typeof define === 'function' && define.amd) {
        define([], factory);
    } else if (typeof exports === 'object') {
        module.exports = factory();
    } else {
        root.Microformats = factory();
  }
}(this, function () {

    var modules = {};


    modules.version = '1.4.0';
    modules.livingStandard = '2015-09-25T12:26:04Z';

    /**
     * constructor
     *
     */
    modules.Parser = function () {
        this.rootPrefix = 'h-';
        this.propertyPrefixes = ['p-', 'dt-', 'u-', 'e-'];
        this.excludeTags = ['br', 'hr'];
    };


    // create objects incase the v1 map modules don't load
    modules.maps = (modules.maps)? modules.maps : {};
    modules.rels = (modules.rels)? modules.rels : {};


    modules.Parser.prototype = {

        init: function() {
            this.rootNode = null;
            this.document = null;
            this.options = {
                'baseUrl': '',
                'filters': [],
                'textFormat': 'whitespacetrimmed',
                'dateFormat': 'auto', // html5 for testing
                'overlappingVersions': false,
                'impliedPropertiesByVersion': true,
                'parseLatLonGeo': false
            };
            this.rootID = 0;
            this.errors = [];
            this.noContentErr = 'No options.node or options.html was provided and no document object could be found.';
        },


        /**
         * internal parse function
         *
         * @param  {Object} options
         * @return {Object}
         */
        get: function(options) {
            var out = this.formatEmpty(),
                data = [],
                rels;

            this.init();
            options = (options)? options : {};
            this.mergeOptions(options);
            this.getDOMContext( options );

            // if we do not have any context create error
            if (!this.rootNode || !this.document) {
                this.errors.push(this.noContentErr);
            } else {

                // only parse h-* microformats if we need to
                // this is added to speed up parsing
                if (this.hasMicroformats(this.rootNode, options)) {
                    this.prepareDOM( options );

                    if (this.options.filters.length > 0) {
                        // parse flat list of items
                        var newRootNode = this.findFilterNodes(this.rootNode, this.options.filters);
                        data = this.walkRoot(newRootNode);
                    } else {
                        // parse whole document from root
                        data = this.walkRoot(this.rootNode);
                    }

                    out.items = data;
                    // don't clear-up DOM if it was cloned
                    if (modules.domUtils.canCloneDocument(this.document) === false) {
                        this.clearUpDom(this.rootNode);
                    }
                }

                // find any rels
                if (this.findRels) {
                    rels = this.findRels(this.rootNode);
                    out.rels = rels.rels;
                    out['rel-urls'] = rels['rel-urls'];
                }

            }

            if (this.errors.length > 0) {
                return this.formatError();
            }
            return out;
        },


        /**
         * parse to get parent microformat of passed node
         *
         * @param  {DOM Node} node
         * @param  {Object} options
         * @return {Object}
         */
        getParent: function(node, options) {
            this.init();
            options = (options)? options : {};

            if (node) {
                return this.getParentTreeWalk(node, options);
            }
            this.errors.push(this.noContentErr);
            return this.formatError();
        },


        /**
         * get the count of microformats
         *
         * @param  {DOM Node} rootNode
         * @return {Int}
         */
        count: function( options ) {
            var out = {},
                items,
                classItems,
                x,
                i;

            this.init();
            options = (options)? options : {};
            this.getDOMContext( options );

            // if we do not have any context create error
            if (!this.rootNode || !this.document) {
                return {'errors': [this.noContentErr]};
            }
            items = this.findRootNodes( this.rootNode, true );
            i = items.length;
            while (i--) {
                classItems = modules.domUtils.getAttributeList(items[i], 'class');
                x = classItems.length;
                while (x--) {
                    // find v2 names
                    if (modules.utils.startWith( classItems[x], 'h-' )) {
                        this.appendCount(classItems[x], 1, out);
                    }
                    // find v1 names
                    for (var key in modules.maps) {
                        // dont double count if v1 and v2 roots are present
                        if (modules.maps[key].root === classItems[x] && classItems.indexOf(key) === -1) {
                            this.appendCount(key, 1, out);
                        }
                    }
                }
            }
            var relCount = this.countRels( this.rootNode );
            if (relCount > 0) {
                out.rels = relCount;
            }

            return out;
        },


        /**
         * does a node have a class that marks it as a microformats root
         *
         * @param  {DOM Node} node
         * @param  {Objecte} options
         * @return {Boolean}
         */
        isMicroformat: function( node, options ) {
            var classes,
                i;

            if (!node) {
                return false;
            }

            // if documemt gets topmost node
            node = modules.domUtils.getTopMostNode( node );

            // look for h-* microformats
            classes = this.getUfClassNames(node);
            if (options && options.filters && modules.utils.isArray(options.filters)) {
                i = options.filters.length;
                while (i--) {
                    if (classes.root.indexOf(options.filters[i]) > -1) {
                        return true;
                    }
                }
                return false;
            }
            return (classes.root.length > 0);
        },


        /**
         * does a node or its children have microformats
         *
         * @param  {DOM Node} node
         * @param  {Objecte} options
         * @return {Boolean}
         */
        hasMicroformats: function( node, options ) {
            var items,
                i;

            if (!node) {
                return false;
            }

            // if browser based documemt get topmost node
            node = modules.domUtils.getTopMostNode( node );

            // returns all microformat roots
            items = this.findRootNodes( node, true );
            if (options && options.filters && modules.utils.isArray(options.filters)) {
                i = items.length;
                while (i--) {
                    if ( this.isMicroformat( items[i], options ) ) {
                        return true;
                    }
                }
                return false;
            }
            return (items.length > 0);
        },


        /**
         * add a new v1 mapping object to parser
         *
         * @param  {Array} maps
         */
        add: function( maps ) {
            maps.forEach(function(map) {
                if (map && map.root && map.name && map.properties) {
                modules.maps[map.name] = JSON.parse(JSON.stringify(map));
                }
            });
        },


        /**
         * internal parse to get parent microformats by walking up the tree
         *
         * @param  {DOM Node} node
         * @param  {Object} options
         * @param  {Int} recursive
         * @return {Object}
         */
        getParentTreeWalk: function (node, options, recursive) {
            options = (options)? options : {};

            // recursive calls
            if (recursive === undefined) {
                if (node.parentNode && node.nodeName !== 'HTML') {
                    return this.getParentTreeWalk(node.parentNode, options, true);
                }
                return this.formatEmpty();
            }
            if (node !== null && node !== undefined && node.parentNode) {
                if (this.isMicroformat( node, options )) {
                    // if we have a match return microformat
                    options.node = node;
                    return this.get( options );
                }
                return this.getParentTreeWalk(node.parentNode, options, true);
            }
            return this.formatEmpty();
        },



        /**
         * configures what are the base DOM objects for parsing
         *
         * @param  {Object} options
         */
        getDOMContext: function( options ) {
            var nodes = modules.domUtils.getDOMContext( options );
            this.rootNode = nodes.rootNode;
            this.document = nodes.document;
        },


        /**
         * prepares DOM before the parse begins
         *
         * @param  {Object} options
         * @return {Boolean}
         */
        prepareDOM: function( options ) {
            var baseTag,
                href;

            // use current document to define baseUrl, try/catch needed for IE10+ error
            try {
                if (!options.baseUrl && this.document && this.document.location) {
                    this.options.baseUrl = this.document.location.href;
                }
            } catch (e) {
                // there is no alt action
            }


            // find base tag to set baseUrl
            baseTag = modules.domUtils.querySelector(this.document,'base');
            if (baseTag) {
                href = modules.domUtils.getAttribute(baseTag, 'href');
                if (href) {
                    this.options.baseUrl = href;
                }
            }

            // get path to rootNode
            // then clone document
            // then reset the rootNode to its cloned version in a new document
            var path,
                newDocument,
                newRootNode;

            path = modules.domUtils.getNodePath(this.rootNode);
            newDocument = modules.domUtils.cloneDocument(this.document);
            newRootNode = modules.domUtils.getNodeByPath(newDocument, path);

            // check results as early IE fails
            if (newDocument && newRootNode) {
                this.document = newDocument;
                this.rootNode = newRootNode;
            }

            // add includes
            if (this.addIncludes) {
                this.addIncludes( this.document );
            }

            return (this.rootNode && this.document);
        },


        /**
         * returns an empty structure with errors
         *
         *   @return {Object}
         */
        formatError: function() {
            var out = this.formatEmpty();
            out.errors = this.errors;
            return out;
        },


        /**
         * returns an empty structure
         *
         *   @return {Object}
         */
        formatEmpty: function() {
            return {
                'items': [],
                'rels': {},
                'rel-urls': {}
            };
        },


        // find microformats of a given type and return node structures
        findFilterNodes: function(rootNode, filters) {
            if (modules.utils.isString(filters)) {
                filters = [filters];
            }
            var newRootNode = modules.domUtils.createNode('div'),
                items = this.findRootNodes(rootNode, true),
                i = 0,
                x = 0,
                y = 0;

            // add v1 names
            y = filters.length;
            while (y--) {
                if (this.getMapping(filters[y])) {
                    var v1Name = this.getMapping(filters[y]).root;
                    filters.push(v1Name);
                }
            }

            if (items) {
                i = items.length;
                while (x < i) {
                    // append matching nodes into newRootNode
                    y = filters.length;
                    while (y--) {
                        if (modules.domUtils.hasAttributeValue(items[x], 'class', filters[y])) {
                            var clone = modules.domUtils.clone(items[x]);
                            modules.domUtils.appendChild(newRootNode, clone);
                            break;
                        }
                    }
                    x++;
                }
            }

            return newRootNode;
        },


        /**
         * appends data to output object for count
         *
         * @param  {string} name
         * @param  {Int} count
         * @param  {Object}
         */
        appendCount: function(name, count, out) {
            if (out[name]) {
                out[name] = out[name] + count;
            } else {
                out[name] = count;
            }
        },


        /**
         * is the microformats type in the filter list
         *
         * @param  {Object} uf
         * @param  {Array} filters
         * @return {Boolean}
         */
        shouldInclude: function(uf, filters) {
            var i;

            if (modules.utils.isArray(filters) && filters.length > 0) {
                i = filters.length;
                while (i--) {
                    if (uf.type[0] === filters[i]) {
                        return true;
                    }
                }
                return false;
            }
            return true;
        },


        /**
         * finds all microformat roots in a rootNode
         *
         * @param  {DOM Node} rootNode
         * @param  {Boolean} includeRoot
         * @return {Array}
         */
        findRootNodes: function(rootNode, includeRoot) {
            var arr = null,
                out = [],
                classList = [],
                items,
                x,
                i,
                y,
                key;


            // build an array of v1 root names
            for (key in modules.maps) {
                if (modules.maps.hasOwnProperty(key)) {
                    classList.push(modules.maps[key].root);
                }
            }

            // get all elements that have a class attribute
            includeRoot = (includeRoot) ? includeRoot : false;
            if (includeRoot && rootNode.parentNode) {
                arr = modules.domUtils.getNodesByAttribute(rootNode.parentNode, 'class');
            } else {
                arr = modules.domUtils.getNodesByAttribute(rootNode, 'class');
            }

            // loop elements that have a class attribute
            x = 0;
            i = arr.length;
            while (x < i) {

                items = modules.domUtils.getAttributeList(arr[x], 'class');

                // loop classes on an element
                y = items.length;
                while (y--) {
                    // match v1 root names
                    if (classList.indexOf(items[y]) > -1) {
                        out.push(arr[x]);
                        break;
                    }

                    // match v2 root name prefix
                    if (modules.utils.startWith(items[y], 'h-')) {
                        out.push(arr[x]);
                        break;
                    }
                }

                x++;
            }
            return out;
        },


        /**
         * starts the tree walk to find microformats
         *
         * @param  {DOM Node} node
         * @return {Array}
         */
        walkRoot: function(node) {
            var context = this,
                children = [],
                child,
                classes,
                items = [],
                out = [];

            classes = this.getUfClassNames(node);
            // if it is a root microformat node
            if (classes && classes.root.length > 0) {
                items = this.walkTree(node);

                if (items.length > 0) {
                    out = out.concat(items);
                }
            } else {
                // check if there are children and one of the children has a root microformat
                children = modules.domUtils.getChildren( node );
                if (children && children.length > 0 && this.findRootNodes(node, true).length > -1) {
                    for (var i = 0; i < children.length; i++) {
                        child = children[i];
                        items = context.walkRoot(child);
                        if (items.length > 0) {
                            out = out.concat(items);
                        }
                    }
                }
            }
            return out;
        },


        /**
         * starts the tree walking for a single microformat
         *
         * @param  {DOM Node} node
         * @return {Array}
         */
        walkTree: function(node) {
            var classes,
                out = [],
                obj,
                itemRootID;

            // loop roots found on one element
            classes = this.getUfClassNames(node);
            if (classes && classes.root.length && classes.root.length > 0) {

                this.rootID++;
                itemRootID = this.rootID;
                obj = this.createUfObject(classes.root, classes.typeVersion);

                this.walkChildren(node, obj, classes.root, itemRootID, classes);
                if (this.impliedRules) {
                    this.impliedRules(node, obj, classes);
                }
                out.push( this.cleanUfObject(obj) );


            }
            return out;
        },


        /**
         * finds child properties of microformat
         *
         * @param  {DOM Node} node
         * @param  {Object} out
         * @param  {String} ufName
         * @param  {Int} rootID
         * @param  {Object} parentClasses
         */
        walkChildren: function(node, out, ufName, rootID, parentClasses) {
            var context = this,
                children = [],
                rootItem,
                itemRootID,
                value,
                propertyName,
                propertyVersion,
                i,
                x,
                y,
                z,
                child;

            children = modules.domUtils.getChildren( node );

            y = 0;
            z = children.length;
            while (y < z) {
                child = children[y];

                // get microformat classes for this single element
                var classes = context.getUfClassNames(child, ufName);

                // a property which is a microformat
                if (classes.root.length > 0 && classes.properties.length > 0 && !child.addedAsRoot) {
                    // create object with type, property and value
                    rootItem = context.createUfObject(
                        classes.root,
                        classes.typeVersion,
                        modules.text.parse(this.document, child, context.options.textFormat)
                    );

                    // add the microformat as an array of properties
                    propertyName = context.removePropPrefix(classes.properties[0][0]);

                    // modifies value with "implied value rule"
                    if (parentClasses && parentClasses.root.length === 1 && parentClasses.properties.length === 1) {
                        if (context.impliedValueRule) {
                            out = context.impliedValueRule(out, parentClasses.properties[0][0], classes.properties[0][0], value);
                        }
                    }

                    if (out.properties[propertyName]) {
                        out.properties[propertyName].push(rootItem);
                    } else {
                        out.properties[propertyName] = [rootItem];
                    }

                    context.rootID++;
                    // used to stop duplication in heavily nested structures
                    child.addedAsRoot = true;


                    x = 0;
                    i = rootItem.type.length;
                    itemRootID = context.rootID;
                    while (x < i) {
                        context.walkChildren(child, rootItem, rootItem.type, itemRootID, classes);
                        x++;
                    }
                    if (this.impliedRules) {
                        context.impliedRules(child, rootItem, classes);
                    }
                    this.cleanUfObject(rootItem);

                }

                // a property which is NOT a microformat and has not been used for a given root element
                if (classes.root.length === 0 && classes.properties.length > 0) {

                    x = 0;
                    i = classes.properties.length;
                    while (x < i) {

                        value = context.getValue(child, classes.properties[x][0], out);
                        propertyName = context.removePropPrefix(classes.properties[x][0]);
                        propertyVersion = classes.properties[x][1];

                        // modifies value with "implied value rule"
                        if (parentClasses && parentClasses.root.length === 1 && parentClasses.properties.length === 1) {
                            if (context.impliedValueRule) {
                                out = context.impliedValueRule(out, parentClasses.properties[0][0], classes.properties[x][0], value);
                            }
                        }

                        // if we have not added this value into a property with the same name already
                        if (!context.hasRootID(child, rootID, propertyName)) {
                            // check the root and property is the same version or if overlapping versions are allowed
                            if ( context.isAllowedPropertyVersion( out.typeVersion, propertyVersion ) ) {
                                // add the property as an array of properties
                                if (out.properties[propertyName]) {
                                    out.properties[propertyName].push(value);
                                } else {
                                    out.properties[propertyName] = [value];
                                }
                                // add rootid to node so we can track its use
                                context.appendRootID(child, rootID, propertyName);
                            }
                        }

                        x++;
                    }

                    context.walkChildren(child, out, ufName, rootID, classes);
                }

                // if the node has no microformat classes, see if its children have
                if (classes.root.length === 0 && classes.properties.length === 0) {
                    context.walkChildren(child, out, ufName, rootID, classes);
                }

                // if the node is a child root add it to the children tree
                if (classes.root.length > 0 && classes.properties.length === 0) {

                    // create object with type, property and value
                    rootItem = context.createUfObject(
                        classes.root,
                        classes.typeVersion,
                        modules.text.parse(this.document, child, context.options.textFormat)
                    );

                    // add the microformat as an array of properties
                    if (!out.children) {
                        out.children =  [];
                    }

                    if (!context.hasRootID(child, rootID, 'child-root')) {
                        out.children.push( rootItem );
                        context.appendRootID(child, rootID, 'child-root');
                        context.rootID++;
                    }

                    x = 0;
                    i = rootItem.type.length;
                    itemRootID = context.rootID;
                    while (x < i) {
                        context.walkChildren(child, rootItem, rootItem.type, itemRootID, classes);
                        x++;
                    }
                    if (this.impliedRules) {
                        context.impliedRules(child, rootItem, classes);
                    }
                    context.cleanUfObject( rootItem );

                }



                y++;
            }

        },




        /**
         * gets the value of a property from a node
         *
         * @param  {DOM Node} node
         * @param  {String} className
         * @param  {Object} uf
         * @return {String || Object}
         */
        getValue: function(node, className, uf) {
            var value = '';

            if (modules.utils.startWith(className, 'p-')) {
                value = this.getPValue(node, true);
            }

            if (modules.utils.startWith(className, 'e-')) {
                value = this.getEValue(node);
            }

            if (modules.utils.startWith(className, 'u-')) {
                value = this.getUValue(node, true);
            }

            if (modules.utils.startWith(className, 'dt-')) {
                value = this.getDTValue(node, className, uf, true);
            }
            return value;
        },


        /**
         * gets the value of a node which contains a 'p-' property
         *
         * @param  {DOM Node} node
         * @param  {Boolean} valueParse
         * @return {String}
         */
        getPValue: function(node, valueParse) {
            var out = '';
            if (valueParse) {
                out = this.getValueClass(node, 'p');
            }

            if (!out && valueParse) {
                out = this.getValueTitle(node);
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['abbr'], 'title');
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['data','input'], 'value');
            }

            if (node.name === 'br' || node.name === 'hr') {
                out = '';
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['img', 'area'], 'alt');
            }

            if (!out) {
                out = modules.text.parse(this.document, node, this.options.textFormat);
            }

            return (out) ? out : '';
        },


        /**
         * gets the value of a node which contains the 'e-' property
         *
         * @param  {DOM Node} node
         * @return {Object}
         */
        getEValue: function(node) {

            var out = {value: '', html: ''};

            this.expandURLs(node, 'src', this.options.baseUrl);
            this.expandURLs(node, 'href', this.options.baseUrl);

            out.value = modules.text.parse(this.document, node, this.options.textFormat);
            out.html = modules.html.parse(node);

            return out;
        },


        /**
         * gets the value of a node which contains the 'u-' property
         *
         * @param  {DOM Node} node
         * @param  {Boolean} valueParse
         * @return {String}
         */
        getUValue: function(node, valueParse) {
            var out = '';
            if (valueParse) {
                out = this.getValueClass(node, 'u');
            }

            if (!out && valueParse) {
                out = this.getValueTitle(node);
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['a', 'area'], 'href');
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['img','audio','video','source'], 'src');
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['object'], 'data');
            }

            // if we have no protocol separator, turn relative url to absolute url
            if (out && out !== '' && out.indexOf('://') === -1) {
                out = modules.url.resolve(out, this.options.baseUrl);
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['abbr'], 'title');
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['data','input'], 'value');
            }

            if (!out) {
                out = modules.text.parse(this.document, node, this.options.textFormat);
            }

            return (out) ? out : '';
        },


        /**
         * gets the value of a node which contains the 'dt-' property
         *
         * @param  {DOM Node} node
         * @param  {String} className
         * @param  {Object} uf
         * @param  {Boolean} valueParse
         * @return {String}
         */
        getDTValue: function(node, className, uf, valueParse) {
            var out = '';

            if (valueParse) {
                out = this.getValueClass(node, 'dt');
            }

            if (!out && valueParse) {
                out = this.getValueTitle(node);
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['time', 'ins', 'del'], 'datetime');
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['abbr'], 'title');
            }

            if (!out) {
                out = modules.domUtils.getAttrValFromTagList(node, ['data', 'input'], 'value');
            }

            if (!out) {
                out = modules.text.parse(this.document, node, this.options.textFormat);
            }

            if (out) {
                if (modules.dates.isDuration(out)) {
                    // just duration
                    return out;
                } else if (modules.dates.isTime(out)) {
                    // just time or time+timezone
                    if (uf) {
                        uf.times.push([className, modules.dates.parseAmPmTime(out, this.options.dateFormat)]);
                    }
                    return modules.dates.parseAmPmTime(out, this.options.dateFormat);
                }
                // returns a date - microformat profile
                if (uf) {
                    uf.dates.push([className, new modules.ISODate(out).toString( this.options.dateFormat )]);
                }
                return new modules.ISODate(out).toString( this.options.dateFormat );
            }
            return '';
        },


        /**
         * appends a new rootid to a given node
         *
         * @param  {DOM Node} node
         * @param  {String} id
         * @param  {String} propertyName
         */
        appendRootID: function(node, id, propertyName) {
            if (this.hasRootID(node, id, propertyName) === false) {
                var rootids = [];
                if (modules.domUtils.hasAttribute(node,'rootids')) {
                    rootids = modules.domUtils.getAttributeList(node,'rootids');
                }
                rootids.push('id' + id + '-' + propertyName);
                modules.domUtils.setAttribute(node, 'rootids', rootids.join(' '));
            }
        },


        /**
         * does a given node already have a rootid
         *
         * @param  {DOM Node} node
         * @param  {String} id
         * @param  {String} propertyName
         * @return {Boolean}
         */
        hasRootID: function(node, id, propertyName) {
            var rootids = [];
            if (!modules.domUtils.hasAttribute(node,'rootids')) {
                return false;
            }
            rootids = modules.domUtils.getAttributeList(node, 'rootids');
            return (rootids.indexOf('id' + id + '-' + propertyName) > -1);
        },



        /**
         * gets the text of any child nodes with a class value
         *
         * @param  {DOM Node} node
         * @param  {String} propertyName
         * @return {String || null}
         */
        getValueClass: function(node, propertyType) {
            var context = this,
                children = [],
                out = [],
                child,
                x,
                i;

            children = modules.domUtils.getChildren( node );

            x = 0;
            i = children.length;
            while (x < i) {
                child = children[x];
                var value = null;
                if (modules.domUtils.hasAttributeValue(child, 'class', 'value')) {
                    switch (propertyType) {
                    case 'p':
                        value = context.getPValue(child, false);
                        break;
                    case 'u':
                        value = context.getUValue(child, false);
                        break;
                    case 'dt':
                        value = context.getDTValue(child, '', null, false);
                        break;
                    }
                    if (value) {
                        out.push(modules.utils.trim(value));
                    }
                }
                x++;
            }
            if (out.length > 0) {
                if (propertyType === 'p') {
                    return modules.text.parseText( this.document, out.join(' '), this.options.textFormat);
                }
                if (propertyType === 'u') {
                    return out.join('');
                }
                if (propertyType === 'dt') {
                    return modules.dates.concatFragments(out,this.options.dateFormat).toString(this.options.dateFormat);
                }
                return undefined;
            }
            return null;
        },


        /**
         * returns a single string of the 'title' attr from all
         * the child nodes with the class 'value-title'
         *
         * @param  {DOM Node} node
         * @return {String}
         */
        getValueTitle: function(node) {
            var out = [],
                items,
                i,
                x;

            items = modules.domUtils.getNodesByAttributeValue(node, 'class', 'value-title');
            x = 0;
            i = items.length;
            while (x < i) {
                if (modules.domUtils.hasAttribute(items[x], 'title')) {
                    out.push(modules.domUtils.getAttribute(items[x], 'title'));
                }
                x++;
            }
            return out.join('');
        },


       /**
         * finds out whether a node has h-* class v1 and v2
         *
         * @param  {DOM Node} node
         * @return {Boolean}
         */
        hasHClass: function(node) {
            var classes = this.getUfClassNames(node);
            if (classes.root && classes.root.length > 0) {
                return true;
            }
            return false;
        },


        /**
         * get both the root and property class names from a node
         *
         * @param  {DOM Node} node
         * @param  {Array} ufNameArr
         * @return {Object}
         */
        getUfClassNames: function(node, ufNameArr) {
            var context = this,
                out = {
                    'root': [],
                    'properties': []
                },
                classNames,
                key,
                items,
                item,
                i,
                x,
                z,
                y,
                map,
                prop,
                propName,
                v2Name,
                impiedRel,
                ufName;

            // don't get classes from excluded list of tags
            if (modules.domUtils.hasTagName(node, this.excludeTags) === false) {

                // find classes for node
                classNames = modules.domUtils.getAttribute(node, 'class');
                if (classNames) {
                    items = classNames.split(' ');
                    x = 0;
                    i = items.length;
                    while (x < i) {

                        item = modules.utils.trim(items[x]);

                        // test for root prefix - v2
                        if (modules.utils.startWith(item, context.rootPrefix)) {
                            if (out.root.indexOf(item) === -1) {
                                out.root.push(item);
                            }
                            out.typeVersion = 'v2';
                        }

                        // test for property prefix - v2
                        z = context.propertyPrefixes.length;
                        while (z--) {
                            if (modules.utils.startWith(item, context.propertyPrefixes[z])) {
                                out.properties.push([item,'v2']);
                            }
                        }

                        // test for mapped root classnames v1
                        for (key in modules.maps) {
                            if (modules.maps.hasOwnProperty(key)) {
                                // only add a root once
                                if (modules.maps[key].root === item && out.root.indexOf(key) === -1) {
                                    // if root map has subTree set to true
                                    // test to see if we should create a property or root
                                    if (modules.maps[key].subTree) {
                                        out.properties.push(['p-' + modules.maps[key].root, 'v1']);
                                    } else {
                                        out.root.push(key);
                                        if (!out.typeVersion) {
                                            out.typeVersion = 'v1';
                                        }
                                    }
                                }
                            }
                        }


                        // test for mapped property classnames v1
                        if (ufNameArr) {
                            for (var a = 0; a < ufNameArr.length; a++) {
                                ufName = ufNameArr[a];
                                // get mapped property v1 microformat
                                map = context.getMapping(ufName);
                                if (map) {
                                    for (key in map.properties) {
                                        if (map.properties.hasOwnProperty(key)) {

                                            prop = map.properties[key];
                                            propName = (prop.map) ? prop.map : 'p-' + key;

                                            if (key === item) {
                                                if (prop.uf) {
                                                    // loop all the classList make sure
                                                    //   1. this property is a root
                                                    //   2. that there is not already an equivalent v2 property i.e. url and u-url on the same element
                                                    y = 0;
                                                    while (y < i) {
                                                        v2Name = context.getV2RootName(items[y]);
                                                        // add new root
                                                        if (prop.uf.indexOf(v2Name) > -1 && out.root.indexOf(v2Name) === -1) {
                                                            out.root.push(v2Name);
                                                            out.typeVersion = 'v1';
                                                        }
                                                        y++;
                                                    }
                                                    //only add property once
                                                    if (out.properties.indexOf(propName) === -1) {
                                                        out.properties.push([propName,'v1']);
                                                    }
                                                } else if (out.properties.indexOf(propName) === -1) {
                                                    out.properties.push([propName,'v1']);
                                                }
                                            }
                                        }

                                    }
                                }
                            }

                        }

                        x++;

                    }
                }
            }


            // finds any alt rel=* mappings for a given node/microformat
            if (ufNameArr && this.findRelImpied) {
                for (var b = 0; b < ufNameArr.length; b++) {
                    ufName = ufNameArr[b];
                    impiedRel = this.findRelImpied(node, ufName);
                    if (impiedRel && out.properties.indexOf(impiedRel) === -1) {
                        out.properties.push([impiedRel, 'v1']);
                    }
                }
            }


            //if(out.root.length === 1 && out.properties.length === 1) {
            //  if(out.root[0].replace('h-','') === this.removePropPrefix(out.properties[0][0])) {
            //      out.typeVersion = 'v2';
            //  }
            //}

            return out;
        },


        /**
         * given a v1 or v2 root name, return mapping object
         *
         * @param  {String} name
         * @return {Object || null}
         */
        getMapping: function(name) {
            var key;
            for (key in modules.maps) {
                if (modules.maps[key].root === name || key === name) {
                    return modules.maps[key];
                }
            }
            return null;
        },


        /**
         * given a v1 root name returns a v2 root name i.e. vcard >>> h-card
         *
         * @param  {String} name
         * @return {String || null}
         */
        getV2RootName: function(name) {
            var key;
            for (key in modules.maps) {
                if (modules.maps[key].root === name) {
                    return key;
                }
            }
            return null;
        },


        /**
         * whether a property is the right microformats version for its root type
         *
         * @param  {String} typeVersion
         * @param  {String} propertyVersion
         * @return {Boolean}
         */
        isAllowedPropertyVersion: function(typeVersion, propertyVersion) {
            if (this.options.overlappingVersions === true) {
                return true;
            }
            return (typeVersion === propertyVersion);
        },


        /**
         * creates a blank microformats object
         *
         * @param  {String} name
         * @param  {String} value
         * @return {Object}
         */
        createUfObject: function(names, typeVersion, value) {
            var out = {};

            // is more than just whitespace
            if (value && modules.utils.isOnlyWhiteSpace(value) === false) {
                out.value = value;
            }
            // add type i.e. ["h-card", "h-org"]
            if (modules.utils.isArray(names)) {
                out.type = names;
            } else {
                out.type = [names];
            }
            out.properties = {};
            // metadata properties for parsing
            out.typeVersion = typeVersion;
            out.times = [];
            out.dates = [];
            out.altValue = null;

            return out;
        },


        /**
         * removes unwanted microformats property before output
         *
         * @param  {Object} microformat
         */
        cleanUfObject: function( microformat ) {
            delete microformat.times;
            delete microformat.dates;
            delete microformat.typeVersion;
            delete microformat.altValue;
            return microformat;
        },



        /**
         * removes microformat property prefixes from text
         *
         * @param  {String} text
         * @return {String}
         */
        removePropPrefix: function(text) {
            var i;

            i = this.propertyPrefixes.length;
            while (i--) {
                var prefix = this.propertyPrefixes[i];
                if (modules.utils.startWith(text, prefix)) {
                    text = text.substr(prefix.length);
                }
            }
            return text;
        },


        /**
         * expands all relative URLs to absolute ones where it can
         *
         * @param  {DOM Node} node
         * @param  {String} attrName
         * @param  {String} baseUrl
         */
        expandURLs: function(node, attrName, baseUrl) {
            var i,
                nodes,
                attr;

            nodes = modules.domUtils.getNodesByAttribute(node, attrName);
            i = nodes.length;
            while (i--) {
                try {
                    // the url parser can blow up if the format is not right
                    attr = modules.domUtils.getAttribute(nodes[i], attrName);
                    if (attr && attr !== '' && baseUrl !== '' && attr.indexOf('://') === -1) {
                        //attr = urlParser.resolve(baseUrl, attr);
                        attr = modules.url.resolve(attr, baseUrl);
                        modules.domUtils.setAttribute(nodes[i], attrName, attr);
                    }
                } catch (err) {
                    // do nothing - convert only the urls we can, leave the rest as they are
                }
            }
        },



        /**
         * merges passed and default options -single level clone of properties
         *
         * @param  {Object} options
         */
        mergeOptions: function(options) {
            var key;
            for (key in options) {
                if (options.hasOwnProperty(key)) {
                    this.options[key] = options[key];
                }
            }
        },


        /**
         * removes all rootid attributes
         *
         * @param  {DOM Node} rootNode
         */
        removeRootIds: function(rootNode) {
            var arr,
                i;

            arr = modules.domUtils.getNodesByAttribute(rootNode, 'rootids');
            i = arr.length;
            while (i--) {
                modules.domUtils.removeAttribute(arr[i],'rootids');
            }
        },


        /**
         * removes all changes made to the DOM
         *
         * @param  {DOM Node} rootNode
         */
        clearUpDom: function(rootNode) {
            if (this.removeIncludes) {
                this.removeIncludes(rootNode);
            }
            this.removeRootIds(rootNode);
        }


    };


    modules.Parser.prototype.constructor = modules.Parser;


    // check parser module is loaded
    if (modules.Parser) {

        /**
         * applies "implied rules" microformat output structure i.e. feed-title, name, photo, url and date
         *
         * @param  {DOM Node} node
         * @param  {Object} uf (microformat output structure)
         * @param  {Object} parentClasses (classes structure)
         * @param  {Boolean} impliedPropertiesByVersion
         * @return {Object}
         */
         modules.Parser.prototype.impliedRules = function(node, uf, parentClasses) {
            var typeVersion = (uf.typeVersion)? uf.typeVersion: 'v2';

            // TEMP: override to allow v1 implied properties while spec changes
            if (this.options.impliedPropertiesByVersion === false) {
                typeVersion = 'v2';
            }

            if (node && uf && uf.properties) {
                uf = this.impliedBackwardComp( node, uf, parentClasses );
                if (typeVersion === 'v2') {
                    uf = this.impliedhFeedTitle( uf );
                    uf = this.impliedName( node, uf );
                    uf = this.impliedPhoto( node, uf );
                    uf = this.impliedUrl( node, uf );
                }
                uf = this.impliedValue( node, uf, parentClasses );
                uf = this.impliedDate( uf );

                // TEMP: flagged while spec changes are put forward
                if (this.options.parseLatLonGeo === true) {
                    uf = this.impliedGeo( uf );
                }
            }

            return uf;
        };


        /**
         * apply implied name rule
         *
         * @param  {DOM Node} node
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedName = function(node, uf) {
            // implied name rule
            /*
                img.h-x[alt]                                        <img class="h-card" src="glenn.htm" alt="Glenn Jones"></a>
                area.h-x[alt]                                       <area class="h-card" href="glenn.htm" alt="Glenn Jones"></area>
                abbr.h-x[title]                                     <abbr class="h-card" title="Glenn Jones"GJ</abbr>

                .h-x>img:only-child[alt]:not[.h-*]                  <div class="h-card"><a src="glenn.htm" alt="Glenn Jones"></a></div>
                .h-x>area:only-child[alt]:not[.h-*]                 <div class="h-card"><area href="glenn.htm" alt="Glenn Jones"></area></div>
                .h-x>abbr:only-child[title]                         <div class="h-card"><abbr title="Glenn Jones">GJ</abbr></div>

                .h-x>:only-child>img:only-child[alt]:not[.h-*]      <div class="h-card"><span><img src="jane.html" alt="Jane Doe"/></span></div>
                .h-x>:only-child>area:only-child[alt]:not[.h-*]     <div class="h-card"><span><area href="jane.html" alt="Jane Doe"></area></span></div>
                .h-x>:only-child>abbr:only-child[title]             <div class="h-card"><span><abbr title="Jane Doe">JD</abbr></span></div>
            */
            var name,
                value;

            if (!uf.properties.name) {
                value = this.getImpliedProperty(node, ['img', 'area', 'abbr'], this.getNameAttr);
                var textFormat = this.options.textFormat;
                // if no value for tags/properties use text
                if (!value) {
                    name = [modules.text.parse(this.document, node, textFormat)];
                } else {
                    name = [modules.text.parseText(this.document, value, textFormat)];
                }
                if (name && name[0] !== '') {
                    uf.properties.name = name;
                }
            }

            return uf;
        };


        /**
         * apply implied photo rule
         *
         * @param  {DOM Node} node
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedPhoto = function(node, uf) {
            // implied photo rule
            /*
                img.h-x[src]                                                <img class="h-card" alt="Jane Doe" src="jane.jpeg"/>
                object.h-x[data]                                            <object class="h-card" data="jane.jpeg"/>Jane Doe</object>
                .h-x>img[src]:only-of-type:not[.h-*]                        <div class="h-card"><img alt="Jane Doe" src="jane.jpeg"/></div>
                .h-x>object[data]:only-of-type:not[.h-*]                    <div class="h-card"><object data="jane.jpeg"/>Jane Doe</object></div>
                .h-x>:only-child>img[src]:only-of-type:not[.h-*]            <div class="h-card"><span><img alt="Jane Doe" src="jane.jpeg"/></span></div>
                .h-x>:only-child>object[data]:only-of-type:not[.h-*]        <div class="h-card"><span><object data="jane.jpeg"/>Jane Doe</object></span></div>
            */
            var value;
            if (!uf.properties.photo) {
                value = this.getImpliedProperty(node, ['img', 'object'], this.getPhotoAttr);
                if (value) {
                    // relative to absolute URL
                    if (value && value !== '' && this.options.baseUrl !== '' && value.indexOf('://') === -1) {
                        value = modules.url.resolve(value, this.options.baseUrl);
                    }
                    uf.properties.photo = [modules.utils.trim(value)];
                }
            }
            return uf;
        };


        /**
         * apply implied URL rule
         *
         * @param  {DOM Node} node
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedUrl = function(node, uf) {
            // implied URL rule
            /*
                a.h-x[href]                             <a class="h-card" href="glenn.html">Glenn</a>
                area.h-x[href]                          <area class="h-card" href="glenn.html">Glenn</area>
                .h-x>a[href]:only-of-type:not[.h-*]     <div class="h-card" ><a href="glenn.html">Glenn</a><p>...</p></div>
                .h-x>area[href]:only-of-type:not[.h-*]  <div class="h-card" ><area href="glenn.html">Glenn</area><p>...</p></div>
            */
            var value;
            if (!uf.properties.url) {
                value = this.getImpliedProperty(node, ['a', 'area'], this.getURLAttr);
                if (value) {
                    // relative to absolute URL
                    if (value && value !== '' && this.options.baseUrl !== '' && value.indexOf('://') === -1) {
                        value = modules.url.resolve(value, this.options.baseUrl);
                    }
                    uf.properties.url = [modules.utils.trim(value)];
                }
            }
            return uf;
        };


        /**
         * apply implied date rule - if there is a time only property try to concat it with any date property
         *
         * @param  {DOM Node} node
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedDate = function(uf) {
            // implied date rule
            // http://microformats.org/wiki/value-class-pattern#microformats2_parsers
            // http://microformats.org/wiki/microformats2-parsing-issues#implied_date_for_dt_properties_both_mf2_and_backcompat
            var newDate;
            if (uf.times.length > 0 && uf.dates.length > 0) {
                newDate = modules.dates.dateTimeUnion(uf.dates[0][1], uf.times[0][1], this.options.dateFormat);
                uf.properties[this.removePropPrefix(uf.times[0][0])][0] = newDate.toString(this.options.dateFormat);
            }
            // clean-up object
            delete uf.times;
            delete uf.dates;
            return uf;
        };


        /**
         * get an implied property value from pre-defined tag/attriubte combinations
         *
         * @param  {DOM Node} node
         * @param  {String} tagList (Array of tags from which an implied value can be pulled)
         * @param  {String} getAttrFunction (Function which can extract implied value)
         * @return {String || null}
         */
        modules.Parser.prototype.getImpliedProperty = function(node, tagList, getAttrFunction) {
            // i.e. img.h-card
            var value = getAttrFunction(node),
                descendant,
                child;

            if (!value) {
                // i.e. .h-card>img:only-of-type:not(.h-card)
                descendant = modules.domUtils.getSingleDescendantOfType( node, tagList);
                if (descendant && this.hasHClass(descendant) === false) {
                    value = getAttrFunction(descendant);
                }
                if (node.children.length > 0 ) {
                    // i.e.  .h-card>:only-child>img:only-of-type:not(.h-card)
                    child = modules.domUtils.getSingleDescendant(node);
                    if (child && this.hasHClass(child) === false) {
                        descendant = modules.domUtils.getSingleDescendantOfType(child, tagList);
                        if (descendant && this.hasHClass(descendant) === false) {
                            value = getAttrFunction(descendant);
                        }
                    }
                }
            }

            return value;
        };


        /**
         * get an implied name value from a node
         *
         * @param  {DOM Node} node
         * @return {String || null}
         */
        modules.Parser.prototype.getNameAttr = function(node) {
            var value = modules.domUtils.getAttrValFromTagList(node, ['img','area'], 'alt');
            if (!value) {
                value = modules.domUtils.getAttrValFromTagList(node, ['abbr'], 'title');
            }
            return value;
        };


        /**
         * get an implied photo value from a node
         *
         * @param  {DOM Node} node
         * @return {String || null}
         */
        modules.Parser.prototype.getPhotoAttr = function(node) {
            var value = modules.domUtils.getAttrValFromTagList(node, ['img'], 'src');
            if (!value && modules.domUtils.hasAttributeValue(node, 'class', 'include') === false) {
                value = modules.domUtils.getAttrValFromTagList(node, ['object'], 'data');
            }
            return value;
        };


        /**
         * get an implied photo value from a node
         *
         * @param  {DOM Node} node
         * @return {String || null}
         */
        modules.Parser.prototype.getURLAttr = function(node) {
            var value = null;
            if (modules.domUtils.hasAttributeValue(node, 'class', 'include') === false) {

                value = modules.domUtils.getAttrValFromTagList(node, ['a'], 'href');
                if (!value) {
                    value = modules.domUtils.getAttrValFromTagList(node, ['area'], 'href');
                }

            }
            return value;
        };


        /**
         *
         *
         * @param  {DOM Node} node
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedValue = function(node, uf, parentClasses) {

            // intersection of implied name and implied value rules
            if (uf.properties.name) {
                if (uf.value && parentClasses.root.length > 0 && parentClasses.properties.length === 1) {
                    uf = this.getAltValue(uf, parentClasses.properties[0][0], 'p-name', uf.properties.name[0]);
                }
            }

            // intersection of implied URL and implied value rules
            if (uf.properties.url) {
                if (parentClasses && parentClasses.root.length === 1 && parentClasses.properties.length === 1) {
                    uf = this.getAltValue(uf, parentClasses.properties[0][0], 'u-url', uf.properties.url[0]);
                }
            }

            // apply alt value
            if (uf.altValue !== null) {
                uf.value = uf.altValue.value;
            }
            delete uf.altValue;


            return uf;
        };


        /**
         * get alt value based on rules about parent property prefix
         *
         * @param  {Object} uf
         * @param  {String} parentPropertyName
         * @param  {String} propertyName
         * @param  {String} value
         * @return {Object}
         */
        modules.Parser.prototype.getAltValue = function(uf, parentPropertyName, propertyName, value) {
            if (uf.value && !uf.altValue) {
                // first p-name of the h-* child
                if (modules.utils.startWith(parentPropertyName,'p-') && propertyName === 'p-name') {
                    uf.altValue = {name: propertyName, value: value};
                }
                // if it's an e-* property element
                if (modules.utils.startWith(parentPropertyName,'e-') && modules.utils.startWith(propertyName,'e-')) {
                    uf.altValue = {name: propertyName, value: value};
                }
                // if it's an u-* property element
                if (modules.utils.startWith(parentPropertyName,'u-') && propertyName === 'u-url') {
                    uf.altValue = {name: propertyName, value: value};
                }
            }
            return uf;
        };


        /**
         * if a h-feed does not have a title use the title tag of a page
         *
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedhFeedTitle = function( uf ) {
            if (uf.type && uf.type.indexOf('h-feed') > -1) {
                // has no name property
                if (uf.properties.name === undefined || uf.properties.name[0] === '' ) {
                    // use the text from the title tag
                    var title = modules.domUtils.querySelector(this.document, 'title');
                    if (title) {
                        uf.properties.name = [modules.domUtils.textContent(title)];
                    }
                }
            }
            return uf;
        };



        /**
         * implied Geo from pattern <abbr class="p-geo" title="37.386013;-122.082932">
         *
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedGeo = function( uf ) {
            var geoPair,
                parts,
                longitude,
                latitude,
                valid = true;

            if (uf.type && uf.type.indexOf('h-geo') > -1) {

                // has no latitude or longitude property
                if (uf.properties.latitude === undefined || uf.properties.longitude === undefined ) {

                    geoPair = (uf.properties.name)? uf.properties.name[0] : null;
                    geoPair = (!geoPair && uf.properties.value)? uf.properties.value : geoPair;

                    if (geoPair) {
                        // allow for the use of a ';' as in microformats and also ',' as in Geo URL
                        geoPair = geoPair.replace(';',',');

                        // has sep char
                        if (geoPair.indexOf(',') > -1 ) {
                            parts = geoPair.split(',');

                            // only correct if we have two or more parts
                            if (parts.length > 1) {

                                // latitude no value outside the range -90 or 90
                                latitude = parseFloat( parts[0] );
                                if (modules.utils.isNumber(latitude) && latitude > 90 || latitude < -90) {
                                    valid = false;
                                }

                                // longitude no value outside the range -180 to 180
                                longitude = parseFloat( parts[1] );
                                if (modules.utils.isNumber(longitude) && longitude > 180 || longitude < -180) {
                                    valid = false;
                                }

                                if (valid) {
                                    uf.properties.latitude = [latitude];
                                    uf.properties.longitude  = [longitude];
                                }
                            }

                        }
                    }
                }
            }
            return uf;
        };


        /**
         * if a backwards compat built structure has no properties add name through this.impliedName
         *
         * @param  {Object} uf
         * @return {Object}
         */
        modules.Parser.prototype.impliedBackwardComp = function(node, uf, parentClasses) {

            // look for pattern in parent classes like "p-geo h-geo"
            // these are structures built from backwards compat parsing of geo
            if (parentClasses.root.length === 1 && parentClasses.properties.length === 1) {
                if (parentClasses.root[0].replace('h-','') === this.removePropPrefix(parentClasses.properties[0][0])) {

                    // if microformat has no properties apply the impliedName rule to get value from containing node
                    // this will get value from html such as <abbr class="geo" title="30.267991;-97.739568">Brighton</abbr>
                    if ( modules.utils.hasProperties(uf.properties) === false ) {
                        uf = this.impliedName( node, uf );
                    }
                }
            }

            return uf;
        };



    }


    // check parser module is loaded
    if (modules.Parser) {


        /**
         * appends clones of include Nodes into the DOM structure
         *
         * @param  {DOM node} rootNode
         */
        modules.Parser.prototype.addIncludes = function(rootNode) {
            this.addAttributeIncludes(rootNode, 'itemref');
            this.addAttributeIncludes(rootNode, 'headers');
            this.addClassIncludes(rootNode);
        };


        /**
         * appends clones of include Nodes into the DOM structure for attribute based includes
         *
         * @param  {DOM node} rootNode
         * @param  {String} attributeName
         */
        modules.Parser.prototype.addAttributeIncludes = function(rootNode, attributeName) {
            var arr,
                idList,
                i,
                x,
                z,
                y;

            arr = modules.domUtils.getNodesByAttribute(rootNode, attributeName);
            x = 0;
            i = arr.length;
            while (x < i) {
                idList = modules.domUtils.getAttributeList(arr[x], attributeName);
                if (idList) {
                    z = 0;
                    y = idList.length;
                    while (z < y) {
                        this.apppendInclude(arr[x], idList[z]);
                        z++;
                    }
                }
                x++;
            }
        };


        /**
         * appends clones of include Nodes into the DOM structure for class based includes
         *
         * @param  {DOM node} rootNode
         */
        modules.Parser.prototype.addClassIncludes = function(rootNode) {
            var id,
                arr,
                x = 0,
                i;

            arr = modules.domUtils.getNodesByAttributeValue(rootNode, 'class', 'include');
            i = arr.length;
            while (x < i) {
                id = modules.domUtils.getAttrValFromTagList(arr[x], ['a'], 'href');
                if (!id) {
                    id = modules.domUtils.getAttrValFromTagList(arr[x], ['object'], 'data');
                }
                this.apppendInclude(arr[x], id);
                x++;
            }
        };


        /**
         * appends a clone of an include into another Node using Id
         *
         * @param  {DOM node} rootNode
         * @param  {Stringe} id
         */
        modules.Parser.prototype.apppendInclude = function(node, id) {
            var include,
                clone;

            id = modules.utils.trim(id.replace('#', ''));
            include = modules.domUtils.getElementById(this.document, id);
            if (include) {
                clone = modules.domUtils.clone(include);
                this.markIncludeChildren(clone);
                modules.domUtils.appendChild(node, clone);
            }
        };


        /**
         * adds an attribute marker to all the child microformat roots
         *
         * @param  {DOM node} rootNode
         */
        modules.Parser.prototype.markIncludeChildren = function(rootNode) {
            var arr,
                x,
                i;

            // loop the array and add the attribute
            arr = this.findRootNodes(rootNode);
            x = 0;
            i = arr.length;
            modules.domUtils.setAttribute(rootNode, 'data-include', 'true');
            modules.domUtils.setAttribute(rootNode, 'style', 'display:none');
            while (x < i) {
                modules.domUtils.setAttribute(arr[x], 'data-include', 'true');
                x++;
            }
        };


        /**
         * removes all appended include clones from DOM
         *
         * @param  {DOM node} rootNode
         */
        modules.Parser.prototype.removeIncludes = function(rootNode) {
            var arr,
                i;

            // remove all the items that were added as includes
            arr = modules.domUtils.getNodesByAttribute(rootNode, 'data-include');
            i = arr.length;
            while (i--) {
                modules.domUtils.removeChild(rootNode,arr[i]);
            }
        };


    }


    // check parser module is loaded
    if (modules.Parser) {

        /**
         * finds rel=* structures
         *
         * @param  {DOM node} rootNode
         * @return {Object}
         */
        modules.Parser.prototype.findRels = function(rootNode) {
            var out = {
                    'items': [],
                    'rels': {},
                    'rel-urls': {}
                },
                x,
                i,
                y,
                z,
                relList,
                items,
                item,
                value,
                arr;

            arr = modules.domUtils.getNodesByAttribute(rootNode, 'rel');
            x = 0;
            i = arr.length;
            while (x < i) {
                relList = modules.domUtils.getAttribute(arr[x], 'rel');

                if (relList) {
                    items = relList.split(' ');


                    // add rels
                    z = 0;
                    y = items.length;
                    while (z < y) {
                        item = modules.utils.trim(items[z]);

                        // get rel value
                        value = modules.domUtils.getAttrValFromTagList(arr[x], ['a', 'area'], 'href');
                        if (!value) {
                            value = modules.domUtils.getAttrValFromTagList(arr[x], ['link'], 'href');
                        }

                        // create the key
                        if (!out.rels[item]) {
                            out.rels[item] = [];
                        }

                        if (typeof this.options.baseUrl === 'string' && typeof value === 'string') {

                            var resolved = modules.url.resolve(value, this.options.baseUrl);
                            // do not add duplicate rels - based on resolved URLs
                            if (out.rels[item].indexOf(resolved) === -1) {
                                out.rels[item].push( resolved );
                            }
                        }
                        z++;
                    }


                    var url = null;
                    if (modules.domUtils.hasAttribute(arr[x], 'href')) {
                        url = modules.domUtils.getAttribute(arr[x], 'href');
                        if (url) {
                            url = modules.url.resolve(url, this.options.baseUrl );
                        }
                    }


                    // add to rel-urls
                    var relUrl = this.getRelProperties(arr[x]);
                    relUrl.rels = items;
                    // // do not add duplicate rel-urls - based on resolved URLs
                    if (url && out['rel-urls'][url] === undefined) {
                        out['rel-urls'][url] = relUrl;
                    }


                }
                x++;
            }
            return out;
        };


        /**
         * gets the properties of a rel=*
         *
         * @param  {DOM node} node
         * @return {Object}
         */
        modules.Parser.prototype.getRelProperties = function(node) {
            var obj = {};

            if (modules.domUtils.hasAttribute(node, 'media')) {
                obj.media = modules.domUtils.getAttribute(node, 'media');
            }
            if (modules.domUtils.hasAttribute(node, 'type')) {
                obj.type = modules.domUtils.getAttribute(node, 'type');
            }
            if (modules.domUtils.hasAttribute(node, 'hreflang')) {
                obj.hreflang = modules.domUtils.getAttribute(node, 'hreflang');
            }
            if (modules.domUtils.hasAttribute(node, 'title')) {
                obj.title = modules.domUtils.getAttribute(node, 'title');
            }
            if (modules.utils.trim(this.getPValue(node, false)) !== '') {
                obj.text = this.getPValue(node, false);
            }

            return obj;
        };


        /**
         * finds any alt rel=* mappings for a given node/microformat
         *
         * @param  {DOM node} node
         * @param  {String} ufName
         * @return {String || undefined}
         */
        modules.Parser.prototype.findRelImpied = function(node, ufName) {
            var out,
                map,
                i;

            map = this.getMapping(ufName);
            if (map) {
                for (var key in map.properties) {
                    if (map.properties.hasOwnProperty(key)) {
                        var prop = map.properties[key],
                            propName = (prop.map) ? prop.map : 'p-' + key,
                            relCount = 0;

                        // is property an alt rel=* mapping
                        if (prop.relAlt && modules.domUtils.hasAttribute(node, 'rel')) {
                            i = prop.relAlt.length;
                            while (i--) {
                                if (modules.domUtils.hasAttributeValue(node, 'rel', prop.relAlt[i])) {
                                    relCount++;
                                }
                            }
                            if (relCount === prop.relAlt.length) {
                                out = propName;
                            }
                        }
                    }
                }
            }
            return out;
        };


        /**
         * returns whether a node or its children has rel=* microformat
         *
         * @param  {DOM node} node
         * @return {Boolean}
         */
        modules.Parser.prototype.hasRel = function(node) {
            return (this.countRels(node) > 0);
        };


        /**
         * returns the number of rel=* microformats
         *
         * @param  {DOM node} node
         * @return {Int}
         */
        modules.Parser.prototype.countRels = function(node) {
            if (node) {
                return modules.domUtils.getNodesByAttribute(node, 'rel').length;
            }
            return 0;
        };



    }


    modules.utils = {

        /**
         * is the object a string
         *
         * @param  {Object} obj
         * @return {Boolean}
         */
        isString: function( obj ) {
            return typeof( obj ) === 'string';
        },

        /**
         * is the object a number
         *
         * @param  {Object} obj
         * @return {Boolean}
         */
        isNumber: function( obj ) {
            return !isNaN(parseFloat( obj )) && isFinite( obj );
        },


        /**
         * is the object an array
         *
         * @param  {Object} obj
         * @return {Boolean}
         */
        isArray: function( obj ) {
            return obj && !( obj.propertyIsEnumerable( 'length' ) ) && typeof obj === 'object' && typeof obj.length === 'number';
        },


        /**
         * is the object a function
         *
         * @param  {Object} obj
         * @return {Boolean}
         */
        isFunction: function(obj) {
            return !!(obj && obj.constructor && obj.call && obj.apply);
        },


        /**
         * does the text start with a test string
         *
         * @param  {String} text
         * @param  {String} test
         * @return {Boolean}
         */
        startWith: function( text, test ) {
            return (text.indexOf(test) === 0);
        },


        /**
         * removes spaces at front and back of text
         *
         * @param  {String} text
         * @return {String}
         */
        trim: function( text ) {
            if (text && this.isString(text)) {
                return (text.trim())? text.trim() : text.replace(/^\s+|\s+$/g, '');
            }
            return '';
        },


        /**
         * replaces a character in text
         *
         * @param  {String} text
         * @param  {Int} index
         * @param  {String} character
         * @return {String}
         */
        replaceCharAt: function( text, index, character ) {
            if (text && text.length > index) {
               return text.substr(0, index) + character + text.substr(index+character.length);
            }
            return text;
        },


        /**
         * removes whitespace, tabs and returns from start and end of text
         *
         * @param  {String} text
         * @return {String}
         */
        trimWhitespace: function( text ) {
            if (text && text.length) {
                var i = text.length,
                    x = 0;

                // turn all whitespace chars at end into spaces
                while (i--) {
                    if (this.isOnlyWhiteSpace(text[i])) {
                        text = this.replaceCharAt( text, i, ' ' );
                    } else {
                        break;
                    }
                }

                // turn all whitespace chars at start into spaces
                i = text.length;
                while (x < i) {
                    if (this.isOnlyWhiteSpace(text[x])) {
                        text = this.replaceCharAt( text, i, ' ' );
                    } else {
                        break;
                    }
                    x++;
                }
            }
            return this.trim(text);
        },


        /**
         * does text only contain whitespace characters
         *
         * @param  {String} text
         * @return {Boolean}
         */
        isOnlyWhiteSpace: function( text ) {
            return !(/[^\t\n\r ]/.test( text ));
        },


        /**
         * removes whitespace from text (leaves a single space)
         *
         * @param  {String} text
         * @return {Sring}
         */
        collapseWhiteSpace: function( text ) {
            return text.replace(/[\t\n\r ]+/g, ' ');
        },


        /**
         * does an object have any of its own properties
         *
         * @param  {Object} obj
         * @return {Boolean}
         */
        hasProperties: function( obj ) {
            var key;
            for (key in obj) {
                if ( obj.hasOwnProperty( key ) ) {
                    return true;
                }
            }
            return false;
        },


        /**
         * a sort function - to sort objects in an array by a given property
         *
         * @param  {String} property
         * @param  {Boolean} reverse
         * @return {Int}
         */
        sortObjects: function(property, reverse) {
            reverse = (reverse) ? -1 : 1;
            return function (a, b) {
                a = a[property];
                b = b[property];
                if (a < b) {
                    return reverse * -1;
                }
                if (a > b) {
                    return reverse * 1;
                }
                return 0;
            };
        }

    };


    modules.domUtils = {

        // blank objects for DOM
        document: null,
        rootNode: null,


         /**
         * gets DOMParser object
         *
         * @return {Object || undefined}
         */
        getDOMParser: function () {
            if (typeof DOMParser === "undefined") {
                try {
                    return Components.classes["@mozilla.org/xmlextras/domparser;1"]
                        .createInstance(Components.interfaces.nsIDOMParser);
                } catch (e) {
                    return undefined;
                }
            } else {
                return new DOMParser();
            }
        },


         /**
         * configures what are the base DOM objects for parsing
         *
         * @param  {Object} options
         * @return {DOM Node} node
         */
        getDOMContext: function( options ) {

            // if a node is passed
            if (options.node) {
                this.rootNode = options.node;
            }


            // if a html string is passed
            if (options.html) {
                //var domParser = new DOMParser();
                var domParser = this.getDOMParser();
                this.rootNode = domParser.parseFromString( options.html, 'text/html' );
            }


            // find top level document from rootnode
            if (this.rootNode !== null) {
                if (this.rootNode.nodeType === 9) {
                    this.document = this.rootNode;
                    this.rootNode = modules.domUtils.querySelector(this.rootNode, 'html');
                } else {
                    // if it's DOM node get parent DOM Document
                    this.document = modules.domUtils.ownerDocument(this.rootNode);
                }
            }


            // use global document object
            if (!this.rootNode && document) {
                this.rootNode = modules.domUtils.querySelector(document, 'html');
                this.document = document;
            }


            if (this.rootNode && this.document) {
                return {document: this.document, rootNode: this.rootNode};
            }

            return {document: null, rootNode: null};
        },



        /**
        * gets the first DOM node
        *
        * @param  {Dom Document}
        * @return {DOM Node} node
        */
        getTopMostNode: function( node ) {
            //var doc = this.ownerDocument(node);
            //if(doc && doc.nodeType && doc.nodeType === 9 && doc.documentElement){
            //  return doc.documentElement;
            //}
            return node;
        },



         /**
         * abstracts DOM ownerDocument
         *
         * @param  {DOM Node} node
         * @return {Dom Document}
         */
        ownerDocument: function(node) {
            return node.ownerDocument;
        },


        /**
         * abstracts DOM textContent
         *
         * @param  {DOM Node} node
         * @return {String}
         */
        textContent: function(node) {
            if (node.textContent) {
                return node.textContent;
            } else if (node.innerText) {
                return node.innerText;
            }
            return '';
        },


        /**
         * abstracts DOM innerHTML
         *
         * @param  {DOM Node} node
         * @return {String}
         */
        innerHTML: function(node) {
            return node.innerHTML;
        },


        /**
         * abstracts DOM hasAttribute
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         * @return {Boolean}
         */
        hasAttribute: function(node, attributeName) {
            return node.hasAttribute(attributeName);
        },


        /**
         * does an attribute contain a value
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         * @param  {String} value
         * @return {Boolean}
         */
        hasAttributeValue: function(node, attributeName, value) {
            return (this.getAttributeList(node, attributeName).indexOf(value) > -1);
        },


        /**
         * abstracts DOM getAttribute
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         * @return {String || null}
         */
        getAttribute: function(node, attributeName) {
            return node.getAttribute(attributeName);
        },


        /**
         * abstracts DOM setAttribute
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         * @param  {String} attributeValue
         */
        setAttribute: function(node, attributeName, attributeValue) {
            node.setAttribute(attributeName, attributeValue);
        },


        /**
         * abstracts DOM removeAttribute
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         */
        removeAttribute: function(node, attributeName) {
            node.removeAttribute(attributeName);
        },


        /**
         * abstracts DOM getElementById
         *
         * @param  {DOM Node || DOM Document} node
         * @param  {String} id
         * @return {DOM Node}
         */
        getElementById: function(docNode, id) {
            return docNode.querySelector( '#' + id );
        },


        /**
         * abstracts DOM querySelector
         *
         * @param  {DOM Node || DOM Document} node
         * @param  {String} selector
         * @return {DOM Node}
         */
        querySelector: function(docNode, selector) {
            return docNode.querySelector( selector );
        },


        /**
         * get value of a Node attribute as an array
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         * @return {Array}
         */
        getAttributeList: function(node, attributeName) {
            var out = [],
                attList;

            attList = node.getAttribute(attributeName);
            if (attList && attList !== '') {
                if (attList.indexOf(' ') > -1) {
                    out = attList.split(' ');
                } else {
                    out.push(attList);
                }
            }
            return out;
        },


        /**
         * gets all child nodes with a given attribute
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         * @return {NodeList}
         */
        getNodesByAttribute: function(node, attributeName) {
            var selector = '[' + attributeName + ']';
            return node.querySelectorAll(selector);
        },


        /**
         * gets all child nodes with a given attribute containing a given value
         *
         * @param  {DOM Node} node
         * @param  {String} attributeName
         * @return {DOM NodeList}
         */
        getNodesByAttributeValue: function(rootNode, name, value) {
            var arr = [],
                x = 0,
                i,
                out = [];

            arr = this.getNodesByAttribute(rootNode, name);
            if (arr) {
                i = arr.length;
                while (x < i) {
                    if (this.hasAttributeValue(arr[x], name, value)) {
                        out.push(arr[x]);
                    }
                    x++;
                }
            }
            return out;
        },


        /**
         * gets attribute value from controlled list of tags
         *
         * @param  {Array} tagNames
         * @param  {String} attributeName
         * @return {String || null}
         */
        getAttrValFromTagList: function(node, tagNames, attributeName) {
            var i = tagNames.length;

            while (i--) {
                if (node.tagName.toLowerCase() === tagNames[i]) {
                    var attrValue = this.getAttribute(node, attributeName);
                    if (attrValue && attrValue !== '') {
                        return attrValue;
                    }
                }
            }
            return null;
        },


       /**
         * get node if it has no siblings. CSS equivalent is :only-child
         *
         * @param  {DOM Node} rootNode
         * @param  {Array} tagNames
         * @return {DOM Node || null}
         */
        getSingleDescendant: function(node) {
            return this.getDescendant( node, null, false );
        },


        /**
         * get node if it has no siblings of the same type. CSS equivalent is :only-of-type
         *
         * @param  {DOM Node} rootNode
         * @param  {Array} tagNames
         * @return {DOM Node || null}
         */
        getSingleDescendantOfType: function(node, tagNames) {
            return this.getDescendant( node, tagNames, true );
        },


        /**
         * get child node limited by presence of siblings - either CSS :only-of-type or :only-child
         *
         * @param  {DOM Node} rootNode
         * @param  {Array} tagNames
         * @return {DOM Node || null}
         */
        getDescendant: function( node, tagNames, onlyOfType ) {
            var i = node.children.length,
                countAll = 0,
                countOfType = 0,
                child,
                out = null;

            while (i--) {
                child = node.children[i];
                if (child.nodeType === 1) {
                    if (tagNames) {
                        // count just only-of-type
                        if (this.hasTagName(child, tagNames)) {
                            out = child;
                            countOfType++;
                        }
                    } else {
                        // count all elements
                        out = child;
                        countAll++;
                    }
                }
            }
            if (onlyOfType === true) {
                return (countOfType === 1)? out : null;
            }
            return (countAll === 1)? out : null;
        },


       /**
         * is a node one of a list of tags
         *
         * @param  {DOM Node} rootNode
         * @param  {Array} tagNames
         * @return {Boolean}
         */
        hasTagName: function(node, tagNames) {
            var i = tagNames.length;
            while (i--) {
                if (node.tagName.toLowerCase() === tagNames[i]) {
                    return true;
                }
            }
            return false;
        },


       /**
         * abstracts DOM appendChild
         *
         * @param  {DOM Node} node
         * @param  {DOM Node} childNode
         * @return {DOM Node}
         */
        appendChild: function(node, childNode) {
            return node.appendChild(childNode);
        },


       /**
         * abstracts DOM removeChild
         *
         * @param  {DOM Node} childNode
         * @return {DOM Node || null}
         */
        removeChild: function(childNode) {
            if (childNode.parentNode) {
                return childNode.parentNode.removeChild(childNode);
            }
            return null;
        },


        /**
         * abstracts DOM cloneNode
         *
         * @param  {DOM Node} node
         * @return {DOM Node}
         */
        clone: function(node) {
            var newNode = node.cloneNode(true);
            newNode.removeAttribute('id');
            return newNode;
        },


        /**
         * gets the text of a node
         *
         * @param  {DOM Node} node
         * @return {String}
         */
        getElementText: function( node ) {
            if (node && node.data) {
                return node.data;
            }
            return '';
        },


        /**
         * gets the attributes of a node - ordered by sequence in html
         *
         * @param  {DOM Node} node
         * @return {Array}
         */
        getOrderedAttributes: function( node ) {
            var nodeStr = node.outerHTML,
                attrs = [];

            for (var i = 0; i < node.attributes.length; i++) {
                var attr = node.attributes[i];
                    attr.indexNum = nodeStr.indexOf(attr.name);

                attrs.push( attr );
            }
            return attrs.sort( modules.utils.sortObjects( 'indexNum' ) );
        },


        /**
         * decodes html entities in given text
         *
         * @param  {DOM Document} doc
         * @param  String} text
         * @return {String}
         */
        decodeEntities: function( doc, text ) {
            //return text;
            return doc.createTextNode( text ).nodeValue;
        },


        /**
         * clones a DOM document
         *
         * @param  {DOM Document} document
         * @return {DOM Document}
         */
        cloneDocument: function( document ) {
            var newNode,
                newDocument = null;

            if ( this.canCloneDocument( document )) {
                newDocument = document.implementation.createHTMLDocument('');
                newNode = newDocument.importNode( document.documentElement, true );
                newDocument.replaceChild(newNode, newDocument.querySelector('html'));
            }
            return (newNode && newNode.nodeType && newNode.nodeType === 1)? newDocument : document;
        },


        /**
         * can environment clone a DOM document
         *
         * @param  {DOM Document} document
         * @return {Boolean}
         */
        canCloneDocument: function( document ) {
            return (document && document.importNode && document.implementation && document.implementation.createHTMLDocument);
        },


        /**
         * get the child index of a node. Used to create a node path
         *
         *   @param  {DOM Node} node
         *   @return {Int}
         */
        getChildIndex: function (node) {
            var parent = node.parentNode,
                i = -1,
                child;
            while (parent && (child = parent.childNodes[++i])) {
                 if (child === node) {
                     return i;
                 }
            }
            return -1;
        },


        /**
         * get a node's path
         *
         *   @param  {DOM Node} node
         *   @return {Array}
         */
        getNodePath: function  (node) {
            var parent = node.parentNode,
                path = [],
                index = this.getChildIndex(node);

          if (parent && (path = this.getNodePath(parent))) {
               if (index > -1) {
                   path.push(index);
               }
          }
          return path;
        },


        /**
         * get a node from a path.
         *
         *   @param  {DOM document} document
         *   @param  {Array} path
         *   @return {DOM Node}
         */
        getNodeByPath: function (document, path) {
            var node = document.documentElement,
                i = 0,
                index;
          while ((index = path[++i]) > -1) {
              node = node.childNodes[index];
          }
          return node;
        },


        /**
        * get an array/nodeList of child nodes
        *
        *   @param  {DOM node} node
        *   @return {Array}
        */
        getChildren: function( node ) {
            return node.children;
        },


        /**
        * create a node
        *
        *   @param  {String} tagName
        *   @return {DOM node}
        */
        createNode: function( tagName ) {
            return this.document.createElement(tagName);
        },


        /**
        * create a node with text content
        *
        *   @param  {String} tagName
        *   @param  {String} text
        *   @return {DOM node}
        */
        createNodeWithText: function( tagName, text ) {
            var node = this.document.createElement(tagName);
            node.innerHTML = text;
            return node;
        }



    };


    modules.url = {


        /**
         * creates DOM objects needed to resolve URLs
         */
        init: function() {
            //this._domParser = new DOMParser();
            this._domParser = modules.domUtils.getDOMParser();
            // do not use a head tag it does not work with IE9
            this._html = '<base id="base" href=""></base><a id="link" href=""></a>';
            this._nodes = this._domParser.parseFromString( this._html, 'text/html' );
            this._baseNode =  modules.domUtils.getElementById(this._nodes,'base');
            this._linkNode =  modules.domUtils.getElementById(this._nodes,'link');
        },


        /**
         * resolves url to absolute version using baseUrl
         *
         * @param  {String} url
         * @param  {String} baseUrl
         * @return {String}
         */
        resolve: function(url, baseUrl) {
            // use modern URL web API where we can
            if (modules.utils.isString(url) && modules.utils.isString(baseUrl) && url.indexOf('://') === -1) {
                // this try catch is required as IE has an URL object but no constuctor support
                // http://glennjones.net/articles/the-problem-with-window-url
                try {
                    var resolved = new URL(url, baseUrl).toString();
                    // deal with early Webkit not throwing an error - for Safari
                    if (resolved === '[object URL]') {
                        resolved = URI.resolve(baseUrl, url);
                    }
                    return resolved;
                } catch (e) {
                    // otherwise fallback to DOM
                    if (this._domParser === undefined) {
                        this.init();
                    }

                    // do not use setAttribute it does not work with IE9
                    this._baseNode.href = baseUrl;
                    this._linkNode.href = url;

                    // dont use getAttribute as it returns orginal value not resolved
                    return this._linkNode.href;
                }
            } else {
                if (modules.utils.isString(url)) {
                    return url;
                }
                return '';
            }
        },

    };


    /**
     * constructor
     * parses text to find just the date element of an ISO date/time string i.e. 2008-05-01
     *
     * @param  {String} dateString
     * @param  {String} format
     * @return {String}
     */
    modules.ISODate = function ( dateString, format ) {
        this.clear();

        this.format = (format)? format : 'auto'; // auto or W3C or RFC3339 or HTML5
        this.setFormatSep();

        // optional should be full iso date/time string
        if (arguments[0]) {
            this.parse(dateString, format);
        }
    };


    modules.ISODate.prototype = {


        /**
         * clear all states
         *
         */
        clear: function() {
            this.clearDate();
            this.clearTime();
            this.clearTimeZone();
            this.setAutoProfileState();
        },


        /**
         * clear date states
         *
         */
        clearDate: function() {
            this.dY = -1;
            this.dM = -1;
            this.dD = -1;
            this.dDDD = -1;
        },


        /**
         * clear time states
         *
         */
        clearTime: function() {
            this.tH = -1;
            this.tM = -1;
            this.tS = -1;
            this.tD = -1;
        },


        /**
         * clear timezone states
         *
         */
        clearTimeZone: function() {
            this.tzH = -1;
            this.tzM = -1;
            this.tzPN = '+';
            this.z = false;
        },


        /**
         * resets the auto profile state
         *
         */
        setAutoProfileState: function() {
            this.autoProfile = {
               sep: 'T',
               dsep: '-',
               tsep: ':',
               tzsep: ':',
               tzZulu: 'Z'
            };
        },


        /**
         * parses text to find ISO date/time string i.e. 2008-05-01T15:45:19Z
         *
         * @param  {String} dateString
         * @param  {String} format
         * @return {String}
         */
        parse: function( dateString, format ) {
            this.clear();

            var parts = [],
                tzArray = [],
                position = 0,
                datePart = '',
                timePart = '',
                timeZonePart = '';

            if (format) {
                this.format = format;
            }



            // discover date time separtor for auto profile
            // Set to 'T' by default
            if (dateString.indexOf('t') > -1) {
                this.autoProfile.sep = 't';
            }
            if (dateString.indexOf('z') > -1) {
                this.autoProfile.tzZulu = 'z';
            }
            if (dateString.indexOf('Z') > -1) {
                this.autoProfile.tzZulu = 'Z';
            }
            if (dateString.toUpperCase().indexOf('T') === -1) {
                this.autoProfile.sep = ' ';
            }


            dateString = dateString.toUpperCase().replace(' ','T');

            // break on 'T' divider or space
            if (dateString.indexOf('T') > -1) {
                parts = dateString.split('T');
                datePart = parts[0];
                timePart = parts[1];

                // zulu UTC
                if (timePart.indexOf( 'Z' ) > -1) {
                    this.z = true;
                }

                // timezone
                if (timePart.indexOf( '+' ) > -1 || timePart.indexOf( '-' ) > -1) {
                    tzArray = timePart.split( 'Z' ); // incase of incorrect use of Z
                    timePart = tzArray[0];
                    timeZonePart = tzArray[1];

                    // timezone
                    if (timePart.indexOf( '+' ) > -1 || timePart.indexOf( '-' ) > -1) {
                        position = 0;

                        if (timePart.indexOf( '+' ) > -1) {
                            position = timePart.indexOf( '+' );
                        } else {
                            position = timePart.indexOf( '-' );
                        }

                        timeZonePart = timePart.substring( position, timePart.length );
                        timePart = timePart.substring( 0, position );
                    }
                }

            } else {
                datePart = dateString;
            }

            if (datePart !== '') {
                this.parseDate( datePart );
                if (timePart !== '') {
                    this.parseTime( timePart );
                    if (timeZonePart !== '') {
                        this.parseTimeZone( timeZonePart );
                    }
                }
            }
            return this.toString( format );
        },


        /**
         * parses text to find just the date element of an ISO date/time string i.e. 2008-05-01
         *
         * @param  {String} dateString
         * @param  {String} format
         * @return {String}
         */
        parseDate: function( dateString, format ) {
            this.clearDate();

            var parts = [];

            // discover timezone separtor for auto profile // default is ':'
            if (dateString.indexOf('-') === -1) {
                this.autoProfile.tsep = '';
            }

            // YYYY-DDD
            parts = dateString.match( /(\d\d\d\d)-(\d\d\d)/ );
            if (parts) {
                if (parts[1]) {
                    this.dY = parts[1];
                }
                if (parts[2]) {
                    this.dDDD = parts[2];
                }
            }

            if (this.dDDD === -1) {
                // YYYY-MM-DD ie 2008-05-01 and YYYYMMDD ie 20080501
                parts = dateString.match( /(\d\d\d\d)?-?(\d\d)?-?(\d\d)?/ );
                if (parts[1]) {
                    this.dY = parts[1];
                }
                if (parts[2]) {
                    this.dM = parts[2];
                }
                if (parts[3]) {
                    this.dD = parts[3];
                }
            }
            return this.toString(format);
        },


        /**
         * parses text to find just the time element of an ISO date/time string i.e. 13:30:45
         *
         * @param  {String} timeString
         * @param  {String} format
         * @return {String}
         */
        parseTime: function( timeString, format ) {
            this.clearTime();
            var parts = [];

            // discover date separtor for auto profile // default is ':'
            if (timeString.indexOf(':') === -1) {
                this.autoProfile.tsep = '';
            }

            // finds timezone HH:MM:SS and HHMMSS  ie 13:30:45, 133045 and 13:30:45.0135
            parts = timeString.match( /(\d\d)?:?(\d\d)?:?(\d\d)?.?([0-9]+)?/ );
            if (parts[1]) {
                this.tH = parts[1];
            }
            if (parts[2]) {
                this.tM = parts[2];
            }
            if (parts[3]) {
                this.tS = parts[3];
            }
            if (parts[4]) {
                this.tD = parts[4];
            }
            return this.toTimeString(format);
        },


        /**
         * parses text to find just the time element of an ISO date/time string i.e. +08:00
         *
         * @param  {String} timeString
         * @param  {String} format
         * @return {String}
         */
        parseTimeZone: function( timeString, format ) {
            this.clearTimeZone();
            var parts = [];

            if (timeString.toLowerCase() === 'z') {
                this.z = true;
                // set case for z
                this.autoProfile.tzZulu = (timeString === 'z')? 'z' : 'Z';
            } else {

                // discover timezone separtor for auto profile // default is ':'
                if (timeString.indexOf(':') === -1) {
                    this.autoProfile.tzsep = '';
                }

                // finds timezone +HH:MM and +HHMM  ie +13:30 and +1330
                parts = timeString.match( /([\-\+]{1})?(\d\d)?:?(\d\d)?/ );
                if (parts[1]) {
                    this.tzPN = parts[1];
                }
                if (parts[2]) {
                    this.tzH = parts[2];
                }
                if (parts[3]) {
                    this.tzM = parts[3];
                }


            }
            this.tzZulu = 'z';
            return this.toTimeString( format );
        },


        /**
         * returns ISO date/time string in W3C Note, RFC 3339, HTML5, or auto profile
         *
         * @param  {String} format
         * @return {String}
         */
        toString: function( format ) {
            var output = '';

            if (format) {
                this.format = format;
            }
            this.setFormatSep();

            if (this.dY  > -1) {
                output = this.dY;
                if (this.dM > 0 && this.dM < 13) {
                    output += this.dsep + this.dM;
                    if (this.dD > 0 && this.dD < 32) {
                        output += this.dsep + this.dD;
                        if (this.tH > -1 && this.tH < 25) {
                            output += this.sep + this.toTimeString( format );
                        }
                    }
                }
                if (this.dDDD > -1) {
                    output += this.dsep + this.dDDD;
                }
            } else if (this.tH > -1) {
                output += this.toTimeString( format );
            }

            return output;
        },


        /**
         * returns just the time string element of an ISO date/time
         * in W3C Note, RFC 3339, HTML5, or auto profile
         *
         * @param  {String} format
         * @return {String}
         */
        toTimeString: function( format ) {
            var out = '';

            if (format) {
                this.format = format;
            }
            this.setFormatSep();

            // time can only be created with a full date
            if (this.tH) {
                if (this.tH > -1 && this.tH < 25) {
                    out += this.tH;
                    if (this.tM > -1 && this.tM < 61) {
                        out += this.tsep + this.tM;
                        if (this.tS > -1 && this.tS < 61) {
                            out += this.tsep + this.tS;
                            if (this.tD > -1) {
                                out += '.' + this.tD;
                            }
                        }
                    }



                    // time zone offset
                    if (this.z) {
                        out += this.tzZulu;
                    } else if (this.tzH && this.tzH > -1 && this.tzH < 25) {
                        out += this.tzPN + this.tzH;
                        if (this.tzM > -1 && this.tzM < 61) {
                            out += this.tzsep + this.tzM;
                        }
                    }
                }
            }
            return out;
        },


        /**
         * set the current profile to W3C Note, RFC 3339, HTML5, or auto profile
         *
         */
        setFormatSep: function() {
            switch ( this.format.toLowerCase() ) {
                case 'rfc3339':
                    this.sep = 'T';
                    this.dsep = '';
                    this.tsep = '';
                    this.tzsep = '';
                    this.tzZulu = 'Z';
                    break;
                case 'w3c':
                    this.sep = 'T';
                    this.dsep = '-';
                    this.tsep = ':';
                    this.tzsep = ':';
                    this.tzZulu = 'Z';
                    break;
                case 'html5':
                    this.sep = ' ';
                    this.dsep = '-';
                    this.tsep = ':';
                    this.tzsep = ':';
                    this.tzZulu = 'Z';
                    break;
                default:
                    // auto - defined by format of input string
                    this.sep = this.autoProfile.sep;
                    this.dsep = this.autoProfile.dsep;
                    this.tsep = this.autoProfile.tsep;
                    this.tzsep = this.autoProfile.tzsep;
                    this.tzZulu = this.autoProfile.tzZulu;
            }
        },


        /**
         * does current data contain a full date i.e. 2015-03-23
         *
         * @return {Boolean}
         */
        hasFullDate: function() {
            return (this.dY !== -1 && this.dM !== -1 && this.dD !== -1);
        },


        /**
         * does current data contain a minimum date which is just a year number i.e. 2015
         *
         * @return {Boolean}
         */
        hasDate: function() {
            return (this.dY !== -1);
        },


        /**
         * does current data contain a minimum time which is just a hour number i.e. 13
         *
         * @return {Boolean}
         */
        hasTime: function() {
            return (this.tH !== -1);
        },

        /**
         * does current data contain a minimum timezone i.e. -1 || +1 || z
         *
         * @return {Boolean}
         */
        hasTimeZone: function() {
            return (this.tzH !== -1);
        }

    };

    modules.ISODate.prototype.constructor = modules.ISODate;


    modules.dates = {


        /**
         * does text contain am
         *
         * @param  {String} text
         * @return {Boolean}
         */
        hasAM: function( text ) {
            text = text.toLowerCase();
            return (text.indexOf('am') > -1 || text.indexOf('a.m.') > -1);
        },


        /**
         * does text contain pm
         *
         * @param  {String} text
         * @return {Boolean}
         */
        hasPM: function( text ) {
            text = text.toLowerCase();
            return (text.indexOf('pm') > -1 || text.indexOf('p.m.') > -1);
        },


        /**
         * remove am and pm from text and return it
         *
         * @param  {String} text
         * @return {String}
         */
        removeAMPM: function( text ) {
            return text.replace('pm', '').replace('p.m.', '').replace('am', '').replace('a.m.', '');
        },


       /**
         * simple test of whether ISO date string is a duration  i.e.  PY17M or PW12
         *
         * @param  {String} text
         * @return {Boolean}
         */
        isDuration: function( text ) {
            if (modules.utils.isString( text )) {
                text = text.toLowerCase();
                if (modules.utils.startWith(text, 'p') ) {
                    return true;
                }
            }
            return false;
        },


       /**
         * is text a time or timezone
         * i.e. HH-MM-SS or z+-HH-MM-SS 08:43 | 15:23:00:0567 | 10:34pm | 10:34 p.m. | +01:00:00 | -02:00 | z15:00 | 0843
         *
         * @param  {String} text
         * @return {Boolean}
         */
        isTime: function( text ) {
            if (modules.utils.isString(text)) {
                text = text.toLowerCase();
                text = modules.utils.trim( text );
                // start with timezone char
                if ( text.match(':') && ( modules.utils.startWith(text, 'z') || modules.utils.startWith(text, '-')  || modules.utils.startWith(text, '+') )) {
                    return true;
                }
                // has ante meridiem or post meridiem
                if ( text.match(/^[0-9]/) &&
                    ( this.hasAM(text) || this.hasPM(text) )) {
                    return true;
                }
                // contains time delimiter but not datetime delimiter
                if ( text.match(':') && !text.match(/t|\s/) ) {
                    return true;
                }

                // if it's a number of 2, 4 or 6 chars
                if (modules.utils.isNumber(text)) {
                    if (text.length === 2 || text.length === 4 || text.length === 6) {
                        return true;
                    }
                }
            }
            return false;
        },


        /**
         * parses a time from text and returns 24hr time string
         * i.e. 5:34am = 05:34:00 and 1:52:04p.m. = 13:52:04
         *
         * @param  {String} text
         * @return {String}
         */
        parseAmPmTime: function( text ) {
            var out = text,
                times = [];

            // if the string has a text : or am or pm
            if (modules.utils.isString(out)) {
                //text = text.toLowerCase();
                text = text.replace(/[ ]+/g, '');

                if (text.match(':') || this.hasAM(text) || this.hasPM(text)) {

                    if (text.match(':')) {
                        times = text.split(':');
                    } else {
                        // single number text i.e. 5pm
                        times[0] = text;
                        times[0] = this.removeAMPM(times[0]);
                    }

                    // change pm hours to 24hr number
                    if (this.hasPM(text)) {
                        if (times[0] < 12) {
                            times[0] = parseInt(times[0], 10) + 12;
                        }
                    }

                    // add leading zero's where needed
                    if (times[0] && times[0].length === 1) {
                        times[0] = '0' + times[0];
                    }

                    // rejoin text elements together
                    if (times[0]) {
                        text = times.join(':');
                    }
                }
            }

            // remove am/pm strings
            return this.removeAMPM(text);
        },


       /**
         * overlays a time on a date to return the union of the two
         *
         * @param  {String} date
         * @param  {String} time
         * @param  {String} format ( Modules.ISODate profile format )
         * @return {Object} Modules.ISODate
         */
        dateTimeUnion: function(date, time, format) {
            var isodate = new modules.ISODate(date, format),
                isotime = new modules.ISODate();

            isotime.parseTime(this.parseAmPmTime(time), format);
            if (isodate.hasFullDate() && isotime.hasTime()) {
                isodate.tH = isotime.tH;
                isodate.tM = isotime.tM;
                isodate.tS = isotime.tS;
                isodate.tD = isotime.tD;
                return isodate;
            }
            if (isodate.hasFullDate()) {
                return isodate;
            }
            return new modules.ISODate();
        },


       /**
         * concatenate an array of date and time text fragments to create an ISODate object
         * used for microformat value and value-title rules
         *
         * @param  {Array} arr ( Array of Strings )
         * @param  {String} format ( Modules.ISODate profile format )
         * @return {Object} Modules.ISODate
         */
        concatFragments: function (arr, format) {
            var out = new modules.ISODate(),
                i = 0,
                value = '';

            // if the fragment already contains a full date just return it once
            if (arr[0].toUpperCase().match('T')) {
                return new modules.ISODate(arr[0], format);
            }
            for (i = 0; i < arr.length; i++) {
                value = arr[i];

                // date pattern
                if ( value.charAt(4) === '-' && out.hasFullDate() === false ) {
                    out.parseDate(value);
                }

                // time pattern
                if ( (value.indexOf(':') > -1 || modules.utils.isNumber( this.parseAmPmTime(value) )) && out.hasTime() === false ) {
                    // split time and timezone
                    var items = this.splitTimeAndZone(value);
                    value = items[0];

                    // parse any use of am/pm
                    value = this.parseAmPmTime(value);
                    out.parseTime(value);

                    // parse any timezone
                    if (items.length > 1) {
                         out.parseTimeZone(items[1], format);
                    }
                }

                // timezone pattern
                if (value.charAt(0) === '-' || value.charAt(0) === '+' || value.toUpperCase() === 'Z') {
                    if ( out.hasTimeZone() === false ) {
                        out.parseTimeZone(value);
                    }
                }

            }
            return out;
        },


       /**
         * parses text by splitting it into an array of time and timezone strings
         *
         * @param  {String} text
         * @return {Array} Modules.ISODate
         */
        splitTimeAndZone: function ( text ) {
           var out = [text],
               chars = ['-','+','z','Z'],
               i = chars.length;

            while (i--) {
              if (text.indexOf(chars[i]) > -1) {
                  out[0] = text.slice( 0, text.indexOf(chars[i]) );
                  out.push( text.slice( text.indexOf(chars[i]) ) );
                  break;
               }
            }
           return out;
        }

    };


    modules.text = {

        // normalised or whitespace or whitespacetrimmed
        textFormat: 'whitespacetrimmed',

        // block level tags, used to add line returns
        blockLevelTags: ['h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'p', 'hr', 'pre', 'table',
            'address', 'article', 'aside', 'blockquote', 'caption', 'col', 'colgroup', 'dd', 'div',
            'dt', 'dir', 'fieldset', 'figcaption', 'figure', 'footer', 'form',  'header', 'hgroup', 'hr',
            'li', 'map', 'menu', 'nav', 'optgroup', 'option', 'section', 'tbody', 'testarea',
            'tfoot', 'th', 'thead', 'tr', 'td', 'ul', 'ol', 'dl', 'details'],

        // tags to exclude
        excludeTags: ['noframe', 'noscript', 'template', 'script', 'style', 'frames', 'frameset'],


        /**
         * parses the text from the DOM Node
         *
         * @param  {DOM Node} node
         * @param  {String} textFormat
         * @return {String}
         */
        parse: function(doc, node, textFormat) {
            var out;
            this.textFormat = (textFormat)? textFormat : this.textFormat;
            if (this.textFormat === 'normalised') {
                out = this.walkTreeForText( node );
                if (out !== undefined) {
                    return this.normalise( doc, out );
                }
                return '';
            }
            return this.formatText( doc, modules.domUtils.textContent(node), this.textFormat );
        },


        /**
         * parses the text from a html string
         *
         * @param  {DOM Document} doc
         * @param  {String} text
         * @param  {String} textFormat
         * @return {String}
         */
        parseText: function( doc, text, textFormat ) {
           var node = modules.domUtils.createNodeWithText( 'div', text );
           return this.parse( doc, node, textFormat );
        },


        /**
         * parses the text from a html string - only for whitespace or whitespacetrimmed formats
         *
         * @param  {String} text
         * @param  {String} textFormat
         * @return {String}
         */
        formatText: function( doc, text, textFormat ) {
           this.textFormat = (textFormat)? textFormat : this.textFormat;
           if (text) {
              var out = '',
                  regex = /(<([^>]+)>)/ig;

              out = text.replace(regex, '');
              if (this.textFormat === 'whitespacetrimmed') {
                 out = modules.utils.trimWhitespace( out );
              }

              //return entities.decode( out, 2 );
              return modules.domUtils.decodeEntities( doc, out );
           }
           return '';
        },


        /**
         * normalises whitespace in given text
         *
         * @param  {String} text
         * @return {String}
         */
        normalise: function( doc, text ) {
            text = text.replace( /&nbsp;/g, ' ') ;    // exchanges html entity for space into space char
            text = modules.utils.collapseWhiteSpace( text );     // removes linefeeds, tabs and addtional spaces
            text = modules.domUtils.decodeEntities( doc, text );  // decode HTML entities
            text = text.replace( '–', '-' );          // correct dash decoding
            return modules.utils.trim( text );
        },


        /**
         * walks DOM tree parsing the text from DOM Nodes
         *
         * @param  {DOM Node} node
         * @return {String}
         */
        walkTreeForText: function( node ) {
            var out = '',
                j = 0;

            if (node.tagName && this.excludeTags.indexOf( node.tagName.toLowerCase() ) > -1) {
                return out;
            }

            // if node is a text node get its text
            if (node.nodeType && node.nodeType === 3) {
                out += modules.domUtils.getElementText( node );
            }

            // get the text of the child nodes
            if (node.childNodes && node.childNodes.length > 0) {
                for (j = 0; j < node.childNodes.length; j++) {
                    var text = this.walkTreeForText( node.childNodes[j] );
                    if (text !== undefined) {
                        out += text;
                    }
                }
            }

            // if it's a block level tag add an additional space at the end
            if (node.tagName && this.blockLevelTags.indexOf( node.tagName.toLowerCase() ) !== -1) {
                out += ' ';
            }

            return (out === '')? undefined : out ;
        }

    };


    modules.html = {

        // elements which are self-closing
        selfClosingElt: ['area', 'base', 'br', 'col', 'hr', 'img', 'input', 'link', 'meta', 'param', 'command', 'keygen', 'source'],


        /**
         * parse the html string from DOM Node
         *
         * @param  {DOM Node} node
         * @return {String}
         */
        parse: function( node ) {
            var out = '',
                j = 0;

            // we do not want the outer container
            if (node.childNodes && node.childNodes.length > 0) {
                for (j = 0; j < node.childNodes.length; j++) {
                    var text = this.walkTreeForHtml( node.childNodes[j] );
                    if (text !== undefined) {
                        out += text;
                    }
                }
            }

            return out;
        },


        /**
         * walks the DOM tree parsing the html string from the nodes
         *
         * @param  {DOM Document} doc
         * @param  {DOM Node} node
         * @return {String}
         */
        walkTreeForHtml: function( node ) {
            var out = '',
                j = 0;

            // if node is a text node get its text
            if (node.nodeType && node.nodeType === 3) {
                out += modules.domUtils.getElementText( node );
            }


            // exclude text which has been added with include pattern  -
            if (node.nodeType && node.nodeType === 1 && modules.domUtils.hasAttribute(node, 'data-include') === false) {

                // begin tag
                out += '<' + node.tagName.toLowerCase();

                // add attributes
                var attrs = modules.domUtils.getOrderedAttributes(node);
                for (j = 0; j < attrs.length; j++) {
                    out += ' ' + attrs[j].name +  '=' + '"' + attrs[j].value + '"';
                }

                if (this.selfClosingElt.indexOf(node.tagName.toLowerCase()) === -1) {
                    out += '>';
                }

                // get the text of the child nodes
                if (node.childNodes && node.childNodes.length > 0) {

                    for (j = 0; j < node.childNodes.length; j++) {
                        var text = this.walkTreeForHtml( node.childNodes[j] );
                        if (text !== undefined) {
                            out += text;
                        }
                    }
                }

                // end tag
                if (this.selfClosingElt.indexOf(node.tagName.toLowerCase()) > -1) {
                    out += ' />';
                } else {
                    out += '</' + node.tagName.toLowerCase() + '>';
                }
            }

            return (out === '')? undefined : out;
        }


    };


    modules.maps['h-adr'] = {
        root: 'adr',
        name: 'h-adr',
        properties: {
            'post-office-box': {},
            'street-address': {},
            'extended-address': {},
            'locality': {},
            'region': {},
            'postal-code': {},
            'country-name': {}
        }
    };


    modules.maps['h-card'] =  {
        root: 'vcard',
        name: 'h-card',
        properties: {
            'fn': {
                'map': 'p-name'
            },
            'adr': {
                'map': 'p-adr',
                'uf': ['h-adr']
            },
            'agent': {
                'uf': ['h-card']
            },
            'bday': {
                'map': 'dt-bday'
            },
            'class': {},
            'category': {
                'map': 'p-category',
                'relAlt': ['tag']
            },
            'email': {
                'map': 'u-email'
            },
            'geo': {
                'map': 'p-geo',
                'uf': ['h-geo']
            },
            'key': {
                'map': 'u-key'
            },
            'label': {},
            'logo': {
                'map': 'u-logo'
            },
            'mailer': {},
            'honorific-prefix': {},
            'given-name': {},
            'additional-name': {},
            'family-name': {},
            'honorific-suffix': {},
            'nickname': {},
            'note': {}, // could be html i.e. e-note
            'org': {},
            'p-organization-name': {},
            'p-organization-unit': {},
            'photo': {
                'map': 'u-photo'
            },
            'rev': {
                'map': 'dt-rev'
            },
            'role': {},
            'sequence': {},
            'sort-string': {},
            'sound': {
                'map': 'u-sound'
            },
            'title': {
                'map': 'p-job-title'
            },
            'tel': {},
            'tz': {},
            'uid': {
                'map': 'u-uid'
            },
            'url': {
                'map': 'u-url'
            }
        }
    };


    modules.maps['h-entry'] = {
        root: 'hentry',
        name: 'h-entry',
        properties: {
            'entry-title': {
                'map': 'p-name'
            },
            'entry-summary': {
                'map': 'p-summary'
            },
            'entry-content': {
                'map': 'e-content'
            },
            'published': {
                'map': 'dt-published'
            },
            'updated': {
                'map': 'dt-updated'
            },
            'author': {
                'uf': ['h-card']
            },
            'category': {
                'map': 'p-category',
                'relAlt': ['tag']
            },
            'geo': {
                'map': 'p-geo',
                'uf': ['h-geo']
            },
            'latitude': {},
            'longitude': {},
            'url': {
                'map': 'u-url',
                'relAlt': ['bookmark']
            }
        }
    };


    modules.maps['h-event'] = {
        root: 'vevent',
        name: 'h-event',
        properties: {
            'summary': {
                'map': 'p-name'
            },
            'dtstart': {
                'map': 'dt-start'
            },
            'dtend': {
                'map': 'dt-end'
            },
            'description': {},
            'url': {
                'map': 'u-url'
            },
            'category': {
                'map': 'p-category',
                'relAlt': ['tag']
            },
            'location': {
                'uf': ['h-card']
            },
            'geo': {
                'uf': ['h-geo']
            },
            'latitude': {},
            'longitude': {},
            'duration': {
                'map': 'dt-duration'
            },
            'contact': {
                'uf': ['h-card']
            },
            'organizer': {
                'uf': ['h-card']},
            'attendee': {
                'uf': ['h-card']},
            'uid': {
                'map': 'u-uid'
            },
            'attach': {
                'map': 'u-attach'
            },
            'status': {},
            'rdate': {},
            'rrule': {}
        }
    };


    modules.maps['h-feed'] = {
        root: 'hfeed',
        name: 'h-feed',
        properties: {
            'category': {
                'map': 'p-category',
                'relAlt': ['tag']
            },
            'summary': {
                'map': 'p-summary'
            },
            'author': {
                'uf': ['h-card']
            },
            'url': {
                'map': 'u-url'
            },
            'photo': {
                'map': 'u-photo'
            },
        }
    };


    modules.maps['h-geo'] = {
        root: 'geo',
        name: 'h-geo',
        properties: {
            'latitude': {},
            'longitude': {}
        }
    };


    modules.maps['h-item'] = {
        root: 'item',
        name: 'h-item',
        subTree: false,
        properties: {
            'fn': {
                'map': 'p-name'
            },
            'url': {
                'map': 'u-url'
            },
            'photo': {
                'map': 'u-photo'
            }
        }
    };


    modules.maps['h-listing'] = {
            root: 'hlisting',
            name: 'h-listing',
            properties: {
                'version': {},
                'lister': {
                    'uf': ['h-card']
                },
                'dtlisted': {
                    'map': 'dt-listed'
                },
                'dtexpired': {
                    'map': 'dt-expired'
                },
                'location': {},
                'price': {},
                'item': {
                    'uf': ['h-card','a-adr','h-geo']
                },
                'summary': {
                    'map': 'p-name'
                },
                'description': {
                    'map': 'e-description'
                },
                'listing': {}
            }
        };


    modules.maps['h-news'] = {
            root: 'hnews',
            name: 'h-news',
            properties: {
                'entry': {
                    'uf': ['h-entry']
                },
                'geo': {
                    'uf': ['h-geo']
                },
                'latitude': {},
                'longitude': {},
                'source-org': {
                    'uf': ['h-card']
                },
                'dateline': {
                    'uf': ['h-card']
                },
                'item-license': {
                    'map': 'u-item-license'
                },
                'principles': {
                    'map': 'u-principles',
                    'relAlt': ['principles']
                }
            }
        };


    modules.maps['h-org'] = {
        root: 'h-x-org',  // drop this from v1 as it causes issue with fn org hcard pattern
        name: 'h-org',
        childStructure: true,
        properties: {
            'organization-name': {},
            'organization-unit': {}
        }
    };


    modules.maps['h-product'] = {
            root: 'hproduct',
            name: 'h-product',
            properties: {
                'brand': {
                    'uf': ['h-card']
                },
                'category': {
                    'map': 'p-category',
                    'relAlt': ['tag']
                },
                'price': {},
                'description': {
                    'map': 'e-description'
                },
                'fn': {
                    'map': 'p-name'
                },
                'photo': {
                    'map': 'u-photo'
                },
                'url': {
                    'map': 'u-url'
                },
                'review': {
                    'uf': ['h-review', 'h-review-aggregate']
                },
                'listing': {
                    'uf': ['h-listing']
                },
                'identifier': {
                    'map': 'u-identifier'
                }
            }
        };


    modules.maps['h-recipe'] = {
            root: 'hrecipe',
            name: 'h-recipe',
            properties: {
                'fn': {
                    'map': 'p-name'
                },
                'ingredient': {
                    'map': 'e-ingredient'
                },
                'yield': {},
                'instructions': {
                    'map': 'e-instructions'
                },
                'duration': {
                    'map': 'dt-duration'
                },
                'photo': {
                    'map': 'u-photo'
                },
                'summary': {},
                'author': {
                    'uf': ['h-card']
                },
                'published': {
                    'map': 'dt-published'
                },
                'nutrition': {},
                'category': {
                    'map': 'p-category',
                    'relAlt': ['tag']
                },
            }
        };


    modules.maps['h-resume'] = {
        root: 'hresume',
        name: 'h-resume',
        properties: {
            'summary': {},
            'contact': {
                'uf': ['h-card']
            },
            'education': {
                'uf': ['h-card', 'h-event']
            },
            'experience': {
                'uf': ['h-card', 'h-event']
            },
            'skill': {},
            'affiliation': {
                'uf': ['h-card']
            }
        }
    };


    modules.maps['h-review-aggregate'] = {
        root: 'hreview-aggregate',
        name: 'h-review-aggregate',
        properties: {
            'summary': {
                'map': 'p-name'
            },
            'item': {
                'map': 'p-item',
                'uf': ['h-item', 'h-geo', 'h-adr', 'h-card', 'h-event', 'h-product']
            },
            'rating': {},
            'average': {},
            'best': {},
            'worst': {},
            'count': {},
            'votes': {},
            'category': {
                'map': 'p-category',
                'relAlt': ['tag']
            },
            'url': {
                'map': 'u-url',
                'relAlt': ['self', 'bookmark']
            }
        }
    };


    modules.maps['h-review'] = {
        root: 'hreview',
        name: 'h-review',
        properties: {
            'summary': {
                'map': 'p-name'
            },
            'description': {
                'map': 'e-description'
            },
            'item': {
                'map': 'p-item',
                'uf': ['h-item', 'h-geo', 'h-adr', 'h-card', 'h-event', 'h-product']
            },
            'reviewer': {
                'uf': ['h-card']
            },
            'dtreviewer': {
                'map': 'dt-reviewer'
            },
            'rating': {},
            'best': {},
            'worst': {},
            'category': {
                'map': 'p-category',
                'relAlt': ['tag']
            },
            'url': {
                'map': 'u-url',
                'relAlt': ['self', 'bookmark']
            }
        }
    };


    modules.rels = {
        // xfn
        'friend': [ 'yes','external'],
        'acquaintance': [ 'yes','external'],
        'contact': [ 'yes','external'],
        'met': [ 'yes','external'],
        'co-worker': [ 'yes','external'],
        'colleague': [ 'yes','external'],
        'co-resident': [ 'yes','external'],
        'neighbor': [ 'yes','external'],
        'child': [ 'yes','external'],
        'parent': [ 'yes','external'],
        'sibling': [ 'yes','external'],
        'spouse': [ 'yes','external'],
        'kin': [ 'yes','external'],
        'muse': [ 'yes','external'],
        'crush': [ 'yes','external'],
        'date': [ 'yes','external'],
        'sweetheart': [ 'yes','external'],
        'me': [ 'yes','external'],

        // other rel=*
        'license': [ 'yes','yes'],
        'nofollow': [ 'no','external'],
        'tag': [ 'no','yes'],
        'self': [ 'no','external'],
        'bookmark': [ 'no','external'],
        'author': [ 'no','external'],
        'home': [ 'no','external'],
        'directory': [ 'no','external'],
        'enclosure': [ 'no','external'],
        'pronunciation': [ 'no','external'],
        'payment': [ 'no','external'],
        'principles': [ 'no','external']

    };



    var External = {
        version: modules.version,
        livingStandard: modules.livingStandard
    };


    External.get = function(options) {
        var parser = new modules.Parser();
        addV1(parser, options);
        return parser.get( options );
    };


    External.getParent = function(node, options) {
        var parser = new modules.Parser();
        addV1(parser, options);
        return parser.getParent( node, options );
    };


    External.count = function(options) {
        var parser = new modules.Parser();
        addV1(parser, options);
        return parser.count( options );
    };


    External.isMicroformat = function( node, options ) {
        var parser = new modules.Parser();
        addV1(parser, options);
        return parser.isMicroformat( node, options );
    };


    External.hasMicroformats = function( node, options ) {
        var parser = new modules.Parser();
        addV1(parser, options);
        return parser.hasMicroformats( node, options );
    };


    function addV1(parser, options) {
        if (options && options.maps) {
            if (Array.isArray(options.maps)) {
                parser.add(options.maps);
            } else {
                parser.add([options.maps]);
            }
        }
    }


    return External;


}));
try {
    // mozilla jsm support
    Components.utils.importGlobalProperties(["URL"]);
} catch (e) {}
this.EXPORTED_SYMBOLS = ['Microformats'];
