#!/usr/bin/perl
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla page-loader test, released Aug 5, 2001
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 2001 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s):
#    John Morrison <jrgm@netscape.com>, original author
#    
#    
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
