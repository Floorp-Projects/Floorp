#!/usr/bin/perl
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
use Time::HiRes qw(gettimeofday tv_interval);

sub encodeHiResTime {
    my $timeref = shift;
    return unless ref($timeref);
    my $time = $$timeref[0] . "-" . $$timeref[1];    
    return $time;
}

my $time = encodeHiResTime([gettimeofday()]);

print "Content-type: text/html\n\n";
print <<"ENDOFHTML";
<html>
<head>
<script>

var gServerTime = '$time';

function tokenizeQuery() {
  var query = {};
  var pairs = document.location.search.substring(1).split('&');
  for (var i=0; i < pairs.length; i++) {
    var pair = pairs[i].split('=');
    query[pair[0]] = unescape(pair[1]);
  }
  return query;
}

function setLocationHref(aHref, aReplace) {
    if (aReplace)
      document.location.replace(aHref);
    else
      document.location.href = aHref;
}

var gHref;
function doNextRequest(aTime) {
    function getValue(arg,def) {
        return !isNaN(arg) ? parseInt(Number(arg)) : def;
    }
    var q = tokenizeQuery();
    var delay    = getValue(q['delay'],    0);

    var now     = (new Date()).getTime();
    var c_intvl = now - c_ts;
    var c_ts    = now + delay; // adjust for delay time
    // Now make the request ...
    if (q['url']) {
        gHref = q['url'] +              
            "?c_part="  + -1 +    // bogo request is not recorded                      
            "&index="   + 0 +              
            "&id="      + q['id'] +                              
            "&maxcyc="  + q['maxcyc'] + 
            "&replace=" + q['replace'] +     
            "&nocache=" + q['nocache'] +  
            "&delay="   + delay +       
            "&timeout=" + q['timeout'] +       
            "&c_intvl=" + c_intvl +           
            "&s_ts="    + gServerTime +  
            "&c_ts="    + c_ts;                         
        window.setTimeout("setLocationHref(gHref,false);", delay);    
        return true; 
    }
}

function startTest() {
  if (window.innerHeight && window.innerWidth) {
    // force a consistent region for layout and painting.
    window.innerWidth=820;
    window.innerHeight=620;
  }
  doNextRequest(0);
}

window.setTimeout("startTest()", 1000);

</script>
</head>
<body>
  <p>
    This page starts the test.
  </p>
  <p>
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
  </p>
  <p>
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
  </p>
  <p>
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
    dummy page dummy page dummy page dummy page dummy page dummy page 
  </p>
</body>
</html>

ENDOFHTML

exit 0;
