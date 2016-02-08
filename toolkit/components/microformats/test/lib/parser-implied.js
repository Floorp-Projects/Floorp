/*!
	Parser implied
	All the functions that deal with microformats implied rules

	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
	Dependencies  dates.js, domutils.js, html.js, isodate,js, text.js, utilities.js, url.js
*/

var Modules = (function (modules) {

	// check parser module is loaded
	if(modules.Parser){

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
			if(this.options.impliedPropertiesByVersion === false){
				typeVersion = 'v2';
			}

			if(node && uf && uf.properties) {
				uf = this.impliedBackwardComp( node, uf, parentClasses );
				if(typeVersion === 'v2'){
					uf = this.impliedhFeedTitle( uf );
					uf = this.impliedName( node, uf );
					uf = this.impliedPhoto( node, uf );
					uf = this.impliedUrl( node, uf );
				}
				uf = this.impliedValue( node, uf, parentClasses );
				uf = this.impliedDate( uf );

				// TEMP: flagged while spec changes are put forward
				if(this.options.parseLatLonGeo === true){
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
				img.h-x[alt]										<img class="h-card" src="glenn.htm" alt="Glenn Jones"></a>
				area.h-x[alt] 										<area class="h-card" href="glenn.htm" alt="Glenn Jones"></area>
				abbr.h-x[title]										<abbr class="h-card" title="Glenn Jones"GJ</abbr>

				.h-x>img:only-child[alt]:not[.h-*]					<div class="h-card"><a src="glenn.htm" alt="Glenn Jones"></a></div>
				.h-x>area:only-child[alt]:not[.h-*] 				<div class="h-card"><area href="glenn.htm" alt="Glenn Jones"></area></div>
				.h-x>abbr:only-child[title] 						<div class="h-card"><abbr title="Glenn Jones">GJ</abbr></div>

				.h-x>:only-child>img:only-child[alt]:not[.h-*] 		<div class="h-card"><span><img src="jane.html" alt="Jane Doe"/></span></div>
				.h-x>:only-child>area:only-child[alt]:not[.h-*] 	<div class="h-card"><span><area href="jane.html" alt="Jane Doe"></area></span></div>
				.h-x>:only-child>abbr:only-child[title]				<div class="h-card"><span><abbr title="Jane Doe">JD</abbr></span></div>
			*/
			var name,
				value;

			if(!uf.properties.name) {
				value = this.getImpliedProperty(node, ['img', 'area', 'abbr'], this.getNameAttr);
				var textFormat = this.options.textFormat;
				// if no value for tags/properties use text
				if(!value) {
					name = [modules.text.parse(this.document, node, textFormat)];
				}else{
					name = [modules.text.parseText(this.document, value, textFormat)];
				}
				if(name && name[0] !== ''){
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
				img.h-x[src] 												<img class="h-card" alt="Jane Doe" src="jane.jpeg"/>
				object.h-x[data] 											<object class="h-card" data="jane.jpeg"/>Jane Doe</object>
				.h-x>img[src]:only-of-type:not[.h-*]						<div class="h-card"><img alt="Jane Doe" src="jane.jpeg"/></div>
				.h-x>object[data]:only-of-type:not[.h-*] 					<div class="h-card"><object data="jane.jpeg"/>Jane Doe</object></div>
				.h-x>:only-child>img[src]:only-of-type:not[.h-*] 			<div class="h-card"><span><img alt="Jane Doe" src="jane.jpeg"/></span></div>
				.h-x>:only-child>object[data]:only-of-type:not[.h-*] 		<div class="h-card"><span><object data="jane.jpeg"/>Jane Doe</object></span></div>
			*/
			var value;
			if(!uf.properties.photo) {
				value = this.getImpliedProperty(node, ['img', 'object'], this.getPhotoAttr);
				if(value) {
					// relative to absolute URL
					if(value && value !== '' && this.options.baseUrl !== '' && value.indexOf('://') === -1) {
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
				a.h-x[href]  							<a class="h-card" href="glenn.html">Glenn</a>
				area.h-x[href]  						<area class="h-card" href="glenn.html">Glenn</area>
				.h-x>a[href]:only-of-type:not[.h-*]  	<div class="h-card" ><a href="glenn.html">Glenn</a><p>...</p></div>
				.h-x>area[href]:only-of-type:not[.h-*]  <div class="h-card" ><area href="glenn.html">Glenn</area><p>...</p></div>
			*/
			var value;
			if(!uf.properties.url) {
				value = this.getImpliedProperty(node, ['a', 'area'], this.getURLAttr);
				if(value) {
					// relative to absolute URL
					if(value && value !== '' && this.options.baseUrl !== '' && value.indexOf('://') === -1) {
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
			if(uf.times.length > 0 && uf.dates.length > 0) {
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

			if(!value) {
				// i.e. .h-card>img:only-of-type:not(.h-card)
				descendant = modules.domUtils.getSingleDescendantOfType( node, tagList);
				if(descendant && this.hasHClass(descendant) === false){
					value = getAttrFunction(descendant);
				}
				if(node.children.length > 0 ){
					// i.e.  .h-card>:only-child>img:only-of-type:not(.h-card)
					child = modules.domUtils.getSingleDescendant(node);
					if(child && this.hasHClass(child) === false){
						descendant = modules.domUtils.getSingleDescendantOfType(child, tagList);
						if(descendant && this.hasHClass(descendant) === false){
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
			if(!value) {
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
			if(!value && modules.domUtils.hasAttributeValue(node, 'class', 'include') === false) {
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
			if(modules.domUtils.hasAttributeValue(node, 'class', 'include') === false){

				value = modules.domUtils.getAttrValFromTagList(node, ['a'], 'href');
				if(!value) {
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
		modules.Parser.prototype.impliedValue = function(node, uf, parentClasses){

			// intersection of implied name and implied value rules
			if(uf.properties.name) {
				if(uf.value && parentClasses.root.length > 0 && parentClasses.properties.length === 1){
					uf = this.getAltValue(uf, parentClasses.properties[0][0], 'p-name', uf.properties.name[0]);
				}
			}

			// intersection of implied URL and implied value rules
			if(uf.properties.url) {
				if(parentClasses && parentClasses.root.length === 1 && parentClasses.properties.length === 1){
					uf = this.getAltValue(uf, parentClasses.properties[0][0], 'u-url', uf.properties.url[0]);
				}
			}

			// apply alt value
			if(uf.altValue !== null){
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
		modules.Parser.prototype.getAltValue = function(uf, parentPropertyName, propertyName, value){
			if(uf.value && !uf.altValue){
				// first p-name of the h-* child
				if(modules.utils.startWith(parentPropertyName,'p-') && propertyName === 'p-name'){
					uf.altValue = {name: propertyName, value: value};
				}
				// if it's an e-* property element
				if(modules.utils.startWith(parentPropertyName,'e-') && modules.utils.startWith(propertyName,'e-')){
					uf.altValue = {name: propertyName, value: value};
				}
				// if it's an u-* property element
				if(modules.utils.startWith(parentPropertyName,'u-') && propertyName === 'u-url'){
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
		modules.Parser.prototype.impliedhFeedTitle = function( uf ){
			if(uf.type && uf.type.indexOf('h-feed') > -1){
				// has no name property
				if(uf.properties.name === undefined || uf.properties.name[0] === '' ){
					// use the text from the title tag
					var title = modules.domUtils.querySelector(this.document, 'title');
					if(title){
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
		modules.Parser.prototype.impliedGeo = function( uf ){
			var geoPair,
				parts,
				longitude,
				latitude,
				valid = true;

			if(uf.type && uf.type.indexOf('h-geo') > -1){

				// has no latitude or longitude property
				if(uf.properties.latitude === undefined || uf.properties.longitude === undefined ){

					geoPair = (uf.properties.name)? uf.properties.name[0] : null;
					geoPair = (!geoPair && uf.properties.value)? uf.properties.value : geoPair;

					if(geoPair){
						// allow for the use of a ';' as in microformats and also ',' as in Geo URL
						geoPair = geoPair.replace(';',',');

						// has sep char
						if(geoPair.indexOf(',') > -1 ){
							parts = geoPair.split(',');

							// only correct if we have two or more parts
							if(parts.length > 1){

								// latitude no value outside the range -90 or 90
								latitude = parseFloat( parts[0] );
								if(modules.utils.isNumber(latitude) && latitude > 90 || latitude < -90){
									valid = false;
								}

								// longitude no value outside the range -180 to 180
								longitude = parseFloat( parts[1] );
								if(modules.utils.isNumber(longitude) && longitude > 180 || longitude < -180){
									valid = false;
								}

								if(valid){
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
		modules.Parser.prototype.impliedBackwardComp = function(node, uf, parentClasses){

			// look for pattern in parent classes like "p-geo h-geo"
			// these are structures built from backwards compat parsing of geo
			if(parentClasses.root.length === 1 && parentClasses.properties.length === 1) {
				if(parentClasses.root[0].replace('h-','') === this.removePropPrefix(parentClasses.properties[0][0])) {

					// if microformat has no properties apply the impliedName rule to get value from containing node
					// this will get value from html such as <abbr class="geo" title="30.267991;-97.739568">Brighton</abbr>
					if( modules.utils.hasProperties(uf.properties) === false ){
						uf = this.impliedName( node, uf );
					}
				}
			}

			return uf;
		};



	}

	return modules;

} (Modules || {}));
