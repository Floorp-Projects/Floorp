#!/usr/bonsaitools/bin/perl -w
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

use diagnostics;
use strict;

require "CGI.pl";

# Shut up misguided -w warnings about "used only once":
use vars @::legal_platform,
    @::buffer,
    @::legal_severity,
    @::legal_opsys,
    @::legal_priority;


if (!defined $::FORM{'product'}) {
    GetVersionTable();
    my @prodlist = keys %::versions;
    if ($#prodlist != 0) {
        print "Content-type: text/html\n\n";
        PutHeader("Enter Bug");
        
        print "<H2>First, you must pick a product on which to enter\n";
        print "a bug.</H2>\n";
        print "<table>";
        foreach my $p (sort (@prodlist)) {
            print "<tr><th align=right valign=top><a href=\"enter_bug.cgi?product=" . url_quote($p) . "\"&$::buffer>$p</a>:</th>\n";
            if (defined $::proddesc{$p}) {
                print "<td valign=top>$::proddesc{$p}</td>\n";
            }
            print "</tr>";
        }
        print "</table>\n";
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
    for ($ENV{'HTTP_USER_AGENT'}) {
        /Mozilla.*\(Windows/ && do {return "PC";};
        /Mozilla.*\(Macintosh/ && do {return "Macintosh";};
        /Mozilla.*\(Win/ && do {return "PC";};
        /Mozilla.*Linux.*86/ && do {return "PC";};
        /Mozilla.*Linux.*alpha/ && do {return "DEC";};
        /Mozilla.*OSF/ && do {return "DEC";};
        /Mozilla.*HP-UX/ && do {return "HP";};
        /Mozilla.*IRIX/ && do {return "SGI";};
        /Mozilla.*(SunOS|Solaris)/ && do {return "Sun";};
        # default
        return "Other";
    }
}



sub pickversion {
    my $version = formvalue('version');
    if ($version eq "") {
        if ($ENV{'HTTP_USER_AGENT'} =~ m@Mozilla[ /]([^ ]*)@) {
            $version = $1;
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
    for ($ENV{'HTTP_USER_AGENT'}) {
        /Mozilla.*\(.*;.*; IRIX.*\)/    && do {return "IRIX";};
        /Mozilla.*\(.*;.*; 32bit.*\)/   && do {return "Windows 95";};
        /Mozilla.*\(.*;.*; 16bit.*\)/   && do {return "Windows 3.1";};
        /Mozilla.*\(.*;.*; 68K.*\)/     && do {return "System 7.5";};
        /Mozilla.*\(.*;.*; PPC.*\)/     && do {return "System 7.5";};
        /Mozilla.*\(.*;.*; OSF.*\)/     && do {return "OSF/1";};
        /Mozilla.*\(.*;.*; Linux.*\)/   && do {return "Linux";};
        /Mozilla.*\(.*;.*; SunOS 5.*\)/ && do {return "Solaris";};
        /Mozilla.*\(.*;.*; SunOS.*\)/   && do {return "SunOS";};
        /Mozilla.*\(.*;.*; SunOS.*\)/   && do {return "SunOS";};
        /Mozilla.*\(.*;.*; BSD\/OS.*\)/ && do {return "BSDI";};
        /Mozilla.*\(Win16.*\)/          && do {return "Windows 3.1";};
        /Mozilla.*\(Win95.*\)/          && do {return "Windows 95";};
        /Mozilla.*\(WinNT.*\)/          && do {return "Windows NT";};
        # default
        return "other";
    }
}


GetVersionTable();

my $assign_element = GeneratePersonInput('assigned_to', 1,
                                         formvalue('assigned_to'));
my $cc_element = GeneratePeopleInput('cc', formvalue('cc'));


my $priority_popup = make_popup('priority', \@::legal_priority,
                                formvalue('priority', 'P2'), 0);
my $sev_popup = make_popup('bug_severity', \@::legal_severity,
                           formvalue('bug_severity', 'normal'), 0);
my $platform_popup = make_popup('rep_platform', \@::legal_platform,
                                pickplatform(), 0);
my $opsys_popup = make_popup('op_sys', \@::legal_opsys, pickos(), 0);

my $component_popup = make_popup('component', $::components{$product},
                                 formvalue('component'), 1);

PutHeader ("Enter Bug");

print "
<FORM NAME=enterForm METHOD=POST ACTION=\"post_bug.cgi\">
<INPUT TYPE=HIDDEN NAME=bug_status VALUE=NEW>
<INPUT TYPE=HIDDEN NAME=reporter VALUE=$::COOKIE{'Bugzilla_login'}>
<INPUT TYPE=HIDDEN NAME=product VALUE=$product>
  <TABLE CELLSPACING=2 CELLPADDING=0 BORDER=0>
  <TR>
    <td ALIGN=right valign=top><B>Product:</B></td>
    <td valign=top>$product</td>
  </TR>
  <TR>
    <td ALIGN=right valign=top><B>Version:</B></td>
    <td>" . Version_element(pickversion(), $product) . "</td>
    <td align=right valign=top><b>Component:</b></td>
    <td>$component_popup</td>
  </TR>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <TR>
    <td align=right><b><B><A HREF=\"bug_status.html#rep_platform\">Platform:</A></B></td>
    <TD>$platform_popup</TD>
    <TD ALIGN=RIGHT><B>OS:</B></TD>
    <TD>$opsys_popup</TD>
    <td align=right valign=top></td>
    <td rowspan=3></td>
    <td></td>
  </TR>
  <TR>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#priority\">Priority</A>:</B></TD>
    <TD>$priority_popup</TD>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#severity\">Severity</A>:</B></TD>
    <TD>$sev_popup</TD>
    <td></td>
    <td></td>
  </TR>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <tr>
    <TD ALIGN=RIGHT><B><A HREF=\"bug_status.html#assigned_to\">Assigned To:
        </A></B></TD>
    <TD colspan=5>$assign_element
    (Leave blank to assign to default owner for component)</td>
  </tr>
  <tr>
    <TD ALIGN=RIGHT ><B>Cc:</B></TD>
    <TD colspan=5>$cc_element</TD>
  </tr>
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <TR>
    <TD ALIGN=RIGHT><B>URL:</B>
    <TD COLSPAN=5>
      <INPUT NAME=bug_file_loc SIZE=60 value=\"" .
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
  <tr><td>&nbsp<td> <td> <td> <td> <td> </tr>
  <tr>
    <td aligh=right valign=top><B>Description:</b>
    <td colspan=5><TEXTAREA WRAP=HARD NAME=comment ROWS=10 COLS=80>" .
    value_quote(formvalue('comment')) .
    "</TEXTAREA><BR></td>
  </tr>
  <tr>
    <td></td>
    <td colspan=5>
       <INPUT TYPE=\"submit\" VALUE=\"    Commit    \">
       &nbsp;&nbsp;&nbsp;&nbsp;
       <INPUT TYPE=\"reset\" VALUE=\"Reset\">
       &nbsp;&nbsp;&nbsp;&nbsp;
       <INPUT TYPE=\"submit\" NAME=maketemplate VALUE=\"Remember values as bookmarkable template\">
    </td>
  </tr>
  </TABLE>
  <INPUT TYPE=hidden name=form_name VALUE=enter_bug>
</FORM>

Some fields initialized from your user-agent, <b>$ENV{'HTTP_USER_AGENT'}</b>.
If you think it got it wrong, please tell " . Param('maintainer') . " what it should have been.

</BODY></HTML>";
