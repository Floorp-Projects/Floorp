#!/usr/bonsaitools/bin/perl -w
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
# Corporation. Portions created by Netscape are Copyright (C) 1998
# Netscape Communications Corporation. All Rights Reserved.
# 
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 Dave Miller <justdave@syndicomm.com>
#                 Joe Robins <jmrobins@tgix.com>


########################################################################
#
# enter_bug.cgi
# -------------
# Displays bug entry form. Bug fields are specified through popup menus, 
# drop-down lists, or text fields. Default for these values can be passed
# in as parameters to the cgi.
#
########################################################################

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once".  "use vars" just
# doesn't work for me.

sub sillyness {
    my $zz;
    $zz = $::unconfirmedstate;
    $zz = @::legal_opsys;
    $zz = @::legal_platform;
    $zz = @::legal_priority;
    $zz = @::legal_severity;
}

# I've moved the call to confirm_login up to here, since if we're using bug
# groups to restrict bug entry, we need to know who the user is right from
# the start.  If that parameter is turned off, there's still no harm done in
# doing it now instead of a bit later.  -JMR, 2/18/00
# Except that it will cause people without cookies enabled to have to log
# in an extra time.  Only do it here if we really need to.  -terry, 3/10/00
if (Param("usebuggroupsentry")) {
    confirm_login();
}

if (!defined $::FORM{'product'}) {
    GetVersionTable();
    my @prodlist;
    foreach my $p (sort(keys %::versions)) {
        if (defined $::proddesc{$p} && $::proddesc{$p} eq '0') {
            # Special hack.  If we stuffed a "0" into proddesc, that means
            # that disallownew was set for this bug, and so we don't want
            # to allow people to specify that product here.
            next;
        }
        if(Param("usebuggroupsentry")
           && GroupExists($p)
           && !UserInGroup($p)) {
          # If we're using bug groups to restrict entry on products, and
          # this product has a bug group, and the user is not in that
          # group, we don't want to include that product in this list.
          next;
        }
        push(@prodlist, $p);
    }
    if (0 == @prodlist) {
        print "Content-type: text/html\n\n";
        PutHeader("No Products Available");

        print "Either no products have been defined to enter bugs against ";
        print "or you have not been given access to any.  Please email ";
        print "<A HREF=\"mailto:" . Param("maintainer") . "\">";
        print Param("maintainer") . "</A> if you feel this is in error.<P>\n";

        PutFooter();
        exit;
    } elsif (1 < @prodlist) {
        print "Content-type: text/html\n\n";
        PutHeader("Enter Bug");
        
        print "<H2>First, you must pick a product on which to enter\n";
        print "a bug.</H2>\n";
        print "<table>";
        foreach my $p (@prodlist) {
# removed $::proddesc{$p} eq '0' check and UserInGroup($p) check from here
# because it's redundant.  See the foreach loop above that created @prodlist.
# 1/13/01 - dave@intrec.com
            print "<tr><th align=right valign=top><a href=\"enter_bug.cgi?product=" . url_quote($p) . "\">$p</a>:</th>\n";
            if (defined $::proddesc{$p}) {
                print "<td valign=top>$::proddesc{$p}</td>\n";
            }
            print "</tr>";
        }
        print "</table>\n";
        PutFooter();
        exit;
    }
    $::FORM{'product'} = $prodlist[0];
}

my $product = $::FORM{'product'};

confirm_login();

print "Content-type: text/html\n\n";

sub formvalue {
    my ($name, $default) = (@_);
    if (exists $::FORM{$name}) {
        return $::FORM{$name};
    }
    if (defined $default) {
        return $default;
    }
    return "";
}

sub pickplatform {
    my $value = formvalue("rep_platform");
    if ($value ne "") {
        return $value;
    }
    if ( Param('usebrowserinfo') ) {
        for ($ENV{'HTTP_USER_AGENT'}) {
        #PowerPC
            /\(.*PowerPC.*\)/i && do {return "Macintosh";};
            /\(.*PPC.*\)/ && do {return "Macintosh";};
            /\(.*AIX.*\)/ && do {return "Macintosh";};
        #Intel x86
            /\(.*[ix0-9]86.*\)/ && do {return "PC";};
        #Versions of Windows that only run on Intel x86
            /\(.*Windows 9.*\)/ && do {return "PC";};
            /\(.*Win9.*\)/ && do {return "PC";};
            /\(.*Windows 3.*\)/ && do {return "PC";};
            /\(.*Win16.*\)/ && do {return "PC";};
        #Sparc
            /\(.*sparc.*\)/ && do {return "Sun";};
            /\(.*sun4.*\)/ && do {return "Sun";};
        #Alpha
            /\(.*Alpha.*\)/i && do {return "DEC";};
        #MIPS
            /\(.*IRIX.*\)/i && do {return "SGI";};
            /\(.*MIPS.*\)/i && do {return "SGI";};
        #68k
            /\(.*68K.*\)/ && do {return "Macintosh";};
            /\(.*680[x0]0.*\)/ && do {return "Macintosh";};
        #ARM
#            /\(.*ARM.*\) && do {return "ARM";};
        #Stereotypical and broken
            /\(.*Macintosh.*\)/ && do {return "Macintosh";};
            /\(.*Mac OS [89].*\)/ && do {return "Macintosh";};
            /\(Win.*\)/ && do {return "PC";};
            /\(.*Windows NT.*\)/ && do {return "PC";};
            /\(.*OSF.*\)/ && do {return "DEC";};
            /\(.*HP-?UX.*\)/i && do {return "HP";};
            /\(.*IRIX.*\)/i && do {return "SGI";};
            /\(.*(SunOS|Solaris).*\)/ && do {return "Sun";};
        #Braindead old browsers who didn't follow convention:
            /Amiga/ && do {return "Macintosh";};
        }
    }
    # default
    return "Other";
}



sub pickversion {
    my $version = formvalue('version');

    if ( Param('usebrowserinfo') ) {
        if ($version eq "") {
            if ($ENV{'HTTP_USER_AGENT'} =~ m@Mozilla[ /]([^ ]*)@) {
                $version = $1;
            }
        }
    }
    
    if (lsearch($::versions{$product}, $version) >= 0) {
        return $version;
    } else {
        if (defined $::COOKIE{"VERSION-$product"}) {
            if (lsearch($::versions{$product},
                        $::COOKIE{"VERSION-$product"}) >= 0) {
                return $::COOKIE{"VERSION-$product"};
            }
        }
    }
    return $::versions{$product}->[0];
}


sub pickcomponent {
    my $result =formvalue('component');
    if ($result ne "" && lsearch($::components{$product}, $result) < 0) {
        $result = "";
    }
    return $result;
}


sub pickos {
    if (formvalue('op_sys') ne "") {
        return formvalue('op_sys');
    }
    if ( Param('usebrowserinfo') ) {
        for ($ENV{'HTTP_USER_AGENT'}) {
            /\(.*IRIX.*\)/ && do {return "IRIX";};
            /\(.*OSF.*\)/ && do {return "OSF/1";};
            /\(.*Linux.*\)/ && do {return "Linux";};
            /\(.*SunOS 5.*\)/ && do {return "Solaris";};
            /\(.*SunOS.*\)/ && do {return "SunOS";};
            /\(.*HP-?UX.*\)/ && do {return "HP-UX";};
            /\(.*BSD\/OS.*\)/ && do {return "BSDI";};
            /\(.*FreeBSD.*\)/ && do {return "FreeBSD";};
            /\(.*OpenBSD.*\)/ && do {return "OpenBSD";};
            /\(.*NetBSD.*\)/ && do {return "NetBSD";};
            /\(.*BeOS.*\)/ && do {return "BeOS";};
            /\(.*AIX.*\)/ && do {return "AIX";};
            /\(.*IBM.*\)/ && do {return "OS/2";};
            /\(.*QNX.*\)/ && do {return "Neutrino";};
            /\(.*VMS.*\)/ && do {return "OpenVMS";};
#            /\(.*Windows XP.*\)/ && do {return "Windows XP";};
#            /\(.*Windows NT 5\.1.*\)/ && do {return "Windows XP";};
            /\(.*Windows 2000.*\)/ && do {return "Windows 2000";};
            /Windows NT 5.*\)/ && do {return "Windows 2000";};
            /\(Windows.*NT/ && do {return "Windows NT";};
            /\(.*Win.*98.*4\.9.*\)/ && do {return "Windows ME";};
            /\(.*Win98.*\)/ && do {return "Windows 98";};
            /\(.*Win95.*\)/ && do {return "Windows 95";};
            /\(.*Win16.*\)/ && do {return "Windows 3.1";};
            /\(.*WinNT.*\)/ && do {return "Windows NT";};
            /\(.*32bit.*\)/ && do {return "Windows 95";};
            /\(.*16bit.*\)/ && do {return "Windows 3.1";};
            /\(.*Macintosh.*\)/ && do {return "Macintosh";};
            /\(.*Mac OS 9.*\)/ && do {return "Mac System 9.x";};
            /\(.*Mac OS 8\.6.*\)/ && do {return "Mac System 8.6";};
            /\(.*Mac OS 8.*\)/ && do {return "Mac System 8.5";};
#evil
            /Amiga/i && do {return "other";};
            /\(.*68K.*\)/ && do {return "Mac System 8.5";};
            /\(.*PPC.*\)/ && do {return "Mac System 8.5";};
        }
    }
    # default
    return "other";
}


GetVersionTable();

my $assign_element = GeneratePersonInput('assigned_to', 1,
                                         formvalue('assigned_to'));
my $cc_element = GeneratePeopleInput('cc', formvalue('cc'));


my $priority = Param('defaultpriority');

my $priority_popup = make_popup('priority', \@::legal_priority,
                                formvalue('priority', $priority), 0);
my $sev_popup = make_popup('bug_severity', \@::legal_severity,
                           formvalue('bug_severity', 'normal'), 0);
my $platform_popup = make_popup('rep_platform', \@::legal_platform,
                                pickplatform(), 0);
my $opsys_popup = make_popup('op_sys', \@::legal_opsys, pickos(), 0);

if (0 == @{$::components{$product}}) {
        print "<H1>Permission Denied</H1>\n";
        print "Sorry.  You need to have at least one component for this product\n";
        print "in order to create a new bug.  Go to the \"Components\" link to create\n";
        print "a new component\n";
        print "<P>\n";
        PutFooter();
        exit;
} elsif (1 == @{$::components{$product}}) {
    # Only one component; just pick it.
    $::FORM{'component'} = $::components{$product}->[0];
}

my $component_popup = make_popup('component', $::components{$product},
                                 formvalue('component'), 1);

PutHeader ("Enter Bug","Enter Bug","This page lets you enter a new bug into Bugzilla.");

# Modified, -JMR, 2/24,00
# If the usebuggroupsentry parameter is set, we need to check and make sure
# that the user has permission to enter a bug against this product.
# Modified, -DDM, 3/11/00
# added GroupExists check so we don't choke on a groupless product
if(Param("usebuggroupsentry")
   && GroupExists($product)
   && !UserInGroup($product)) {
  print "<H1>Permission denied.</H1>\n";
  print "Sorry; you do not have the permissions necessary to enter\n";
  print "a bug against this product.\n";
  print "<P>\n";
  PutFooter();
  exit;
}

# Modified, -JMR, 2/18/00
# I'm putting in a select box in order to select whether to restrict this bug to
# the product's bug group or not, if the usebuggroups parameter is set, and if
# this product has a bug group.  This box will default to selected, but can be
# turned off if this bug should be world-viewable for some reason.
#
# To do this, I need to (1) get the bit and description for the bug group from
# the database, (2) insert the select box in the giant print statements below,
# and (3) update post_bug.cgi to process the additional input field.

# Modified, -DDM, 3/11/00
# Only need the bit here, and not the description.  Description is gotten
# when the select boxes for all the groups this user has access to are read
# in later on.
# First we get the bit and description for the group.
my $group_bit=0;
if(Param("usebuggroups") && GroupExists($product)) {
    SendSQL("select bit from groups ".
            "where name = ".SqlQuote($product)." ".
            "and isbuggroup != 0");
    ($group_bit) = FetchSQLData();
}

print "
<FORM METHOD=POST ACTION=\"post_bug.cgi\">
<INPUT TYPE=HIDDEN NAME=reporter VALUE=\"$::COOKIE{'Bugzilla_login'}\">
<INPUT TYPE=HIDDEN NAME=product VALUE=\""  . value_quote($product) . "\">
  <TABLE CELLSPACING=2 CELLPADDING=0 BORDER=0>";

if (Param("entryheaderhtml")){
  print "
  <TR>
    <td></td>
    <td colspan=3>" .
  Param("entryheaderhtml") . "\n" .
  " </td> 
  </TR>
  <TR><td><br></td></TR>";
}

print "
  <TR>
    <td ALIGN=right valign=top><B>Reporter:</B></td>
    <td valign=top>$::COOKIE{'Bugzilla_login'}</td>
    <td ALIGN=right valign=top><B>Product:</B></td>
    <td valign=top>$product</td>
  </TR>
  <TR>
    <td ALIGN=right valign=top><B>Version:</B></td>
    <td>" . Version_element(pickversion(), $product) . "</td>
    <td align=right valign=top><b><a href=\"describecomponents.cgi?product=" .
    url_quote($product) . "\">Component:</a></b></td>
    <td>$component_popup</td>
  </TR>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <TR>
    <td align=right><B><A HREF=\"bug_status.html#rep_platform\">Platform:</A></B></td>
    <TD>$platform_popup</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#op_sys\">OS:</A></B></TD>
    <TD>$opsys_popup</TD>
    <td align=right valign=top></td>
    <td rowspan=3></td>
    <td></td>
  </TR>
  <TR>";
if (Param('letsubmitterchoosepriority')) {
    print "
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#priority\">Resolution<br>Priority</A>:</B></TD>
    <TD>$priority_popup</TD>";
} else {
    print '<INPUT TYPE=HIDDEN NAME=priority VALUE="' .
        value_quote($priority) . '">';
}
print "
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#severity\">Severity:</A></B></TD>
    <TD>$sev_popup</TD>
    <td></td>
    <td></td>
  </TR>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
";

if (UserInGroup("editbugs") || UserInGroup("canconfirm")) {
    SendSQL("SELECT votestoconfirm FROM products WHERE product = " .
            SqlQuote($product));
    if (FetchOneColumn()) {
        print qq{
  <TR>
    <TD ALIGN="right"><B><A HREF="bug_status.html#status">Initial state:</B></A></TD>
    <TD COLSPAN="5">
};
        print BuildPulldown("bug_status",
                            [[$::unconfirmedstate], ["NEW"]],
                            "NEW");
        print "</TD></TR>";
    }
}


print "
  <tr>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#assigned_to\">Assigned To:</A></B></TD>
    <TD colspan=5>$assign_element
    (Leave blank to assign to default component owner)</td>
  </tr>
  <tr>
    <TD ALIGN=RIGHT><B>Cc:</B></TD>
    <TD colspan=5>$cc_element</TD>
  </tr>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <TR>
    <TD ALIGN=RIGHT><B>URL:</B>
    <TD COLSPAN=5>
      <INPUT NAME=bug_file_loc SIZE=60 value=\"http://" .
    value_quote(formvalue('bug_file_loc')) .
    "\"></TD>
  </TR>
  <TR>
    <TD ALIGN=RIGHT><B>Summary:</B>
    <TD COLSPAN=5>
      <INPUT NAME=short_desc SIZE=60 value=\"" .
    value_quote(formvalue('short_desc')) .
    "\"></TD>
  </TR>
  <tr><td align=right valign=top><B>Description:</b></td>
<!--  </tr> <tr> -->
    <td colspan=5><TEXTAREA WRAP=HARD NAME=comment ROWS=10 COLS=80>" .
    value_quote(formvalue('comment')) .
    "</TEXTAREA><BR></td>
  </tr>";

print "
  <tr>
   <td></td><td colspan=5>
";

if ($::usergroupset ne '0') {
    SendSQL("SELECT bit, name, description FROM groups " .
            "WHERE bit & $::usergroupset != 0 " .
            "  AND isbuggroup != 0 AND isactive = 1 ORDER BY description");
    # We only print out a header bit for this section if there are any
    # results.
    my $groupFound = 0;
    while (MoreSQLData()) {
        my ($bit, $prodname, $description) = (FetchSQLData());
        # Don't want to include product groups other than this product.
        unless(($prodname eq $product) || (!defined($::proddesc{$prodname}))) {
            next;
        }
        if(!$groupFound) {
          print "<br><b>Only users in the selected groups can view this bug:</b><br>\n";
          print "<font size=\"-1\">(Leave all boxes unchecked to make this a public bug.)</font><br><br>\n";
          $groupFound = 1;
        }
        # Rather than waste time with another Param check and another database
        # access, $group_bit will only have a non-zero value if we're using
        # bug groups and have  one for this product, so I'll check on that
        # instead here.  -JMR, 2/18/00
        # Moved this check to this location to fix conflict with existing
        # select-box patch.  Also, if $group_bit is 0, it won't match the
        # current group, either, so I'll compare it to the current bit
        # instead of checking for non-zero. -DDM, 3/11/00
        # Modifying this to use checkboxes instead of a select list.
        # -JMR, 5/11/01
        # If this is the group for this product, make it checked.
        my $check = ($group_bit == $bit);
        # If this is a bookmarked template, then we only want to set the bit
        # for those bits set in the template.
        if(formvalue("maketemplate","") eq "Remember values as bookmarkable template") {
          $check = formvalue("bit-$bit",0);
        }
        my $checked = $check ? " CHECKED" : "";
        # indent these a bit
        print "&nbsp;&nbsp;&nbsp;&nbsp;";
        print "<input type=checkbox name=\"bit-$bit\" value=1$checked>\n";
        print "$description<br>\n";
    }
}

print "
   </td>
  </tr>
  <tr>
    <td></td>
    <td colspan=5>
       <INPUT TYPE=\"submit\" VALUE=\"    Commit    \" ONCLICK=\"if (this.form.short_desc.value =='') { alert('Please enter a summary sentence for this bug.'); return false; }\">
       &nbsp;&nbsp;&nbsp;&nbsp;
       <INPUT TYPE=\"reset\" VALUE=\"Reset\">
       &nbsp;&nbsp;&nbsp;&nbsp;
       <INPUT TYPE=\"submit\" NAME=maketemplate VALUE=\"Remember values as bookmarkable template\">
    </td>
  </tr>";

if ( Param('usebrowserinfo') ) {
    print "
  <tr>
    <td></td>
    <td colspan=3>
     <br>
     Some fields initialized from your user-agent, 
     <b>$ENV{'HTTP_USER_AGENT'}</b>.  If you think it got it wrong, 
     please tell " . Param('maintainer') . " what it should have been.
    </td>
  </tr>";
}
print "
  </TABLE>
  <INPUT TYPE=hidden name=form_name VALUE=enter_bug>
</FORM>\n";

PutFooter();

print "</BODY></HTML>\n";

