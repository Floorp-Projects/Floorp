/*
		Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
		MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt  
*/

var Modules = (function (modules) {
		
	modules.maps = (modules.maps)? modules.maps : {};

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

		return modules;

} (Modules || {}));

