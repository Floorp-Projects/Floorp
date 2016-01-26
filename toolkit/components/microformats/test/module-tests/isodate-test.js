/*
Unit test for dates
*/

assert = chai.assert;


// Tests private Modules.ISODate object 
// Modules.ISODate is unit tested as it has an interface access by other modules  


describe('Modules.ISODates', function() {
    

   
   it('constructor', function(){
       assert.equal( new Modules.ISODate().toString('auto'), '' );
       assert.equal( new Modules.ISODate('2015-01-23T05:34:00', 'html5').toString('html5'), '2015-01-23 05:34:00' );
       assert.equal( new Modules.ISODate('2015-01-23T05:34:00', 'w3c').toString('w3c'), '2015-01-23T05:34:00' );
       assert.equal( new Modules.ISODate('2015-01-23T05:34:00', 'html5').toString('rfc3339'), '20150123T053400' );
       assert.equal( new Modules.ISODate('2015-01-23T05:34:00', 'auto').toString('auto'), '2015-01-23T05:34:00' );
   });
   
   
   it('parse', function(){
       assert.equal( new Modules.ISODate().parse('2015-01-23T05:34:00', 'html5').toString('html5'), '2015-01-23 05:34:00' );
       assert.equal( new Modules.ISODate().parse('2015-01-23T05:34:00', 'auto').toString('auto'), '2015-01-23T05:34:00' );
       assert.equal( new Modules.ISODate().parse('2015-01-23t05:34:00', 'auto').toString('auto'), '2015-01-23t05:34:00' );
       
       assert.equal( new Modules.ISODate().parse('2015-01-23t05:34:00Z', 'auto').toString('auto'), '2015-01-23t05:34:00Z' );
       assert.equal( new Modules.ISODate().parse('2015-01-23t05:34:00z', 'auto').toString('auto'), '2015-01-23t05:34:00z' );
       assert.equal( new Modules.ISODate().parse('2015-01-23 05:34:00Z', 'auto').toString('auto'), '2015-01-23 05:34:00Z' );
       assert.equal( new Modules.ISODate().parse('2015-01-23 05:34', 'auto').toString('auto'), '2015-01-23 05:34' );
       assert.equal( new Modules.ISODate().parse('2015-01-23 05', 'auto').toString('auto'), '2015-01-23 05' );
       
       assert.equal( new Modules.ISODate().parse('2015-01-23 05:34+01:00', 'auto').toString('auto'), '2015-01-23 05:34+01:00' );
       assert.equal( new Modules.ISODate().parse('2015-01-23 05:34-01:00', 'auto').toString('auto'), '2015-01-23 05:34-01:00' );
       assert.equal( new Modules.ISODate().parse('2015-01-23 05:34-01', 'auto').toString('auto'), '2015-01-23 05:34-01' );
       
       
       assert.equal( new Modules.ISODate().parse('2015-01-23', 'auto').toString('auto'), '2015-01-23' );
       // TODO support for importing rfc3339 profile dates
       // assert.equal( new Modules.ISODate().parse('20150123t0534', 'auto').toString('auto'), '2015-01-23 05:34' );
   });
   
   
   it('parseDate', function(){
       assert.equal( new Modules.ISODate().parseDate('2015-01-23T05:34:00', 'html5'), '2015-01-23' );
       assert.equal( new Modules.ISODate().parseDate('2015-01-23', 'auto'), '2015-01-23' );
       assert.equal( new Modules.ISODate().parseDate('2015-01', 'auto'), '2015-01' );
       assert.equal( new Modules.ISODate().parseDate('2015', 'auto'), '2015' );
       assert.equal( new Modules.ISODate().parseDate('2015-134', 'auto'), '2015-134' );
   });
   
   
   it('parseTime', function(){
       assert.equal( new Modules.ISODate().parseTime('05:34:00.1267', 'html5'), '05:34:00.1267' );
       assert.equal( new Modules.ISODate().parseTime('05:34:00', 'html5'), '05:34:00' );
       assert.equal( new Modules.ISODate().parseTime('05:34', 'html5'), '05:34' );
       assert.equal( new Modules.ISODate().parseTime('05', 'html5'), '05' );
   });
   
   it('parseTimeZone', function(){
        var date = new Modules.ISODate();
        date.parseTime('14:00');
        assert.equal( date.parseTimeZone('-01:00', 'auto'), '14:00-01:00' );
        
        date.clear();
        date.parseTime('14:00');
        assert.equal( date.parseTimeZone('-01', 'auto'), '14:00-01' );
        
        date.clear();
        date.parseTime('14:00');
        assert.equal( date.parseTimeZone('+01:00', 'auto').toString('auto'), '14:00+01:00' );
        
        date.clear();
        date.parseTime('15:00');
        assert.equal( date.parseTimeZone('Z', 'auto').toString('auto'), '15:00Z' );
       
        date.clear();
        date.parseTime('16:00');
        assert.equal( date.parseTimeZone('z', 'auto'), '16:00z' );
       
    });
    
    
    
   it('toString', function(){
       var date = new Modules.ISODate();    
       date.parseTime('05:34:00.1267');
       
       assert.equal( date.toString('html5'), '05:34:00.1267' );
   });
   
   
   it('toTimeString', function(){
       var date = new Modules.ISODate();    
       date.parseTime('05:34:00.1267');
       
       assert.equal( date.toTimeString('html5'), '05:34:00.1267' );
   });
   
   
    it('hasFullDate', function(){
        var dateEmpty = new Modules.ISODate(),
            date = new Modules.ISODate('2015-01-23T05:34:00');
        
        assert.isFalse( dateEmpty.hasFullDate() ); 
        assert.isTrue( date.hasFullDate() );    
    });
    
    
    it('hasDate', function(){
       var dateEmpty = new Modules.ISODate(),
           date = new Modules.ISODate('2015-01-23');
        
        assert.isFalse( dateEmpty.hasDate() ); 
        assert.isTrue( date.hasDate() );    
    });
    
   
    it('hasTime', function(){
       var dateEmpty = new Modules.ISODate(),
           date = new Modules.ISODate();
           
        date.parseTime('12:34');
        
        assert.isFalse( dateEmpty.hasTime() ); 
        assert.isTrue( date.hasTime() );     
    });
    
    
    it('hasTimeZone', function(){
       var dateEmpty = new Modules.ISODate(),
           date = new Modules.ISODate();
           
        date.parseTime('12:34'),
        date.parseTimeZone('-01:00');
        
        assert.isFalse( dateEmpty.hasTimeZone() ); 
        assert.isTrue( date.hasTimeZone() );     
    });
   
   
});
