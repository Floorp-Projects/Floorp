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
#                 Dan Mosedale <dmose@mozilla.org>
#                 Joe Robins <jmrobins@tgix.com>

# Contains some global routines used throughout the CGI scripts of Bugzilla.

use diagnostics;
use strict;
# use Carp;                       # for confess
# Shut up misguided -w warnings about "used only once".  For some reason,
# "use vars" chokes on me when I try it here.

# commented out the following snippet of code. this tosses errors into the
# CGI if you are perl 5.6, and doesn't if you have perl 5.003. 
# We want to check for the existence of the LDAP modules here.
# eval "use Mozilla::LDAP::Conn";
# my $have_ldap = $@ ? 0 : 1;


sub CGI_pl_sillyness {
    my $zz;
    $zz = %::FILENAME;
    $zz = %::MFORM;
    $zz = %::dontchange;
}

use CGI::Carp qw(fatalsToBrowser);

require 'globals.pl';

sub GeneratePersonInput {
    my ($field, $required, $def_value, $extraJavaScript) = (@_);
    $extraJavaScript ||= "";
    if ($extraJavaScript ne "") {
        $extraJavaScript = "onChange=\"$extraJavaScript\"";
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


sub ParseUrlString {
    my ($buffer, $f, $m) = (@_);
    undef %$f;
    undef %$m;

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
	    if (defined $f->{$name}) {
		$f->{$name} .= $value;
		my $ref = $m->{$name};
		push @$ref, $value;
	    } else {
		$f->{$name} = $value;
		$m->{$name} = [$value];
	    }
        } else {
            $isnull{$name} = 1;
        }
    }
    if (%isnull) {
        foreach my $name (keys(%isnull)) {
            if (!defined $f->{$name}) {
                $f->{$name} = "";
                $m->{$name} = [];
            }
        }
    }
}


sub ProcessFormFields {
    my ($buffer) = (@_);
    return ParseUrlString($buffer, \%::FORM, \%::MFORM);
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


# check and see if a given field exists, is non-empty, and is set to a 
# legal value.  assume a browser bug and abort appropriately if not.
# if $legalsRef is not passed, just check to make sure the value exists and 
# is non-NULL
# 
sub CheckFormField (\%$;\@) {
    my ($formRef,                # a reference to the form to check (a hash)
        $fieldname,              # the fieldname to check
        $legalsRef               # (optional) ref to a list of legal values 
       ) = @_;

    if ( !defined $formRef->{$fieldname} ||
         trim($formRef->{$fieldname}) eq "" ||
         (defined($legalsRef) && 
          lsearch($legalsRef, $formRef->{$fieldname})<0) ){

        print "A legal $fieldname was not set; ";
        print Param("browserbugmessage");
        PutFooter();
        exit 0;
      }
}

# check and see if a given field is defined, and abort if not
# 
sub CheckFormFieldDefined (\%$) {
    my ($formRef,                # a reference to the form to check (a hash)
        $fieldname,              # the fieldname to check
       ) = @_;

    if ( !defined $formRef->{$fieldname} ) {
        print "$fieldname was not defined; ";
        print Param("browserbugmessage");
        PutFooter();
        exit 0;
      }
}

# check and see if a given string actually represents a positive
# integer, and abort if not.
# 
sub CheckPosInt($) {
    my ($number) = @_;              # the fieldname to check

    if ( $number !~ /^[1-9][0-9]*$/ ) {
        print "Received string \"$number\" when postive integer expected; ";
        print Param("browserbugmessage");
        PutFooter();
        exit 0;
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
    # See bug http://bugzilla.mozilla.org/show_bug.cgi?id=4928 for 
    # explanaion of why bugzilla does this linebreak substitution. 
    # This caused form submission problems in mozilla (bug 22983, 32000).
    $var =~ s/\r\n/\&#013;/g;
    $var =~ s/\n\r/\&#013;/g;
    $var =~ s/\r/\&#013;/g;
    $var =~ s/\n/\&#013;/g;
    return $var;
}

sub navigation_header {
    if (defined $::COOKIE{"BUGLIST"} && $::COOKIE{"BUGLIST"} ne "" &&
        defined $::FORM{'id'}) {
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
        print qq{&nbsp;&nbsp;<A HREF="buglist.cgi?regetlastlist=1">Show list</A>\n};
    }
    print "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=query.cgi>Query page</A>\n";
    print "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<A HREF=enter_bug.cgi>Enter new bug</A>\n"
}

sub make_checkboxes {
    my ($src,$default,$isregexp,$name) = (@_);
    my $last = "";
    my $capitalized = "";
    my $popup = "";
    my $found = 0;
    $default = "" if !defined $default;

    if ($src) {
        foreach my $item (@$src) {
            if ($item eq "-blank-" || $item ne $last) {
                if ($item eq "-blank-") {
                    $item = "";
                }
                $last = $item;
                $capitalized = $item;
                $capitalized =~ tr/A-Z/a-z/;
                $capitalized =~ s/^(.?)(.*)/\u$1$2/;
                if ($isregexp ? $item =~ $default : $default eq $item) {
                    $popup .= "<INPUT NAME=$name TYPE=CHECKBOX VALUE=\"$item\" CHECKED>$capitalized<br>";
                    $found = 1;
                } else {
                    $popup .= "<INPUT NAME=$name TYPE=CHECKBOX VALUE=\"$item\">$capitalized<br>";
                }
            }
        }
    }
    if (!$found && $default ne "") {
	$popup .= "<INPUT NAME=$name TYPE=CHECKBOX CHECKED>$default";
    }
    return $popup;
}

#
# make_selection_widget: creates an HTML selection widget from a list of text strings.
# $groupname is the name of the setting (form value) that this widget will control
# $src is the list of options
# you can specify a $default value which is either a string or a regex pattern to match to
#    identify the default value
# $capitalize lets you optionally capitalize the option strings; the default is the value
#    of Param("capitalizelists")
# $multiple is 1 if several options are selectable (default), 0 otherwise.
# $size is used for lists to control how many items are shown. The default is 7. A list of
#    size 1 becomes a popup menu.
# $preferLists is 1 if selection lists should be used in favor of radio buttons and
#    checkboxes, and 0 otherwise. The default is the value of Param("preferlists").
#
# The actual widget generated depends on the parameter settings:
# 
#        MULTIPLE     PREFERLISTS    SIZE     RESULT
#       0 (single)        0           =1      Popup Menu (normal for list of size 1)
#       0 (single)        0           >1      Radio buttons
#       0 (single)        1           =1      Popup Menu (normal for list of size 1)
#       0 (single)        1           n>1     List of size n, single selection
#       1 (multi)         0           n/a     Check boxes; size ignored
#       1 (multi)         1           n/a     List of size n, multiple selection, of size n
#
sub make_selection_widget {
    my ($groupname,$src,$default,$isregexp,$multiple, $size, $capitalize, $preferLists) = (@_);
    my $last = "";
    my $popup = "";
    my $found = 0;
    my $displaytext = "";
    $groupname = "" if !defined $groupname;
    $default = "" if !defined $default;
    $capitalize = Param("capitalizelists") if !defined $capitalize;
    $multiple = 1 if !defined $multiple;
    $preferLists = Param("preferlists") if !defined $preferLists;
    $size = 7 if !defined $size;
    my $type = "LIST";
    if (!$preferLists) {
        if ($multiple) {
            $type = "CHECKBOX";
        } else {
            if ($size > 1) {
                $type = "RADIO";
            }
        }
    }

    if ($type eq "LIST") {
        $popup .= "<SELECT NAME=\"$groupname\"";
        if ($multiple) {
            $popup .= " MULTIPLE";
        }
        $popup .= " SIZE=$size>\n";
    }
    if ($src) {
        foreach my $item (@$src) {
            if ($item eq "-blank-" || $item ne $last) {
                if ($item eq "-blank-") {
                    $item = "";
                }
                $last = $item;
                $displaytext = $item;
                if ($capitalize) {
                    $displaytext =~ tr/A-Z/a-z/;
                    $displaytext =~ s/^(.?)(.*)/\u$1$2/;
                }   

                if ($isregexp ? $item =~ $default : $default eq $item) {
                    if ($type eq "CHECKBOX") {
                        $popup .= "<INPUT NAME=$groupname type=checkbox VALUE=\"$item\" CHECKED>$displaytext<br>";
                    } elsif ($type eq "RADIO") {
                        $popup .= "<INPUT NAME=$groupname type=radio VALUE=\"$item\" check>$displaytext<br>";
                    } else {
                        $popup .= "<OPTION SELECTED VALUE=\"$item\">$displaytext";
                    }
                    $found = 1;
                } else {
                    if ($type eq "CHECKBOX") {
                        $popup .= "<INPUT NAME=$groupname type=checkbox VALUE=\"$item\">$displaytext<br>";
                    } elsif ($type eq "RADIO") {
                        $popup .= "<INPUT NAME=$groupname type=radio VALUE=\"$item\">$displaytext<br>";
                    } else {
                        $popup .= "<OPTION VALUE=\"$item\">$displaytext";
                    }
                }
            }
        }
    }
    if (!$found && $default ne "") {
        if ($type eq "CHECKBOX") {
            $popup .= "<INPUT NAME=$groupname type=checkbox CHECKED>$default";
        } elsif ($type eq "RADIO") {
            $popup .= "<INPUT NAME=$groupname type=radio checked>$default";
        } else {
            $popup .= "<OPTION SELECTED>$default";
        }
    }
    if ($type eq "LIST") {
        $popup .= "</SELECT>";
    }
    return $popup;
}


$::CheckOptionValues = 1;

sub make_options {
    my ($src,$default,$isregexp) = (@_);
    my $last = "";
    my $popup = "";
    my $found = 0;
    $default = "" if !defined $default;

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
      if ( Param("strictvaluechecks") && $::CheckOptionValues &&
           ($default ne $::dontchange) && ($default ne "-All-") &&
           ($default ne "DUPLICATE") ) {
        print "Possible bug database corruption has been detected.  " .
              "Please send mail to " . Param("maintainer") . " with " .
              "details of what you were doing when this message " . 
              "appeared.  Thank you.\n";
        if (!$src) {
            $src = ["???null???"];
        }
        print "<pre>src = " . value_quote(join(' ', @$src)) . "\n";
        print "default = " . value_quote($default) . "</pre>";
        PutFooter();
#        confess "Gulp.";
        exit 0;
              
      } else {
	$popup .= "<OPTION SELECTED>$default";
      }
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


sub BuildPulldown {
    my ($name, $valuelist, $default) = (@_);

    my $entry = qq{<SELECT NAME="$name">};
    foreach my $i (@$valuelist) {
        my ($tag, $desc) = (@$i);
        my $selectpart = "";
        if ($tag eq $default) {
            $selectpart = " SELECTED";
        }
        if (!defined $desc) {
            $desc = $tag;
        }
        $entry .= qq{<OPTION$selectpart VALUE="$tag">$desc\n};
    }
    $entry .= qq{</SELECT>};
    return $entry;
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
    $::disabledreason = '';
    $::userid = 0;
    if (defined $::COOKIE{"Bugzilla_login"} &&
	defined $::COOKIE{"Bugzilla_logincookie"}) {
        ConnectToDatabase();
        if (!defined $ENV{'REMOTE_HOST'}) {
            $ENV{'REMOTE_HOST'} = $ENV{'REMOTE_ADDR'};
        }
        SendSQL("SELECT profiles.userid, profiles.groupset, " .
                "profiles.login_name, " .
                "profiles.login_name = " .
		SqlQuote($::COOKIE{"Bugzilla_login"}) .
		" AND profiles.cryptpassword = logincookies.cryptpassword " .
		"AND logincookies.hostname = " .
		SqlQuote($ENV{"REMOTE_HOST"}) .
                ", profiles.disabledtext " .
		" FROM profiles, logincookies WHERE logincookies.cookie = " .
		SqlQuote($::COOKIE{"Bugzilla_logincookie"}) .
		" AND profiles.userid = logincookies.userid");
        my @row;
        if (@row = FetchSQLData()) {
            my ($userid, $groupset, $loginname, $ok, $disabledtext) = (@row);
            if ($ok) {
                if ($disabledtext eq '') {
                    $loginok = 1;
                    $::userid = $userid;
                    $::usergroupset = $groupset;
                    $::COOKIE{"Bugzilla_login"} = $loginname; # Makes sure case
                                                              # is in
                                                              # canonical form.
                } else {
                    $::disabledreason = $disabledtext;
                }
            }
        }
    }
    # if 'who' is passed in, verify that it's a good value
    if ($::FORM{'who'}) {
        my $whoid = DBname_to_id($::FORM{'who'});
        delete $::FORM{'who'} unless $whoid;
    }
    if (!$loginok) {
        delete $::COOKIE{"Bugzilla_login"};
    }
    return $loginok;
}




sub CheckEmailSyntax {
    my ($addr) = (@_);
    my $match = Param('emailregexp');
    if ($addr !~ /$match/) {
        print "Content-type: text/html\n\n";

        # For security, escape HTML special characters.
        $addr = html_quote($addr);

        PutHeader("Check e-mail syntax");
        print "The e-mail address you entered\n";
        print "(<b>$addr</b>) didn't match our minimal\n";
        print "syntax checking for a legal email address.\n";
        print Param('emailregexpdesc');
        print "<p>Please click <b>back</b> and try again.\n";
        PutFooter();
        exit;
    }
}



sub MailPassword {
    my ($login, $password) = (@_);
    my $urlbase = Param("urlbase");
    my $template = Param("passwordmail");
    my $msg = PerformSubsts($template,
                            {"mailaddress" => $login . Param('emailsuffix'),
                             "login" => $login,
                             "password" => $password});

    open SENDMAIL, "|/usr/lib/sendmail -t";
    print SENDMAIL $msg;
    close SENDMAIL;

    print "The password for the e-mail address\n";
    print "$login has been e-mailed to that address.\n";
    print "<p>When the e-mail arrives, you can click <b>Back</b>\n";
    print "and enter your password in the form there.\n";
}


sub confirm_login {
    my ($nexturl) = (@_);

# Uncommenting the next line can help debugging...
#    print "Content-type: text/plain\n\n";

    ConnectToDatabase();
    # I'm going to reorganize some of this stuff a bit.  Since we're adding
    # a second possible validation method (LDAP), we need to move some of this
    # to a later section.  -Joe Robins, 8/3/00
    my $enteredlogin = "";
    my $realcryptpwd = "";
    if (defined $::FORM{"Bugzilla_login"} &&
	defined $::FORM{"Bugzilla_password"}) {

       $enteredlogin = $::FORM{"Bugzilla_login"};
       my $enteredpwd = $::FORM{"Bugzilla_password"};
       CheckEmailSyntax($enteredlogin);

       $realcryptpwd  = PasswordForLogin($::FORM{"Bugzilla_login"});

       if (defined $::FORM{"PleaseMailAPassword"}) {
         my $realpwd;
         if ($realcryptpwd eq "") {
           $realpwd = InsertNewUser($enteredlogin, "");
         } else {
           SendSQL("select password from profiles where login_name = " .
                   SqlQuote($enteredlogin));
           $realpwd = FetchOneColumn();
         }
         print "Content-type: text/html\n\n";
         PutHeader("Password has been emailed");
         MailPassword($enteredlogin, $realpwd);
         PutFooter();
         exit;
       }

       SendSQL("SELECT encrypt(" . SqlQuote($enteredpwd) . ", " .
               SqlQuote(substr($realcryptpwd, 0, 2)) . ")");
       my $enteredcryptpwd = FetchOneColumn();

       if ($realcryptpwd eq "" || $enteredcryptpwd ne $realcryptpwd) {
         print "Content-type: text/html\n\n";
         PutHeader("Login failed");
         print "The username or password you entered is not valid.\n";
         print "Please click <b>Back</b> and try again.\n";
         PutFooter();
         exit;
       }
     } elsif (Param("useLDAP") &&
              defined $::FORM{"LDAP_login"} &&
              defined $::FORM{"LDAP_password"}) {
       # If we're using LDAP for login, we've got an entirely different
       # set of things to check.

# see comment at top of file near eval
       # First, if we don't have the LDAP modules available to us, we can't
       # do this.
#       if(!$have_ldap) {
#         print "Content-type: text/html\n\n";
#         PutHeader("LDAP not enabled");
#         print "The necessary modules for LDAP login are not installed on ";
#         print "this machine.  Please send mail to ".Param("maintainer");
#         print " and notify him of this problem.\n";
#         PutFooter();
#         exit;
#       }

       # Next, we need to bind anonymously to the LDAP server.  This is
       # because we need to get the Distinguished Name of the user trying
       # to log in.  Some servers (such as iPlanet) allow you to have unique
       # uids spread out over a subtree of an area (such as "People"), so
       # just appending the Base DN to the uid isn't sufficient to get the
       # user's DN.  For servers which don't work this way, there will still
       # be no harm done.
       my $LDAPserver = Param("LDAPserver");
       if ($LDAPserver eq "") {
         print "Content-type: text/html\n\n";
         PutHeader("LDAP server not defined");
         print "The LDAP server for authentication has not been defined.  ";
         print "Please contact ".Param("maintainer")." ";
         print "and notify him of this problem.\n";
         PutFooter();
         exit;
       }

       my $LDAPport = "389";  #default LDAP port
       if($LDAPserver =~ /:/) {
         ($LDAPserver, $LDAPport) = split(":",$LDAPserver);
       }
       my $LDAPconn = new Mozilla::LDAP::Conn($LDAPserver,$LDAPport);
       if(!$LDAPconn) {
         print "Content-type: text/html\n\n";
         PutHeader("Unable to connect to LDAP server");
         print "I was unable to connect to the LDAP server for user ";
         print "authentication.  Please contact ".Param("maintainer");
         print " and notify him of this problem.\n";
         PutFooter();
         exit;
       }

       # We've got our anonymous bind;  let's look up this user.
       my $dnEntry = $LDAPconn->search(Param("LDAPBaseDN"),"subtree","uid=".$::FORM{"LDAP_login"});
       if(!$dnEntry) {
         print "Content-type: text/html\n\n";
         PutHeader("Login Failed");
         print "The username or password you entered is not valid.\n";
         print "Please click <b>Back</b> and try again.\n";
         PutFooter();
         exit;
       }

       # Now we get the DN from this search.  Once we've got that, we're
       # done with the anonymous bind, so we close it.
       my $userDN = $dnEntry->getDN;
       $LDAPconn->close;

       # Now we attempt to bind as the specified user.
       $LDAPconn = new Mozilla::LDAP::Conn($LDAPserver,$LDAPport,$userDN,$::FORM{"LDAP_password"});
       if(!$LDAPconn) {
         print "Content-type: text/html\n\n";
         PutHeader("Login Failed");
         print "The username or password you entered is not valid.\n";
         print "Please click <b>Back</b> and try again.\n";
         PutFooter();
         exit;
       }

       # And now we're going to repeat the search, so that we can get the
       # mail attribute for this user.
       my $userEntry = $LDAPconn->search(Param("LDAPBaseDN"),"subtree","uid=".$::FORM{"LDAP_login"});
       if(!$userEntry->exists(Param("LDAPmailattribute"))) {
         print "Content-type: text/html\n\n";
         PutHeader("LDAP authentication error");
         print "I was unable to retrieve the ".Param("LDAPmailattribute");
         print " attribute from the LDAP server.  Please contact ";
         print Param("maintainer")." and notify him of this error.\n";
         PutFooter();
         exit;
       }

       # Mozilla::LDAP::Entry->getValues returns an array for the attribute
       # requested, even if there's only one entry.
       $enteredlogin = ($userEntry->getValues(Param("LDAPmailattribute")))[0];

       # We're going to need the cryptpwd for this user from the database
       # so that we can set the cookie below, even though we're not going
       # to use it for authentication.
       $realcryptpwd = PasswordForLogin($enteredlogin);

       # If we don't get a result, then we've got a user who isn't in
       # Bugzilla's database yet, so we've got to add them.
       if($realcryptpwd eq "") {
         # We'll want the user's name for this.
         my $userRealName = ($userEntry->getValues("displayName"))[0];
         if($userRealName eq "") {
           $userRealName = ($userEntry->getValues("cn"))[0];
         }
         InsertNewUser($enteredlogin, $userRealName);
         $realcryptpwd = PasswordForLogin($enteredlogin);
       }
     } # end LDAP authentication

     # And now, if we've logged in via either method, then we need to set
     # the cookies.
     if($enteredlogin ne "") {
       $::COOKIE{"Bugzilla_login"} = $enteredlogin;
       if (!defined $ENV{'REMOTE_HOST'}) {
         $ENV{'REMOTE_HOST'} = $ENV{'REMOTE_ADDR'};
       }
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
        if ($::disabledreason) {
            print "Set-Cookie: Bugzilla_login= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_logincookie= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Set-Cookie: Bugzilla_password= ; path=/; expires=Sun, 30-Jun-80 00:00:00 GMT
Content-type: text/html

";
            PutHeader("Your account has been disabled");
            print $::disabledreason;
            print "<HR>\n";
            print "If you believe your account should be restored, please\n";
            print "send email to " . Param("maintainer") . " explaining\n";
            print "why.\n";
            PutFooter();
            exit();
        }
        print "Content-type: text/html\n\n";
        PutHeader("Login", undef, undef, undef, 1);
        if(Param("useLDAP")) {
          print "I need a legitimate LDAP username and password to continue.\n";
        } else {
          print "I need a legitimate e-mail address and password to continue.\n";
        }
        if (!defined $nexturl || $nexturl eq "") {
	    # Sets nexturl to be argv0, stripping everything up to and
	    # including the last slash.
	    $0 =~ m:[^/]*$:;
	    $nexturl = $&;
        }
        my $method = "POST";
        if (defined $ENV{"REQUEST_METHOD"} && length($::buffer) > 1) {
            $method = $ENV{"REQUEST_METHOD"};
        }
        print "
<FORM action=$nexturl method=$method>
<table>
<tr>";
        if(Param("useLDAP")) {
          print "
<td align=right><b>Username:</b></td>
<td><input size=10 name=LDAP_login></td>
</tr>
<tr>
<td align=right><b>Password:</b></td>
<td><input type=password size=10 name=LDAP_password></td>";
        } else {
          print "
<td align=right><b>E-mail address:</b></td>
<td><input size=35 name=Bugzilla_login></td>
</tr>
<tr>
<td align=right><b>Password:</b></td>
<td><input type=password size=35 name=Bugzilla_password></td>";
        }
        print "
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
";
        # If we're using LDAP, we can't request that a password be mailed...
        unless(Param("useLDAP")) {
          print "
If you don't have a password, or have forgotten it, then please fill in the
e-mail address above and click
 here:<input type=submit value=\"E-mail me a password\"
name=PleaseMailAPassword>
</form>\n";
        }

        # This seems like as good as time as any to get rid of old
        # crufty junk in the logincookies table.  Get rid of any entry
        # that hasn't been used in a month.
        if ($::dbwritesallowed) {
            SendSQL("DELETE FROM logincookies " .
                    "WHERE TO_DAYS(NOW()) - TO_DAYS(lastused) > 30");
        }

        
        PutFooter();
        exit;
    }

    # Update the timestamp on our logincookie, so it'll keep on working.
    if ($::dbwritesallowed) {
        SendSQL("UPDATE logincookies SET lastused = null " .
                "WHERE cookie = $::COOKIE{'Bugzilla_logincookie'}");
    }
    return $::userid;
}


sub PutHeader {
    my ($title, $h1, $h2, $extra, $ignoreshutdown, $jscript) = (@_);

    if (!defined $h1) {
	$h1 = $title;
    }
    if (!defined $h2) {
	$h2 = "";
    }
    if (!defined $extra) {
	$extra = "";
    }
    $jscript ||= "";

    print "<HTML><HEAD>\n<TITLE>$title</TITLE>\n";
    print Param("headerhtml") . "\n$jscript\n</HEAD>\n";
    print "<BODY " . Param("bodyhtml") . " $extra>\n";

    print PerformSubsts(Param("bannerhtml"), undef);

    print "<TABLE BORDER=0 CELLSPACING=0 WIDTH=\"100%\">\n";
    print " <TR>\n";
    print "  <TD WIDTH=10% VALIGN=TOP ALIGN=LEFT>\n";
    print "   <TABLE BORDER=0 CELLPADDING=0 CELLSPACING=2>\n";
    print "    <TR><TD VALIGN=TOP ALIGN=LEFT NOWRAP>\n";
    print "     <FONT SIZE=+1><B>$h1</B></FONT>";
    print "    </TD></TR>\n";
    print "   </TABLE>\n";
    print "  </TD>\n";
    print "  <TD VALIGN=CENTER>&nbsp;</TD>\n";
    print "  <TD VALIGN=CENTER ALIGN=LEFT>\n";

    print "$h2\n";
    print "</TD></TR></TABLE>\n";

    if (Param("shutdownhtml")) {
        if (!$ignoreshutdown) {
            print Param("shutdownhtml");
            exit;
        }
    }
}


sub PutFooter {
    print PerformSubsts(Param("footerhtml"));
    SyncAnyPendingShadowChanges();
}


sub DisplayError {
  my ($message, $title) = (@_);
  $title ||= "Error";

  print "Content-type: text/html\n\n";
  PutHeader($title);

  print PerformSubsts( Param("errorhtml") , {errormsg => $message} );

  PutFooter();

  return 1;
}

sub PuntTryAgain ($) {
    my ($str) = (@_);
    print PerformSubsts(Param("errorhtml"),
                        {errormsg => $str});
    SendSQL("UNLOCK TABLES");
    PutFooter();
    exit;
}


sub CheckIfVotedConfirmed {
    my ($id, $who) = (@_);
    SendSQL("SELECT bugs.votes, bugs.bug_status, products.votestoconfirm, " .
            "       bugs.everconfirmed " .
            "FROM bugs, products " .
            "WHERE bugs.bug_id = $id AND products.product = bugs.product");
    my ($votes, $status, $votestoconfirm, $everconfirmed) = (FetchSQLData());
    if ($votes >= $votestoconfirm && $status eq $::unconfirmedstate) {
        SendSQL("UPDATE bugs SET bug_status = 'NEW', everconfirmed = 1 " .
                "WHERE bug_id = $id");
        my $fieldid = GetFieldID("bug_status");
        SendSQL("INSERT INTO bugs_activity " .
                "(bug_id,who,bug_when,fieldid,oldvalue,newvalue) VALUES " .
                "($id,$who,now(),$fieldid,'$::unconfirmedstate','NEW')");
        if (!$everconfirmed) {
            $fieldid = GetFieldID("everconfirmed");
            SendSQL("INSERT INTO bugs_activity " .
                    "(bug_id,who,bug_when,fieldid,oldvalue,newvalue) VALUES " .
                    "($id,$who,now(),$fieldid,'0','1')");
        }
        AppendComment($id, DBID_to_name($who),
                      "*** This bug has been confirmed by popular vote. ***");
        print "<TABLE BORDER=1><TD><H2>Bug $id has been confirmed by votes.</H2>\n";
        system("./processmail", $id);
        print "<TD><A HREF=\"show_bug.cgi?id=$id\">Go To BUG# $id</A></TABLE>\n";
    }

}



sub DumpBugActivity {
    my ($id, $starttime) = (@_);
    my $datepart = "";

    die "Invalid id: $id" unless $id=~/^\s*\d+\s*$/;

    if (defined $starttime) {
        $datepart = "and bugs_activity.bug_when >= $starttime";
    }
    my $query = "
        SELECT IFNULL(fielddefs.name, bugs_activity.fieldid),
                bugs_activity.bug_when,
                bugs_activity.oldvalue, bugs_activity.newvalue,
                profiles.login_name
        FROM bugs_activity LEFT JOIN fielddefs ON 
                                     bugs_activity.fieldid = fielddefs.fieldid,
             profiles
        WHERE bugs_activity.bug_id = $id $datepart
              AND profiles.userid = bugs_activity.who
        ORDER BY bugs_activity.bug_when";

    SendSQL($query);
    
    print "<table border cellpadding=4>\n";
    print "<tr>\n";
    print "    <th>Who</th><th>What</th><th>Old value</th><th>New value</th><th>When</th>\n";
    print "</tr>\n";
    
    my @row;
    while (@row = FetchSQLData()) {
        my ($field,$when,$old,$new,$who) = (@row);
        $old = value_quote($old);
        $new = value_quote($new);
        if ($old eq "") {
            $old = "&nbsp;";
        }
        if ($new eq "") {
            $new = "&nbsp;";
        }
        print "<tr>\n";
        print "<td>$who</td>\n";
        print "<td>$field</td>\n";
        print "<td>$old</td>\n";
        print "<td>$new</td>\n";
        print "<td>$when</td>\n";
        print "</tr>\n";
    }
    print "</table>\n";
}


sub GetCommandMenu {
    my $loggedin = quietly_check_login();
    my $html = "";
    $html .= <<"--endquote--";
<FORM METHOD=GET ACTION="show_bug.cgi">
<TABLE width="100%"><TR><TD>
Actions:
</TD><TD VALIGN="middle" NOWRAP>
<a href='enter_bug.cgi'>New</a> | <a href='query.cgi'>Query</a> |
--endquote--

    if (-e "query2.cgi") {
        $html .= "[<a href='query2.cgi'>beta</a>]";
    }
    
    $html .=
        qq{ <INPUT TYPE=SUBMIT VALUE="Find"> bug \# <INPUT NAME=id SIZE=6>};

    $html .= " | <a href='reports.cgi'>Reports</a>"; 
    if ($loggedin) {
        if ($::anyvotesallowed) {
            $html .= " | <A HREF=\"showvotes.cgi\">My votes</A>";
        }
    }
    if ($loggedin) {
    	#a little mandatory SQL, used later on
        SendSQL("SELECT mybugslink, userid, blessgroupset FROM profiles " .
                "WHERE login_name = " . SqlQuote($::COOKIE{'Bugzilla_login'}));
        my ($mybugslink, $userid, $blessgroupset) = (FetchSQLData());
        
        #Begin settings
        $html .= "</TD><TD>&nbsp;</TD><TD VALIGN=middle><NOBR>Edit <a href='userprefs.cgi'>prefs</a></NOBR>";
        if (UserInGroup("tweakparams")) {
            $html .= ", <a href=editparams.cgi>parameters</a>";
            $html .= ", <a href=sanitycheck.cgi><NOBR>sanity check</NOBR></a>";
        }
        if (UserInGroup("editusers") || $blessgroupset) {
            $html .= ", <a href=editusers.cgi>users</a>";
        }
        if (UserInGroup("editcomponents")) {
            $html .= ", <a href=editproducts.cgi>components</a>";
        }
        if (UserInGroup("creategroups")) {
            $html .= ", <a href=editgroups.cgi>groups</a>";
        }
        if (UserInGroup("editkeywords")) {
            $html .= ", <a href=editkeywords.cgi>keywords</a>";
        }
        $html .= " | <NOBR><a href=relogin.cgi>Log out</a> $::COOKIE{'Bugzilla_login'}</NOBR>";
        $html .= "</TD></TR>";
        
		#begin preset queries
        my $mybugstemplate = Param("mybugstemplate");
        my %substs;
        $substs{'userid'} = url_quote($::COOKIE{"Bugzilla_login"});
        if (!defined $::anyvotesallowed) {
            GetVersionTable();
        }
        $html .= "</TR><TR>";
        $html .= "<TD>Preset Queries: </TD>";
        $html .= "<TD colspan=3>\n";
        if ($mybugslink) {
            my $mybugsurl = PerformSubsts($mybugstemplate, \%substs);
            $html = $html . "<A HREF='$mybugsurl'><NOBR>My bugs</NOBR></A>";
        }
        SendSQL("SELECT name FROM namedqueries " .
                "WHERE userid = $userid AND linkinfooter");
        my $anynamedqueries = 0;
        while (MoreSQLData()) {
            my ($name) = (FetchSQLData());
            if ($anynamedqueries || $mybugslink) { $html .= " | " }
            $anynamedqueries = 1;
            $html .= "<A HREF=\"buglist.cgi?&cmdtype=runnamed&namedcmd=" .
                     url_quote($name) . "\"><NOBR>$name</NOBR></A>";
        }
        $html .= "</TR>\n<TR>";
    } else {
    $html .= "</TD><TD>&nbsp;</TD><TD valign=middle align=right>\n";
        $html .=
            " <a href=\"createaccount.cgi\"><NOBR>New account</NOBR></a>\n";
        $html .=
            " | <NOBR><a href=query.cgi?GoAheadAndLogIn=1>Log in</a></NOBR>";
        $html .= "</TD></TR>";
    }
    $html .= "</FORM>\n";
    $html .= "</TABLE>";                
    return $html;
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
        if (exists($ENV{"CONTENT_TYPE"}) && $ENV{"CONTENT_TYPE"} =~
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
