/*
Unit test for get
*/

assert = chai.assert;


describe('Microformat.get', function() {


    var expected = {
        'items': [{
            'type': ['h-card'],
            'properties': {
                'name': ['Glenn Jones'],
                'url': ['http://glennjones.net']
            }
        }],
        'rels': {
            'bookmark': ['http://glennjones.net'],
            'alternate': ['http://example.com/fr'],
            'home': ['http://example.com/fr']
        },
        'rel-urls': {
            'http://glennjones.net': {
                'text': 'Glenn Jones',
                'rels': ['bookmark']
            },
            'http://example.com/fr': {
                'media': 'handheld',
                'hreflang': 'fr',
                'text': 'French mobile homepage',
                'rels': ['alternate', 'home']
            }
        }
    },
    html = '<div class="h-card"><a class="p-name u-url" rel="bookmark" href="http://glennjones.net">Glenn Jones</a></div><a rel="alternate home" href="http://example.com/fr" media="handheld" hreflang="fr">French mobile homepage</a>';





   it('get - no options.node parse this document', function(){
        var result;

        result = Microformats.get({});
        assert.deepEqual( result.items, [] );
   });


   it('get - standard', function(){

        var doc,
            node,
            options,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = html;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'baseUrl': 'http://example.com',
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, expected );

   });


   it('get - doc pass to node', function(){

       var  doc,
            node,
            options,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = html;
        doc.body.appendChild(node);

        options ={
            'node': doc,
            'baseUrl': 'http://example.com',
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, expected );

   });


    it('get - pass base tag', function(){

       var  doc,
            node,
            options,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = html + '<base href="http://example.com">';
        doc.body.appendChild(node);

        options ={
            'node': node,
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, expected );

   });


   it('get - pass no document', function(){

       var  doc,
            node,
            options,
            parser,
            result;

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = html + '<base href="http://example.com">';
        doc.body.appendChild(node);

        options ={
            'node': doc,
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, expected );

   });


   it('get - textFormat: normalised', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<a class="h-card" href="http://glennjones.net">\n';
        altHTML += '     <span class="p-given-name">Glenn</span>\n';
        altHTML += '     <span class="p-family-name">Jones</span>\n';
        altHTML += '</a>\n';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'textFormat': 'normalised',
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.equal( result.items[0].properties.name[0], 'Glenn Jones' );

   });


   it('get - textFormat: whitespace', function(){

       var  doc,
            node,
            options,
            parser,
            result;

        var altHTML = '<a class="h-card" href="http://glennjones.net">\n';
        altHTML += '     <span class="p-given-name">Glenn</span>\n';
        altHTML += '     <span class="p-family-name">Jones</span>\n';
        altHTML += '</a>\n';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'textFormat': 'whitespace',
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.equal( result.items[0].properties.name[0], '\n     Glenn\n     Jones\n' );

   });



   it('get - textFormat: whitespacetrimmed', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<a class="h-card" href="http://glennjones.net">\n';
        altHTML += '     <span class="p-given-name">Glenn</span>\n';
        altHTML += '     <span class="p-family-name">Jones</span>\n';
        altHTML += '</a>\n';


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'textFormat': 'whitespacetrimmed',
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.equal( result.items[0].properties.name[0], 'Glenn\n     Jones' );

   });


    it('get - dateFormat: auto', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'dateFormat': 'auto'
        };

        result = Microformats.get(options);
        assert.equal( result.items[0].properties.start[0], '2015-07-01t17:30z' );

   });


   it('get - dateFormat: w3c', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'dateFormat': 'w3c'
        };

        result = Microformats.get(options);
        assert.equal( result.items[0].properties.start[0], '2015-07-01T17:30Z' );

   });


   it('get - dateFormat: html5', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.equal( result.items[0].properties.start[0], '2015-07-01 17:30Z' );

   });



   it('get - dateFormat: rfc3339', function(){

       var  doc,
            node,
            options,

            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'dateFormat': 'rfc3339'
        };

        result = Microformats.get(options);
        assert.equal( result.items[0].properties.start[0], '20150701T1730Z' );

   });



   it('get - filters h-card', function(){

       var  doc,
            node,
            options,
            parser,
            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div><a class="h-card" href="http://glennjones.net">Glenn Jones</a>';
        var altExpected = {
            'items': [{
                'type': ['h-card'],
                'properties': {
                    'name': ['Glenn Jones'],
                    'url': ['http://glennjones.net']
                }
            }],
            'rels': {},
            'rel-urls': {}
        }


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        // filter as an array
        options ={
            'node': node,
            'filters': ['h-card'],
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, altExpected );

        // filter as an string
        options ={
            'node': node,
            'filters': 'h-card',
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, altExpected );
   });


   it('get - filters h-event', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div><a class="h-card" href="http://glennjones.net">Glenn Jones</a>';
        var altExpected = {
            'items': [{
                'type': ['h-event'],
                'properties': {
                    'name': ['Pub'],
                    'start': ['2015-07-01 17:30Z']
                }
            }],
            'rels': {},
            'rel-urls': {}
        }


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'filters': ['h-event'],
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, altExpected );

   });


    it('get - filters h-card and h-event', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div><a class="h-card" href="http://glennjones.net">Glenn Jones</a>';
        var altExpected = {
                'items': [{
                    'type': ['h-event'],
                    'properties': {
                        'name': ['Pub'],
                        'start': ['2015-07-01 17:30Z']
                    }
                },
                {
                    'type': ['h-card'],
                    'properties': {
                        'name': ['Glenn Jones'],
                        'url': ['http://glennjones.net']
                    }
                }],
                'rels': {},
                'rel-urls': {}
            }


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'filter': ['h-event'],
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, altExpected );

   });


   it('get - filters h-card no result', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<div class="h-event"><span class="p-name">Pub</span><span class="dt-start">2015-07-01t17:30z</span></div>';
        var altExpected = {
                'items': [],
                'rels': {},
                'rel-urls': {}
            }


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'filters': ['h-card'],
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, altExpected );

   });


    it('get - filters h-card match v1 format', function(){

       var  doc,
            node,
            options,
            parser,
            result;

        var altHTML = '<a class="vcard" href="http://glennjones.net"><span class="fn">Glenn Jones</span></a>';
        var altExpected = {
                'items': [{
                    'type': ['h-card'],
                    'properties': {
                        'name': ['Glenn Jones']
                    }
                }],
                'rels': {},
                'rel-urls': {}
            }


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'filter': ['h-card'],
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, altExpected );

   });


   it('get - add new v1 format through options', function(){

       var  doc,
            node,
            options,
            result;

        var altHTML = '<div class="hpayment">£<span class="amount">36.78</span></div>';
        var altExpected = {
                'items': [{
                    'type': ['h-payment'],
                    'properties': {
                        'amount': ['36.78']
                    }
                }],
                'rels': {},
                'rel-urls': {}
            };
        var v1Definition = {
                root: 'hpayment',
                name: 'h-payment',
                properties: {
                    'amount': {}
                }
            };


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = altHTML;
        doc.body.appendChild(node);

        options ={
            'node': node,
            'maps': v1Definition,
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, altExpected );

   });


    it('get - options.html', function(){

        var options,
            result;

        options ={
            'html': html,
            'baseUrl': 'http://example.com',
            'dateFormat': 'html5'
        };

        result = Microformats.get(options);
        assert.deepEqual( result, expected );

   });





});
