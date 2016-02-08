/*
Unit test for count
*/

assert = chai.assert;


describe('Microformat.count', function() {

   it('count', function(){

       var  doc,
            node,
            result;

        var html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a><a class="h-card" href="http://janedoe.net"><span class="p-name">Jane</span></a><a class="h-event" href="http://janedoe.net"><span class="p-name">Event</span><span class="dt-start">2015-07-01</span></a>';


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = html;
        doc.body.appendChild(node);

        options ={
            'node': node,
        };

        result = Microformats.count(options);
        assert.deepEqual( result, {'h-event': 1,'h-card': 2} );

   });


    it('count rels', function(){

       var  doc,
            node,
            result;

        var html = '<link href="http://glennjones.net/notes/atom" rel="notes alternate" title="Notes" type="application/atom+xml" /><a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a><a class="h-card" href="http://janedoe.net"><span class="p-name">Jane</span></a><a class="h-event" href="http://janedoe.net"><span class="p-name">Event</span><span class="dt-start">2015-07-01</span></a>';


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = html;
        doc.body.appendChild(node);

        options ={
            'node': node,
        };

        result = Microformats.count(options);
        assert.deepEqual( result, {'h-event': 1,'h-card': 2, 'rels': 1} );

   });


   it('count - no results', function(){

       var  doc,
            node,
            result;

        var html = '<span class="p-name">Jane</span>';


        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        node.innerHTML = html;
        doc.body.appendChild(node);

        options ={
            'node': node,
        };

        result = Microformats.count(options);
        assert.deepEqual( result, {} );

   });



   it('count - no options', function(){

       var result;

       result = Microformats.count({});
       assert.deepEqual( result, {} );

   });


   it('count - options.html', function(){

       var  options = {},
            result;

        options.html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a><a class="h-card" href="http://janedoe.net"><span class="p-name">Jane</span></a><a class="h-event" href="http://janedoe.net"><span class="p-name">Event</span><span class="dt-start">2015-07-01</span></a>';

        result = Microformats.count(options);
        assert.deepEqual( result, {'h-event': 1,'h-card': 2} );

   });



 });
