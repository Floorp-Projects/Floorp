/*
Unit test for hasMicroformat
*/

assert = chai.assert;


describe('Microformat.hasMicroformats', function() {

   it('true - v2 on node', function(){

       var  doc,
            node;

        var html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' );

        assert.isTrue( Microformats.hasMicroformats( node ) );

   });


   it('true - v1 on node', function(){

       var  doc,
            node;

        var html = '<a class="vcard" href="http://glennjones.net"><span class="fn">Glenn</span></a>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' );

        assert.isTrue( Microformats.hasMicroformats( node ) );

   });


   it('true - v2 filter on node', function(){

       var  doc,
            node;

        var html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' );

        assert.isTrue( Microformats.hasMicroformats( node, {'filters': ['h-card']} ) );

   });


   it('true - v1 filter on node', function(){

       var  doc,
            node;

        var html = '<a class="vcard" href="http://glennjones.net"><span class="fn">Glenn</span></a>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' );

        assert.isTrue( Microformats.hasMicroformats( node, {'filters': ['h-card']} ) );

   });


   it('false - v2 filter on node', function(){

       var  doc,
            node;

        var html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' );

        assert.isFalse( Microformats.hasMicroformats( node, {'filters': ['h-entry']} ) );

   });



   it('false - property', function(){

       var  doc,
            node,
            parser;

        var html = '<span class="p-name">Glenn</span>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'span' );

        assert.isFalse( Microformats.hasMicroformats( node ) );

   });


   it('false - no class', function(){

       var  doc,
            node,
            parser;

        var html = '<span>Glenn</span>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'span' );

        assert.isFalse( Microformats.hasMicroformats( node ) );

   });


   it('false - no node', function(){
        assert.isFalse( Microformats.hasMicroformats( ) );
   });


   it('false - undefined node', function(){
        assert.isFalse( Microformats.hasMicroformats( undefined ) );
   });


   it('true - child', function(){

       var  doc,
            node;

        var html = '<section><div><a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a></div></section>';

        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;

        assert.isTrue( Microformats.hasMicroformats( node ) );

   });



   it('true - document', function(){

       var  doc,
            node;

        var html = '<html><head></head><body><section><div><a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a></div></section></body></html>';

        var dom = new DOMParser();
        doc = dom.parseFromString( html, 'text/html' );

        assert.isTrue( Microformats.hasMicroformats( doc ) );

   });





 });
