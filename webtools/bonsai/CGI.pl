# -*- Mode: perl; indent-tabs-mode: nil -*-
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
# The Original Code is the Bugzilla Bug Tracking System.
#
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>

# Contains some global routines used throughout the CGI scripts of Bugzilla.

use strict;

use CGI::Carp qw(fatalsToBrowser);

require 'globals.pl';

##
##  Utility routines to convert strings
##

# Get rid of all the %xx encoding and the like from the given URL.
sub url_decode {
    my ($todecode) = (@_);
    $todecode =~ tr/+/ /;       # pluses become spaces
    $todecode =~ s/%([0-9a-fA-F]{2})/pack("c",hex($1))/ge;
    return $todecode;
}

# Quotify a string, suitable for putting into a URL.
sub url_quote {
    my($toencode) = (@_);
    $toencode =~ s/([^a-zA-Z0-9_\-\/.])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}

# Quotify a string, suitable for output as form values
sub value_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/\"/\&quot;/g;
    $var =~ s/\n/\&#010;/g;
    $var =~ s/\r/\&#013;/g;
    return $var;
}

sub url_encode2 {
    my ($s) = @_;

    $s =~ s/\%/\%25/g;
    $s =~ s/\=/\%3d/g;
    $s =~ s/\?/\%3f/g;
    $s =~ s/ /\%20/g;
    $s =~ s/\n/\%0a/g;
    $s =~ s/\r//g;
    $s =~ s/\"/\%22/g;
    $s =~ s/\'/\%27/g;
    $s =~ s/\|/\%7c/g;
    $s =~ s/\&/\%26/g;
    $s =~ s/\+/\%2b/g;
    return $s;
}

# Make sure CVS revisions are in a specific format
sub sanitize_revision {
    my ($rev) = @_;
    if ($rev =~ /^[A-Za-z]+/) {
        $rev =~ s/^([\w-]+).*/$1/;
    } elsif ($rev =~ /^\d+\.\d+/) {
        $rev =~ s/^(\d+[\.\d+]+).*/$1/;
    } elsif (defined($rev) && $rev ne "") {
        $rev = "1.1";
    }
    return $rev;
}

##
##  Routines to generate html as part of Bonsai
##

# Create the URL that has the correct tree and batch information
sub BatchIdPart {
    my ($initstr) = @_;
    my ($result, $ro) = ("", Param('readonly'));

    $initstr = ""                     unless (defined($initstr) && $initstr);

    $result = $initstr                if (($::TreeID ne "default") || $ro);
    $result .= "&treeid=$::TreeID"    if ($::TreeID ne "default");
    $result .= "&batchid=$::BatchID"  if ($ro);

    return $result;
}

# Create a generic page header for bonsai pages
sub PutsHeader {
    my ($title, $h1, $h2) = (@_);

    if (!defined $h1) {
	$h1 = $title;
    }
    if (!defined $h2) {
	$h2 = "";
    }

    print "<HTML><HEAD>\n<TITLE>$title</TITLE>\n";
    print $::Setup_String if (defined($::Setup_String) && $::Setup_String);
    print Param("headerhtml") . "\n</HEAD>\n";
    print "<BODY   BGCOLOR=\"#FFFFFF\" TEXT=\"#000000\"\n";
    print "LINK=\"#0000EE\" VLINK=\"#551A8B\" ALINK=\"#FF0000\">\n";

    print PerformSubsts(Param("bannerhtml"), undef);

    print "<TABLE BORDER=0 CELLPADDING=12 CELLSPACING=0 WIDTH=\"100%\">\n";
    print " <TR>\n";
    print "  <TD>\n";
    print "   <TABLE BORDER=0 CELLPADDING=0 CELLSPACING=2>\n";
    print "    <TR><TD VALIGN=TOP ALIGN=CENTER NOWRAP>\n";
    print "     <FONT SIZE=\"+3\"><B><NOBR>$h1</NOBR></B></FONT>\n";
    print "    </TD></TR><TR><TD VALIGN=TOP ALIGN=CENTER>\n";
    print "     <B>$h2</B>\n";
    print "    </TD></TR>\n";
    print "   </TABLE>\n";
    print "  </TD>\n";
    print "  <TD>\n";

    print Param("blurbhtml");

    print "</TD></TR></TABLE>\n";
}

# Create a generic page trailer for bonsai pages
sub PutsTrailer {
     my $args = BatchIdPart('?');

     print "
<br clear=all>
<hr>
<a href=\"toplevel.cgi$args\" target=_top>Back to the top of Bonsai</a><br>
Found a bug or have a feature request?
<a href=\"http://bugzilla.mozilla.org/enter_bug.cgi?product=Webtools&component=Bonsai\">File a bug report</a> about it.
</html>
";
}


sub GeneratePersonInput {
    my ($field, $required, $def_value, $extraJavaScript) = (@_);
    if (!defined $extraJavaScript) {
        $extraJavaScript = "";
    }
    if ($extraJavaScript ne "") {
        $extraJavaScript = "onChange=\" $extraJavaScript \"";
    }
    return "<INPUT NAME=\"$field\" SIZE=32 $extraJavaScript VALUE=\"$def_value\">";
}

sub GeneratePeopleInput {
    my ($field, $def_value) = (@_);
    return "<INPUT NAME=\"$field\" SIZE=45 VALUE=\"$def_value\">";
}


sub make_options {
    my ($src,$default,$isregexp) = (@_);
    my $last = "";
    my $popup = "";
    my $found = 0;

    if ($src) {
        foreach my $item (@$src) {
            if ($item eq "-blank-" || $item ne $last) {
                if ($item eq "-blank-") {
                    $item = "";
                }
                $last = $item;
                if ($isregexp ? $item =~ $default : $default eq $item) {
                    $popup .= "<OPTION SELECTED VALUE=\"$item\">$item";
                    $found = 1;
                } else {
                    $popup .= "<OPTION VALUE=\"$item\">$item";
                }
            }
        }
    }
    if (!$found && $default ne "") {
	$popup .= "<OPTION SELECTED>$default";
    }
    return $popup;
}

sub make_popup {
    my ($name,$src,$default,$listtype,$onchange) = (@_);
    my $popup = "<SELECT NAME=$name";
    if ($listtype > 0) {
        $popup .= " SIZE=5";
        if ($listtype == 2) {
            $popup .= " MULTIPLE";
        }
    }
    if (defined $onchange && $onchange ne "") {
        $popup .= " onchange=$onchange";
    }
    $popup .= ">" . make_options($src, $default,
                                 ($listtype == 2 && $default ne ""));
    $popup .= "</SELECT>";
    return $popup;
}

sub make_cgi_args {
    my ($k,$v);
    my $ret = "";

    for $k (sort keys %::FORM){
        $ret .= ($ret eq "" ? '?' : '&');
        $v = $::FORM{$k};
        $ret .= &url_encode2($k);
        $ret .= '=';
        $ret .= &url_encode2($v);
    }
    return $ret;
}

sub cvsmenu {
     my ($extra) = @_;
     my ($pass, $i, $page, $title);
     my ($desc, $branch, $root, $module);

     LoadTreeConfig();

     if (!defined $extra) {
         $extra = "";
     }
     print "<table border=1 bgcolor=#ffffcc $extra>\n";
     print "<tr><th>Menu</tr><tr><td><p>\n<dl>\n";
     
     foreach $pass ("cvsqueryform|Query",
                    "rview|Browse",
                    "moduleanalyse|Examine Modules") {
          ($page, $title) = split(/\|/, $pass);
          $page .= ".cgi";
          print "<b>$title</b><br><ul>\n";
          foreach $i (@::TreeList) {
               $branch = '';
               # HACK ALERT
               # quick fix by adam:
               # when browsing with rview, branch needs to be in 'rev' param
               # not 'branch' param.  don't ask me why ...
               my $hack = ($page eq 'rview.cgi') ? 'rev' : 'branch';
               $branch = "&$hack=$::TreeInfo{$i}{'branch'}"
                    if $::TreeInfo{$i}{'branch'};

               $desc = $::TreeInfo{$i}{'shortdesc'};
               $desc = $::TreeInfo{$i}{'description'} unless $desc;

               $root    = "cvsroot=$::TreeInfo{$i}{'repository'}";
               $module  = "module=$::TreeInfo{$i}{'module'}";
               print "<li><a href=\"$page?$root&$module$branch\">$desc</a>\n";
          };
          print "</ul>\n";
     };
     
     if (open(EXTRA, "<data/cvsmenuextra")) {
          while (<EXTRA>) {
               print $_;
          }
          close EXTRA;
     }

     print "</dl>
<p></tr><tr><td>
  Found a bug or have a feature request?
  <a href=\"http://bugzilla.mozilla.org/enter_bug.cgi?product=Webtools&component=Bonsai\">File a bug report</a> about it.</td>
</tr></table>
";
     
}



##
##  Routines to handle initializing CGI form data, cookies, etc...
##

sub ProcessFormFields {
    my ($buffer) = (@_);
    undef %::FORM;
    undef %::MFORM;

    my %isnull;
    my $remaining = $buffer;
    while ($remaining ne "") {
	my $item;
	if ($remaining =~ /^([^&]*)&(.*)$/) {
	    $item = $1;
	    $remaining = $2;
	} else {
	    $item = $remaining;
	    $remaining = "";
	}

	my $name;
	my $value;
	if ($item =~ /^([^=]*)=(.*)$/) {
	    $name = $1;
	    $value = url_decode($2);
	} else {
	    $name = $item;
	    $value = "";
	}
	if ($value ne "") {
	    if (defined $::FORM{$name}) {
		$::FORM{$name} .= $value;
		my $ref = $::MFORM{$name};
		push @$ref, $value;
	    } else {
		$::FORM{$name} = $value;
		$::MFORM{$name} = [$value];
	    }
        } else {
            $isnull{$name} = 1;
        }
    }
    if (%isnull) {
        foreach my $name (keys(%isnull)) {
            if (!defined $::FORM{$name}) {
                $::FORM{$name} = "";
                $::MFORM{$name} = [];
            }
        }
    }
}


sub ProcessMultipartFormFields {
    my ($boundary) = (@_);
    $boundary =~ s/^-*//;
    my $remaining = $ENV{"CONTENT_LENGTH"};
    my $inheader = 1;
    my $itemname = "";

    while ($remaining > 0 && ($_ = <STDIN>)) {
        $remaining -= length($_);
        if ($_ =~ m/^-*$boundary/) {
            $inheader = 1;
            $itemname = "";
            next;
        }

        if ($inheader) {
            if (m/^\s*$/) {
                $inheader = 0;
                $::FORM{$itemname} = "";
            }
            if (m/^Content-Disposition:\s*form-data\s*;\s*name\s*=\s*"([^\"]+)"/i) {
                $itemname = $1;
                if (m/;\s*filename\s*=\s*"([^\"]+)"/i) {
                    $::FILENAME{$itemname} = $1;
                }
            }
            
            next;
        }
        $::FORM{$itemname} .= $_;
    }
    delete $::FORM{""};

    # Get rid of trailing newlines.
    foreach my $i (keys %::FORM) {
        chomp($::FORM{$i});
        $::FORM{$i} =~ s/\r$//;
    }
}
        

sub FormData {
    my ($field) = (@_);

    unless (defined($::FORM{$field})) {
         print "\n<b>Error: Form field `<tt>$field</tt>' is not defined</b>\n";
         exit 0;
    }
    return $::FORM{$field};
}


sub CheckEmailSyntax {
    my ($addr) = (@_);
    if ($addr !~ /^[^@, ]*@[^@, ]*\.[^@, ]*$/) {
        print "Content-type: text/html\n\n";

        print "<H1>Invalid e-mail address entered.</H1>\n";
        print "The e-mail address you entered\n";
        print "(<b>$addr</b>) didn't match our minimal\n";
        print "syntax checking for a legal email address.  A legal\n";
        print "address must contain exactly one '\@', and at least one\n";
        print "'.' after the \@, and may not contain any commas or.\n";
        print "spaces.\n";
        print "<p>Please click <b>back</b> and try again.\n";
        exit;
    }
}



############# Live code below here (that is, not subroutine defs) #############


$| = 1;

# Uncommenting this next line can help debugging.
# print "Content-type: text/html\n\nHello mom\n";

# foreach my $k (sort(keys %ENV)) {
#     print "$k $ENV{$k}<br>\n";
# }

if (defined $ENV{"REQUEST_METHOD"}) {
    if ($ENV{"REQUEST_METHOD"} eq "GET") {
        if (defined $ENV{"QUERY_STRING"}) {
            $::buffer = $ENV{"QUERY_STRING"};
        } else {
            $::buffer = "";
        }
        ProcessFormFields $::buffer;
    } else {
        if ($ENV{"CONTENT_TYPE"} =~
            m@multipart/form-data; boundary=\s*([^; ]+)@) {
            ProcessMultipartFormFields($1);
            $::buffer = "";
        } else {
            read STDIN, $::buffer, $ENV{"CONTENT_LENGTH"} ||
                die "Couldn't get form data";
            ProcessFormFields $::buffer;
        }
    }
}


if (defined $ENV{"HTTP_COOKIE"}) {
    foreach my $pair (split(/;/, $ENV{"HTTP_COOKIE"})) {
	$pair = trim($pair);
	if ($pair =~ /^([^=]*)=(.*)$/) {
	    $::COOKIE{$1} = $2;
	} else {
	    $::COOKIE{$pair} = "";
	}
    }
}


if (defined $::FORM{'treeid'} && $::FORM{'treeid'} ne "") {
     $::TreeID = $::FORM{'treeid'};
}

if (defined $::FORM{'batchid'}) {
     LoadBatchID();
     if ($::BatchID != $::FORM{'batchid'}) {
          $::BatchID = $::FORM{'batchid'};

          # load parameters first to prevent overwriting
          Param('readonly'); 
          $::param{'readonly'} = 1;
     }
}

# Layers are supported only by Netscape 4.
# The DOM standards are supported by Mozilla and IE 5 or above.  It should
# also be supported by any browser claiming "Mozilla/5" or above.
$::use_layers = 0;
$::use_dom = 0;
# MSIE chokes on |type="application/x-javascript"| so if we detect MSIE, we
# we should send |type="text/javascript"|.  While we're at it, we should send
# |language="JavaScript"| for any browser that is "Mozilla/4" or older.
$::script_type = '"language="JavaScript""';
if (defined $ENV{HTTP_USER_AGENT}) {
    my $user_agent = $ENV{HTTP_USER_AGENT};
    if ($user_agent =~ m@^Mozilla/4.@ && $user_agent !~ /MSIE/) {
        $::use_layers = 1;
    } elsif ($user_agent =~ m@MSIE (\d+)@) {
        $::use_dom = 1 if $1 >= 5;
        $::script_type = 'type="text/javascript"';
    } elsif ($user_agent =~ m@^Mozilla/(\d+)@) {
        $::use_dom = 1 if $1 >= 5;
        $::script_type = 'type="application/x-javascript"';
    }
}

1;
