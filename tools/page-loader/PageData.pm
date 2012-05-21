#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
package PageData;
use strict;
use vars qw($MagicString $ClientJS); # defined at end of file

#
# contains a set of URLs and other meta information about them
#
sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {
        ClientJS    => $ClientJS,
        MagicString => $MagicString,
        PageHash    => {},
        PageList    => [],
        Length      => undef,
        FileBase    => undef,
        HTTPBase    => undef
    };
    bless ($self, $class);
    $self->_init();
    return $self;
}


#
# Parse a config file in the current directory for info.
# All requests to the current cgi-bin path will use the same info;
# to set up specialized lists, create a separate cgi-bin subdir
#
sub _init {

    my $self = shift;

    my $file = "urllist.txt";
    open(FILE, "< $file") ||
         die "Can't open file $file: $!";

    while (<FILE>) {
        next if /^$/;
        next if /^#|^\s+#/;
        s/\s+#.*$//;
        if (/^HTTPBASE:\s+(.*)$/i) {
            $self->{HTTPBase} = $1;
        } elsif (/^FILEBASE:\s+(.*)$/i) {
            $self->{FileBase} = $1;
        } else { 
            # 
            # each of the remaining lines are: 
            #   (1) the subdirectory containing the content for this URL,
            #   (2) the name of the top-level document [optional, default='index.html']
            #   (3) mime type for this document [optional, default is text/html]
            #   (4) a character set for this document [optional, default is none]
            # e.g., 
            #  home.netscape.com
            #  www.mozilla.org      index.html
            #  www.aol.com          default.xml      text/xml
            #  www.jp.aol.com       index.html       text/html   Shift_JIS
            #
            my @ary = split(/\s+/, $_);
            $ary[1] ||= 'index.html';
            push @{$self->{PageList}}, { Name    => $ary[0], 
                                         URL     => $ary[0] . '/' . $ary[1],
                                         MimeType => $ary[2] || "text/html",
                                         CharSet => $ary[3] || ''
                                         };
        }
    }

    # check that we have enough to go on
    die "Did not read any URLs" unless scalar(@{$self->{PageList}});
    die "Did not read a value for the http base" unless $self->{HTTPBase};
    die "Did not read a value for the file base" unless $self->{FileBase};

    $self->{Length}   = scalar(@{$self->{PageList}});
    $self->_createHashView();

}


sub _createHashView {
    # repackages the array, so it can be referenced by name
    my $self = shift;
    for my $i (0..$self->lastidx) {
        my $hash = $self->{PageList}[$i];
        #warn $i, " ", $hash, " ", %$hash;
        $self->{PageHash}{$hash->{Name}} = {
            Index => $i,
            URL   => $hash->{URL},
        };
    }
}


sub filebase    { my $self = shift; return $self->{FileBase}; }
sub httpbase    { my $self = shift; return $self->{HTTPBase}; }
sub length      { my $self = shift; return $self->{Length}; }
sub lastidx     { my $self = shift; return $self->{Length} - 1; }
sub magicString { my $self = shift; return $self->{MagicString}; }
sub clientJS    { my $self = shift; return $self->{ClientJS}; }


sub url {
    # get the relative url by index or by name
    my $self = shift;
    my $arg  = shift;
    if ($arg =~ /^\d+$/) {
        return $self->_checkIndex($arg) ? $self->{PageList}[$arg]{URL} : "";
    } else {
        return $self->{PageHash}{$arg}{URL};
    }
}


sub charset {
    # get the charset for this URL, by index
    my $self = shift;
    my $arg  = shift;
    if ($arg =~ /^\d+$/) {
        return $self->_checkIndex($arg) ? $self->{PageList}[$arg]{CharSet} : "";
    } else {
        die "$arg' is not a numeric index";
    }
}


sub mimetype {
    # get the mimetype for this URL, by index
    my $self = shift;
    my $arg  = shift;
    if ($arg =~ /^\d+$/) {
        return $self->_checkIndex($arg) ? $self->{PageList}[$arg]{MimeType} : "";
    } else {
        die "$arg' is not a numeric index";
    }
}


sub name {
    my $self = shift;
    my $arg  = shift;
    if ($arg =~ /^\d+$/) {
        return $self->_checkIndex($arg) ? $self->{PageList}[$arg]{Name} : "";
    } else {
        #warn "You looked up the name using a name.";
        return $arg;
    }
}


sub index {
    my $self = shift;
    my $arg = shift;
    if ($arg =~ /^\d+$/) {
        #warn "You looked up the index using an index.";
        return $arg;
    } else {
        return $self->{PageHash}{$arg}{Index};
    }
}


sub _checkIndex {
    my $self = shift;
    my $idx = shift;
    die "Bogus index passed to PageData: $idx"
        unless defined($idx) &&
               $idx =~ /^\d+$/ &&
               $idx >= 0 &&
               $idx < $self->{Length};
    return 1;
}


#
# JS to insert in the static HTML pages to trigger client timimg and reloading.
# You must escape any '$', '@', '\n' contained in the JS code fragment. Otherwise,
# perl will attempt to interpret them, and silently convert " $foo " to "  ".
#
# JS globals have been intentionally "uglified" with 'moztest_', to avoid collision
# with existing content in the page
#
$MagicString = '<!-- MOZ_INSERT_CONTENT_HOOK -->';
$ClientJS    =<<"ENDOFJS";

//<![CDATA[

function moztest_tokenizeQuery() {
  var query = {};
  var pairs = document.location.search.substring(1).split('&');
  for (var i=0; i < pairs.length; i++) {
    var pair = pairs[i].split('=');
    query[pair[0]] = unescape(pair[1]);
  }
  return query;
}

function moztest_setLocationHref(href, useReplace) {
    // false => "Location.href=url", not ".replace(url)"
    if (useReplace) {
        document.location.replace(href);
    } else {
        document.location.href = href;
    }
}

var g_moztest_Href;
function moztest_nextRequest(c_part) {
    function getValue(arg,def) {
        return !isNaN(arg) ? parseInt(Number(arg)) : def;
    }
    var q = moztest_tokenizeQuery();
    var index    = getValue(q['index'],   0);
    var cycle    = getValue(q['cycle'],   0);
    var maxcyc   = getValue(q['maxcyc'],  1);
    var replace  = getValue(q['replace'], 0);
    var nocache  = getValue(q['nocache'], 0);
    var delay    = getValue(q['delay'],   0);
    var timeout  = getValue(q['timeout'], 30000);
    var c_ts     = getValue(q['c_ts'],    Number.NaN);

    // check for times
    var now      = (new Date()).getTime();
    var c_intvl  = now - c_ts;
    var c_ts     = now + delay; // adjust for delay time

    // Now make the request ...
    g_moztest_Href = document.location.href.split('?')[0] +
        "?c_part="  + c_part +
        "&index="   + ++index +   // increment the request index
        "&id="      + q['id'] +
        "&maxcyc="  + maxcyc +
        "&replace=" + replace +
        "&nocache=" + nocache +
        "&delay="   + delay +
        "&timeout=" + timeout +
        "&c_intvl=" + c_intvl +
        "&s_ts="    + g_moztest_ServerTime +
        "&c_ts="    + c_ts +
        "&content=" + g_moztest_Content;
    window.setTimeout("moztest_setLocationHref(g_moztest_Href,false);", delay);
    return true;
}

function moztest_onDocumentLoad() {
  var loadTime = (new Date()).getTime() - g_moztest_Start;
  window.clearTimeout(g_moztest_safetyTimer); // the onload has fired, clear the safety
  moztest_nextRequest(loadTime);
}

function moztest_safetyValve() {
  moztest_nextRequest(Number.NaN);         // if the onload never fires
}

// normal processing is to calculate load time and fetch another URL
window.onload = moztest_onDocumentLoad;

//]]>

ENDOFJS

1; # return true from module
