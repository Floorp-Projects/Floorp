/*
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt  
*/

var Modules = (function (modules) {
	
	modules.maps = (modules.maps)? modules.maps : {};

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

	return modules;

} (Modules || {}));
