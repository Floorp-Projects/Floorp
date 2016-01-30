/*
Unit test for isMicroformat
*/

assert = chai.assert;


describe('Microformat.isMicroformat', function() {
  
   it('true - v2', function(){
       
       var  doc,
            node;
            
        var html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a>';   
            
        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' ); 

        assert.isTrue( Microformats.isMicroformat( node ) );
        
   });
   
   
   it('true - v1', function(){
       
       var  doc,
            node;
            
        var html = '<a class="vcard" href="http://glennjones.net"><span class="fn">Glenn</span></a>';   
            
        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' ); 

        assert.isTrue( Microformats.isMicroformat( node ) );
        
   });
   
   
   it('true - v2 filter', function(){
       
       var  doc,
            node;
            
        var html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a>';   
            
        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' ); 

        assert.isTrue( Microformats.isMicroformat( node, {'filters': ['h-card']} ) );
        
   });
   
   
   it('true - v1 filter', function(){
       
       var  doc,
            node;
            
        var html = '<a class="vcard" href="http://glennjones.net"><span class="fn">Glenn</span></a>';   
            
        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' ); 

        assert.isTrue( Microformats.isMicroformat( node, {'filters': ['h-card']} ) );
        
   });
   
   
   it('false - v2 filter', function(){
       
       var  doc,
            node;
            
        var html = '<a class="h-card" href="http://glennjones.net"><span class="p-name">Glenn</span></a>';   
            
        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'a' ); 

        assert.isFalse( Microformats.isMicroformat( node, {'filters': ['h-entry']} ) );
        
   });
   
   
     
   it('false - property', function(){
       
       var  doc,
            node;
            
        var html = '<span class="p-name">Glenn</span>';   
            
        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'span' ); 

        assert.isFalse( Microformats.isMicroformat( node ) );
        
   });
   
   
   it('false - no class', function(){
       
       var  doc,
            node;
            
        var html = '<span>Glenn</span>';   
            
        doc = document.implementation.createHTMLDocument('New Document');
        node =  document.createElement('div');
        doc.body.appendChild( node );
        node.innerHTML = html;
        node = doc.querySelector( 'span' ); 

        assert.isFalse( Microformats.isMicroformat( node ) );
        
   });
   
   
   it('false - no node', function(){
        assert.isFalse( Microformats.isMicroformat( ) );
   });
   
   
   it('false - undefined node', function(){
        assert.isFalse( Microformats.isMicroformat( undefined ) );
   });
   
 });