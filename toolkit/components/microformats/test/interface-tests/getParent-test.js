/*
Unit test for getParent
*/

assert = chai.assert;


describe('Microformat.getParent', function() {

        var HTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div>';
        var emptyExpected = {
            "items": [],
            "rels": {},
            "rel-urls": {}
        };
        var expected = {
            "items": [
                {
                    "type": [
                        "h-event"
                    ],
                    "properties": {
                        "name": [
                            "Pub"
                        ],
                        "start": [
                            "2015-07-01 17:30Z"
                        ]
                    }
                }
            ],
            "rels": {},
            "rel-urls": {}
        };
        var options = {'dateFormat': 'html5'};




   it('getParent with parent', function(){

       var  doc,
            node,
            span,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = HTML;
        doc.body.appendChild(node);
        span = doc.querySelector('.dt-start');

        result = Microformats.getParent(span,options);
        assert.deepEqual( result, expected );

   });



   it('getParent without parent', function(){

       var  doc,
            node,
            parser,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = HTML;
        doc.body.appendChild(node);

        result = Microformats.getParent(node,options);
        assert.deepEqual( result, emptyExpected );

   });


   it('getParent found with option.filters', function(){

       var  doc,
            node,
            span,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = HTML;
        doc.body.appendChild(node);
        span = doc.querySelector('.dt-start');

        result = Microformats.getParent( span, {'filters': ['h-event'], 'dateFormat': 'html5'} );
        assert.deepEqual( result, expected );

   });


   it('getParent not found with option.filters', function(){

       var  doc,
            node,
            span,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = HTML;
        doc.body.appendChild(node);
        span = doc.querySelector('.dt-start');

        result = Microformats.getParent( span, {'filters': ['h-card'], 'dateFormat': 'html5'} );
        assert.deepEqual( result, emptyExpected );

   });


   it('getParent use option.filters to up through h-*', function(){

       var  doc,
            node,
            span,
            result;

        var altHTML = '<div class="h-entry"><h1 class="p-name">test</h1><div class="e-content">this</div><a class="p-author h-card" href="http://glennjones.net"><span class="p-name">Glenn Jones</span></a><span class="dt-publish">2015-07-01t17:30z</span></div>';
        var altExpected = {
            "items": [
                {
                    "type": [
                        "h-entry"
                    ],
                    "properties": {
                        "name": [
                            "test"
                        ],
                        "content": [
                            {
                                "value": "this",
                                "html": "this"
                            }
                        ],
                        "author": [
                            {
                                "value": "Glenn Jones",
                                "type": [
                                    "h-card"
                                ],
                                "properties": {
                                    "name": [
                                        "Glenn Jones"
                                    ],
                                    "url": [
                                        "http://glennjones.net"
                                    ]
                                }
                            }
                        ],
                        "publish": [
                            "2015-07-01 17:30Z"
                        ]
                    }
                }
            ],
            "rels": {},
            "rel-urls": {}
        };


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);
        span = doc.querySelector('.h-card .p-name');

        result = Microformats.getParent( span, {'filters': ['h-entry'], 'dateFormat': 'html5'} );
        assert.deepEqual( result, altExpected );

   });


   it('getParent stop at first h-* parent', function(){

       var  doc,
            node,
            span,
            result;

        var altHTML = '<div class="h-entry"><h1 class="p-name">test</h1><div class="e-content">this</div><a class="p-author h-card" href="http://glennjones.net"><span class="p-name">Glenn Jones</span></a><span class="dt-publish">2015-07-01t17:30z</span></div>';
        var altExpected = {
            "items": [
                    {
                        "type": [
                            "h-card"
                        ],
                        "properties": {
                            "name": [
                                "Glenn Jones"
                            ],
                            "url": [
                                "http://glennjones.net"
                            ]
                        }
                    }
            ],
            "rels": {},
            "rel-urls": {}
        };


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);
        span = doc.querySelector('.h-card .p-name');

        result = Microformats.getParent( span, options );
        assert.deepEqual( result, altExpected );

   });


});
