# -*- Mode: perl; indent-tabs-mode: nil -*-
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
# 
# The Original Code is the Bugzilla Bug Tracking System.
# 
# The Initial Developer of the Original Code is Netscape Communications
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>

# Contains some global routines used throughout the CGI scripts of Bugzilla.

use diagnostics;
use strict;

use CGI::Carp qw(fatalsToBrowser);

require 'globals.pl';

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




# Implementations of several of the below were blatently stolen from CGI.pm,
# by Lincoln D. Stein.


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
    $toencode=~s/([^a-zA-Z0-9_\-.])/uc sprintf("%%%02x",ord($1))/eg;
    return $toencode;
}


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
    if (defined %isnull) {
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
#    open(DEBUG, ">debug") || die "Can't open debugging thing";
#    print DEBUG "Boundary is '$boundary'\n";
    while ($remaining > 0 && ($_ = <STDIN>)) {
        $remaining -= length($_);
#        print DEBUG "< $_";
        if ($_ =~ m/^-*$boundary/) {
#            print DEBUG "Entered header\n";
            $inheader = 1;
            $itemname = "";
            next;
        }

        if ($inheader) {
            if (m/^\s*$/) {
                $inheader = 0;
#                print DEBUG "left header\n";
                $::FORM{$itemname} = "";
            }
            if (m/^Content-Disposition:\s*form-data\s*;\s*name\s*=\s*"([^\"]+)"/i) {
                $itemname = $1;
#                print DEBUG "Found itemname $itemname\n";
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
    return $::FORM{$field};
}

sub html_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    return $var;
}

sub value_quote {
    my ($var) = (@_);
    $var =~ s/\&/\&amp;/g;
    $var =~ s/</\&lt;/g;
    $var =~ s/>/\&gt;/g;
    $var =~ s/"/\&quot;/g;
    return $var;
}

sub value_unquote {
    my ($var) = (@_);
    $var =~ s/\&quot/\"/g;
    $var =~ s/\&lt/</g;
    $var =~ s/\&gt/>/g;
    $var =~ s/\&amp/\&/g;
    return $var;
}


sub navigation_header {
    if (defined $::COOKIE{"BUGLIST"} && $::COOKIE{"BUGLIST"} ne "") {
	my @bugs = split(/:/, $::COOKIE{"BUGLIST"});
	my $cur = lsearch(\@bugs, $::FORM{"id"});
	print "<B>Bug List:</B> (@{[$cur + 1]} of @{[$#bugs + 1]})\n";
	print "<A HREF=\"show_bug.cgi?id=$bugs[0]\">First</A>\n";
	print "<A HREF=\"show_bug.cgi?id=$bugs[$#bugs]\">Last</A>\n";
	if ($cur > 0) {
	    print "<A HREF=\"show_bug.cgi?id=$bugs[$cur - 1]\">Prev</A>\n";
	} else {
	    print "<I><FONT COLOR=\#777777>Prev</FONT></I>\n";
	}
	if ($cur < $#bugs) {
	    $::next_bug = $bugs[$cur + 1];
	    print "<A HREF=\"show_bug.cgi?id=$::next_bug\">Next</A>\n";
	} else {
	    print "<I><FONT COLOR=\#777777>Next</FONT></I>\n";
	}
    }
    print "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=query.cgi>Query page</A>\n";
    print "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=enter_bug.cgi>Enter new bug</A>\n"
}


sub make_options {
    my ($src,$default,$isregexp) = (@_);
    my $last = "";
    my $popup = "";
    my $found = 0;
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


sub PasswordForLogin {
    my ($login) = (@_);
    SendSQL("select cryptpassword from profiles where login_name = " .
	    SqlQuote($login));
    my $result = FetchOneColumn();
    if (!defined $result) {
        $result = "";
    }
    return $result;
}


sub quietly_check_login() {
    $::usergroupset = '0';
    my $loginok = 0;
    if (defined $::COOKIE{"Bugzilla_login"} &&
	defined $::COOKIE{"Bugzilla_logincookie"}) {
        ConnectToDatabase();
        SendSQL("select profiles.groupset, profiles.login_name = " .
		SqlQuote($::COOKIE{"Bugzilla_login"}) .
		" and profiles.cryptpassword = logincookies.cryptpassword " .
		"and logincookies.hostname = " .
		SqlQuote($ENV{"REMOTE_HOST"}) .
		" from profiles,logincookies where logincookies.cookie = " .
		SqlQuote($::COOKIE{"Bugzilla_logincookie"}) .
		" and profiles.userid = logincookies.userid");
        my @row;
        if (@row = FetchSQLData()) {
            $loginok = $row[1];
            if ($loginok) {
                $::usergroupset = $row[0];
            }
        }
    }
    return $loginok;
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



sub confirm_login {
    my ($nexturl) = (@_);

# Uncommenting the next line can help debugging...
#    print "Content-type: text/plain\n\n";

    ConnectToDatabase();
    if (defined $::FORM{"Bugzilla_login"} &&
	defined $::FORM{"Bugzilla_password"}) {

	my $enteredlogin = $::FORM{"Bugzilla_login"};
        my $enteredpwd = $::FORM{"Bugzilla_password"};
        CheckEmailSyntax($enteredlogin);

        my $realcryptpwd  = PasswordForLogin($::FORM{"Bugzilla_login"});
        
        if (defined $::FORM{"PleaseMailAPassword"}) {
	    my $realpwd;
            if ($realcryptpwd eq "") {
		$realpwd = InsertNewUser($enteredlogin);
            } else {
                SendSQL("select password from profiles where login_name = " .
			SqlQuote($enteredlogin));
		$realpwd = FetchOneColumn();
            }
            my $urlbase = Param("urlbase");
            my $template = "From: bugzilla-daemon
To: %s
Subject: Your bugzilla password.

To use the wonders of bugzilla, you can use the following:

 E-mail address: %s
       Password: %s

 To change your password, go to:
 ${urlbase}changepassword.cgi

 (Your bugzilla and CVS password, if any, are not currently synchronized.
 Top hackers are working around the clock to fix this, as you read this.)
";
            my $msg = sprintf($template, $enteredlogin, $enteredlogin,
			      $realpwd);
            
	    open SENDMAIL, "|/usr/lib/sendmail -t";
	    print SENDMAIL $msg;
	    close SENDMAIL;

            print "Content-type: text/html\n\n";
            print "<H1>Password has been emailed.</H1>\n";
            print "The password for the e-mail address\n";
            print "$enteredlogin has been e-mailed to that address.\n";
            print "<p>When the e-mail arrives, you can click <b>Back</b>\n";
            print "and enter your password in the form there.\n";
            exit;
        }

	my $enteredcryptpwd = crypt($enteredpwd, substr($realcryptpwd, 0, 2));
        if ($realcryptpwd eq "" || $enteredcryptpwd ne $realcryptpwd) {
            print "Content-type: text/html\n\n";
            print "<H1>Login failed.</H1>\n";
            print "The username or password you entered is not valid.\n";
            print "Please click <b>Back</b> and try again.\n";
            exit;
        }
        $::COOKIE{"Bugzilla_login"} = $enteredlogin;
	SendSQL("insert into logincookies (userid,cryptpassword,hostname) values (@{[DBNameToIdAndCheck($enteredlogin)]}, @{[SqlQuote($realcryptpwd)]}, @{[SqlQuote($ENV{'REMOTE_HOST'})]})");
        SendSQL("select LAST_INSERT_ID()");
        my $logincookie = FetchOneColumn();

        $::COOKIE{"Bugzilla_logincookie"} = $logincookie;
        print "Set-Cookie: Bugzilla_login=$enteredlogin ; path=/; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";
        print "Set-Cookie: Bugzilla_logincookie=$logincookie ; path=/; expires=Sun, 30-Jun-2029 00:00:00 GMT\n";

        # This next one just cleans out any old bugzilla passwords that may
        # be sitting around in the cookie files, from the bad old days when
        # we actually stored the password there.
        print "Set-Cookie: Bugzilla_password= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT\n";

    }


    my $loginok = quietly_check_login();

    if ($loginok != 1) {
        print "Content-type: text/html\n\n";
        print "<H1>Please log in.</H1>\n";
        print "I need a legitimate e-mail address and password to continue.\n";
        if (!defined $nexturl || $nexturl eq "") {
	    # Sets nexturl to be argv0, stripping everything up to and
	    # including the last slash.
	    $0 =~ m:[^/]*$:;
	    $nexturl = $&;
        }
        my $method = "POST";
        if (defined $ENV{"REQUEST_METHOD"}) {
            $method = $ENV{"REQUEST_METHOD"};
        }
        print "
<FORM action=$nexturl method=$method>
<table>
<tr>
<td align=right><b>E-mail address:</b></td>
<td><input size=35 name=Bugzilla_login></td>
</tr>
<tr>
<td align=right><b>Password:</b></td>
<td><input type=password size=35 name=Bugzilla_password></td>
</tr>
</table>
";
        foreach my $i (keys %::FORM) {
            if ($i =~ /^Bugzilla_/) {
                next;
            }
            print "<input type=hidden name=$i value=\"@{[value_quote($::FORM{$i})]}\">\n";
        }
        print "
<input type=submit value=Login name=GoAheadAndLogIn><hr>
If you don't have a password, or have forgotten it, then please fill in the
e-mail address above and click
 here:<input type=submit value=\"E-mail me a password\"
name=PleaseMailAPassword>
</form>\n";

        # This seems like as good as time as any to get rid of old
        # crufty junk in the logincookies table.  Get rid of any entry
        # that hasn't been used in a month.
        SendSQL("delete from logincookies where to_days(now()) - to_days(lastused) > 30");

        
        exit;
    }

    # Update the timestamp on our logincookie, so it'll keep on working.
    SendSQL("update logincookies set lastused = null where cookie = $::COOKIE{'Bugzilla_logincookie'}");
}


sub PutHeader {
    my ($title, $h1, $h2) = (@_);

    if (!defined $h1) {
	$h1 = $title;
    }
    if (!defined $h2) {
	$h2 = "";
    }

    print "<HTML><HEAD><TITLE>$title</TITLE></HEAD>\n";
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

1;
