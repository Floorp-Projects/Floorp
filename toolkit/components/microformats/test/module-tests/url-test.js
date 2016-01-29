/*
Unit test for url
*/

assert = chai.assert;


// Tests the private Modules.url object 
// Modules.url is unit tested as it has an interface access by other modules  


describe('Modules.url', function() {
    
   it('resolve', function(){
       assert.equal( Modules.url.resolve( 'docs/index.html', 'http://example.org' ), 'http://example.org/docs/index.html' );
       assert.equal( Modules.url.resolve( '../index.html', 'http://example.org/docs/' ), 'http://example.org/index.html' );
       assert.equal( Modules.url.resolve( '/', 'http://example.org/' ), 'http://example.org/' );
       assert.equal( Modules.url.resolve( 'http://glennjones.net/', 'http://example.org/' ), 'http://glennjones.net/' );
       
       assert.equal( Modules.url.resolve( undefined, 'http://example.org/' ), '' );
       assert.equal( Modules.url.resolve( undefined, undefined ), '' );
       assert.equal( Modules.url.resolve( 'http://glennjones.net/', undefined ), 'http://glennjones.net/' );
   });
  
});