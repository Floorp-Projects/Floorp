#!/usr/bin/perl -wT
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
#                 Gervase Markham <gerv@gerv.net>
#                 Shane H. W. Travis <travis@sedsystems.ca>

##############################################################################
#
# enter_bug.cgi
# -------------
# Displays bug entry form. Bug fields are specified through popup menus, 
# drop-down lists, or text fields. Default for these values can be 
# passed in as parameters to the cgi.
#
##############################################################################

use strict;

use lib qw(.);

use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::Bug;
use Bugzilla::User;
require "globals.pl";

use vars qw(
  $template
  $vars
  @enterable_products
  @legal_opsys
  @legal_platform
  @legal_priority
  @legal_severity
  @legal_keywords
  $userid
  %versions
  %target_milestone
  $proddesc
  $classdesc
);

# If we're using bug groups to restrict bug entry, we need to know who the 
# user is right from the start. 
Bugzilla->login(LOGIN_REQUIRED) if AnyEntryGroups();

my $cloned_bug;
my $cloned_bug_id;

my $cgi = Bugzilla->cgi;

my $product = $cgi->param('product');

if (!defined $product || $product eq "") {
    GetVersionTable();
    Bugzilla->login();

   if ( ! Param('useclassification') ) {
      # just pick the default one
      $cgi->param(-name => 'classification', -value => (keys %::classdesc)[0]);
   }

   if (!$cgi->param('classification')) {
       my %classdesc;
       my %classifications;
    
       foreach my $c (GetSelectableClassifications()) {
           $classdesc{$c} = $::classdesc{$c};
           $classifications{$c} = $::classifications{$c};
       }

       my $classification_size = scalar(keys %classdesc);
       if ($classification_size == 0) {
           ThrowUserError("no_products");
       } 
       elsif ($classification_size > 1) {
           $vars->{'classdesc'} = \%classdesc;
           $vars->{'classifications'} = \%classifications;

           $vars->{'target'} = "enter_bug.cgi";
           $vars->{'format'} = $cgi->param('format');
           
           $vars->{'cloned_bug_id'} = $cgi->param('cloned_bug_id');

           print $cgi->header();
           $template->process("global/choose-classification.html.tmpl", $vars)
             || ThrowTemplateError($template->error());
           exit;        
       }
       $cgi->param(-name => 'classification', -value => (keys %classdesc)[0]);
   }

    my %products;
    foreach my $p (@enterable_products) {
        if (CanEnterProduct($p)) {
            if (IsInClassification(scalar $cgi->param('classification'),$p) ||
                $cgi->param('classification') eq "__all") {
                $products{$p} = $::proddesc{$p};
            }
        }
    }
 
    my $prodsize = scalar(keys %products);
    if ($prodsize == 0) {
        ThrowUserError("no_products");
    } 
    elsif ($prodsize > 1) {
        my %classifications;
        if ( ! Param('useclassification') ) {
            @{$classifications{"all"}} = keys %products;
        }
        elsif ($cgi->param('classification') eq "__all") {
            %classifications = %::classifications;
        } else {
            $classifications{$cgi->param('classification')} =
                $::classifications{$cgi->param('classification')};
        }
        $vars->{'proddesc'} = \%products;
        $vars->{'classifications'} = \%classifications;
        $vars->{'classdesc'} = \%::classdesc;

        $vars->{'target'} = "enter_bug.cgi";
        $vars->{'format'} = $cgi->param('format');

        $vars->{'cloned_bug_id'} = $cgi->param('cloned_bug_id');
        
        print $cgi->header();
        $template->process("global/choose-product.html.tmpl", $vars)
          || ThrowTemplateError($template->error());
        exit;        
    } else {
        # Only one product exists
        $product = (keys %products)[0];
    }
}

##############################################################################
# Useful Subroutines
##############################################################################
sub formvalue {
    my ($name, $default) = (@_);
    return $cgi->param($name) || $default || "";
}

sub pickplatform {
    return formvalue("rep_platform") if formvalue("rep_platform");

    if (Param('defaultplatform')) {
        return Param('defaultplatform');
    } else {
        for ($ENV{'HTTP_USER_AGENT'}) {
        #PowerPC
            /\(.*PowerPC.*\)/i && do {return "Macintosh";};
            /\(.*PPC.*\)/ && do {return "Macintosh";};
            /\(.*AIX.*\)/ && do {return "Macintosh";};
        #Intel x86
            /\(.*[ix0-9]86.*\)/ && do {return "PC";};
        #Versions of Windows that only run on Intel x86
            /\(.*Win(?:dows )[39M].*\)/ && do {return "PC";};
            /\(.*Win(?:dows )16.*\)/ && do {return "PC";};
        #Sparc
            /\(.*sparc.*\)/ && do {return "Sun";};
            /\(.*sun4.*\)/ && do {return "Sun";};
        #Alpha
            /\(.*AXP.*\)/i && do {return "DEC";};
            /\(.*[ _]Alpha.\D/i && do {return "DEC";};
            /\(.*[ _]Alpha\)/i && do {return "DEC";};
        #MIPS
            /\(.*IRIX.*\)/i && do {return "SGI";};
            /\(.*MIPS.*\)/i && do {return "SGI";};
        #68k
            /\(.*68K.*\)/ && do {return "Macintosh";};
            /\(.*680[x0]0.*\)/ && do {return "Macintosh";};
        #HP
            /\(.*9000.*\)/ && do {return "HP";};
        #ARM
#            /\(.*ARM.*\) && do {return "ARM";};
        #Stereotypical and broken
            /\(.*Macintosh.*\)/ && do {return "Macintosh";};
            /\(.*Mac OS [89].*\)/ && do {return "Macintosh";};
            /\(Win.*\)/ && do {return "PC";};
            /\(.*Win(?:dows[ -])NT.*\)/ && do {return "PC";};
            /\(.*OSF.*\)/ && do {return "DEC";};
            /\(.*HP-?UX.*\)/i && do {return "HP";};
            /\(.*IRIX.*\)/i && do {return "SGI";};
            /\(.*(SunOS|Solaris).*\)/ && do {return "Sun";};
        #Braindead old browsers who didn't follow convention:
            /Amiga/ && do {return "Macintosh";};
            /WinMosaic/ && do {return "PC";};
        }
        return "Other";
    }
}

sub pickos {
    if (formvalue('op_sys') ne "") {
        return formvalue('op_sys');
    }
    if (Param('defaultopsys')) {
        return Param('defaultopsys');
    } else {
        for ($ENV{'HTTP_USER_AGENT'}) {
            /\(.*IRIX.*\)/ && do {return "IRIX";};
            /\(.*OSF.*\)/ && do {return "OSF/1";};
            /\(.*Linux.*\)/ && do {return "Linux";};
            /\(.*Solaris.*\)/ && do {return "Solaris";};
            /\(.*SunOS 5.*\)/ && do {return "Solaris";};
            /\(.*SunOS.*sun4u.*\)/ && do {return "Solaris";};
            /\(.*SunOS.*\)/ && do {return "SunOS";};
            /\(.*HP-?UX.*\)/ && do {return "HP-UX";};
            /\(.*BSD\/(?:OS|386).*\)/ && do {return "BSDI";};
            /\(.*FreeBSD.*\)/ && do {return "FreeBSD";};
            /\(.*OpenBSD.*\)/ && do {return "OpenBSD";};
            /\(.*NetBSD.*\)/ && do {return "NetBSD";};
            /\(.*BeOS.*\)/ && do {return "BeOS";};
            /\(.*AIX.*\)/ && do {return "AIX";};
            /\(.*OS\/2.*\)/ && do {return "OS/2";};
            /\(.*QNX.*\)/ && do {return "Neutrino";};
            /\(.*VMS.*\)/ && do {return "OpenVMS";};
            /\(.*Windows XP.*\)/ && do {return "Windows XP";};
            /\(.*Windows NT 5\.2.*\)/ && do {return "Windows Server 2003";};
            /\(.*Windows NT 5\.1.*\)/ && do {return "Windows XP";};
            /\(.*Windows 2000.*\)/ && do {return "Windows 2000";};
            /\(.*Windows NT 5.*\)/ && do {return "Windows 2000";};
            /\(.*Win.*9[8x].*4\.9.*\)/ && do {return "Windows ME";};
            /\(.*Win(?:dows )M[Ee].*\)/ && do {return "Windows ME";};
            /\(.*Win(?:dows )98.*\)/ && do {return "Windows 98";};
            /\(.*Win(?:dows )95.*\)/ && do {return "Windows 95";};
            /\(.*Win(?:dows )16.*\)/ && do {return "Windows 3.1";};
            /\(.*Win(?:dows[ -])NT.*\)/ && do {return "Windows NT";};
            /\(.*Windows.*NT.*\)/ && do {return "Windows NT";};
            /\(.*32bit.*\)/ && do {return "Windows 95";};
            /\(.*16bit.*\)/ && do {return "Windows 3.1";};
            /\(.*Mac OS 9.*\)/ && do {return "Mac System 9.x";};
            /\(.*Mac OS 8\.6.*\)/ && do {return "Mac System 8.6";};
            /\(.*Mac OS 8\.5.*\)/ && do {return "Mac System 8.5";};
        # Bugzilla doesn't have an entry for 8.1
            /\(.*Mac OS 8\.1.*\)/ && do {return "Mac System 8.0";};
            /\(.*Mac OS 8\.0.*\)/ && do {return "Mac System 8.0";};
            /\(.*Mac OS 8[^.].*\)/ && do {return "Mac System 8.0";};
            /\(.*Mac OS 8.*\)/ && do {return "Mac System 8.6";};
            /\(.*Mac OS X.*\)/ && do {return "Mac OS X 10.0";};
            /\(.*Darwin.*\)/ && do {return "Mac OS X 10.0";};
        # Silly
            /\(.*Mac.*PowerPC.*\)/ && do {return "Mac System 9.x";};
            /\(.*Mac.*PPC.*\)/ && do {return "Mac System 9.x";};
            /\(.*Mac.*68k.*\)/ && do {return "Mac System 8.0";};
        # Evil
            /Amiga/i && do {return "other";};
            /WinMosaic/ && do {return "Windows 95";};
            /\(.*PowerPC.*\)/ && do {return "Mac System 9.x";};
            /\(.*PPC.*\)/ && do {return "Mac System 9.x";};
            /\(.*68K.*\)/ && do {return "Mac System 8.0";};
        }
        return "other";
    }
}
##############################################################################
# End of subroutines
##############################################################################

Bugzilla->login(LOGIN_REQUIRED) if (!(AnyEntryGroups()));

# If a user is trying to clone a bug
#   Check that the user has authorization to view the parent bug
#   Create an instance of Bug that holds the info from the parent
$cloned_bug_id = $cgi->param('cloned_bug_id');

if ($cloned_bug_id) {
    ValidateBugID($cloned_bug_id);
    $cloned_bug = new Bugzilla::Bug($cloned_bug_id, $userid);
}

# We need to check and make sure
# that the user has permission to enter a bug against this product.
CanEnterProductOrWarn($product);

GetVersionTable();

my $product_id = get_product_id($product);

if (0 == @{$::components{$product}}) {        
    ThrowUserError("no_components", {product => $product});   
} 
elsif (1 == @{$::components{$product}}) {
    # Only one component; just pick it.
    $cgi->param('component', $::components{$product}->[0]);
}

my @components;

my $dbh = Bugzilla->dbh;
my $sth = $dbh->prepare(
       q{SELECT name, description, p1.login_name, p2.login_name 
           FROM components 
      LEFT JOIN profiles p1 ON components.initialowner = p1.userid
      LEFT JOIN profiles p2 ON components.initialqacontact = p2.userid
          WHERE product_id = ?
          ORDER BY name});

$sth->execute($product_id);
while (my ($name, $description, $owner, $qacontact)
       = $sth->fetchrow_array()) {
    push @components, {
        name => $name,
        description => $description,
        initialowner => $owner,
        initialqacontact => $qacontact || '',
    };
}

my %default;

$vars->{'product'}               = $product;
$vars->{'component_'}            = \@components;

$vars->{'priority'}              = \@legal_priority;
$vars->{'bug_severity'}          = \@legal_severity;
$vars->{'rep_platform'}          = \@legal_platform;
$vars->{'op_sys'}                = \@legal_opsys; 

$vars->{'use_keywords'}          = 1 if (@::legal_keywords);

$vars->{'assigned_to'}           = formvalue('assigned_to');
$vars->{'assigned_to_disabled'}  = !UserInGroup('editbugs');
$vars->{'cc_disabled'}           = 0;

$vars->{'qa_contact'}           = formvalue('qa_contact');
$vars->{'qa_contact_disabled'}  = !UserInGroup('editbugs');

$vars->{'cloned_bug_id'}         = $cloned_bug_id;

if ($cloned_bug_id) {

    $default{'component_'}    = $cloned_bug->{'component'};
    $default{'priority'}      = $cloned_bug->{'priority'};
    $default{'bug_severity'}  = $cloned_bug->{'bug_severity'};
    $default{'rep_platform'}  = $cloned_bug->{'rep_platform'};
    $default{'op_sys'}        = $cloned_bug->{'op_sys'};

    $vars->{'short_desc'}     = $cloned_bug->{'short_desc'};
    $vars->{'bug_file_loc'}   = $cloned_bug->{'bug_file_loc'};
    $vars->{'keywords'}       = $cloned_bug->keywords;
    $vars->{'dependson'}      = $cloned_bug_id;
    $vars->{'blocked'}        = "";
    $vars->{'deadline'}       = $cloned_bug->{'deadline'};

    if (defined $cloned_bug->cc) {
        $vars->{'cc'}         = join (" ", @{$cloned_bug->cc});
    } else {
        $vars->{'cc'}         = formvalue('cc');
    }

# We need to ensure that we respect the 'insider' status of
# the first comment, if it has one. Either way, make a note
# that this bug was cloned from another bug.

    $cloned_bug->longdescs();
    my $isprivate             = $cloned_bug->{'longdescs'}->[0]->{'isprivate'};

    $vars->{'comment'}        = "";
    $vars->{'commentprivacy'} = 0;

    if ( !($isprivate) ||
         ( ( Param("insidergroup") ) && 
           ( UserInGroup(Param("insidergroup")) ) ) 
       ) {
        $vars->{'comment'}        = $cloned_bug->{'longdescs'}->[0]->{'body'};
        $vars->{'commentprivacy'} = $isprivate;
    }

# Ensure that the groupset information is set up for later use.
    $cloned_bug->groups();

} # end of cloned bug entry form

else {

    $default{'component_'}    = formvalue('component');
    $default{'priority'}      = formvalue('priority', Param('defaultpriority'));
    $default{'bug_severity'}  = formvalue('bug_severity', Param('defaultseverity'));
    $default{'rep_platform'}  = pickplatform();
    $default{'op_sys'}        = pickos();

    $vars->{'short_desc'}     = formvalue('short_desc');
    $vars->{'bug_file_loc'}   = formvalue('bug_file_loc', "http://");
    $vars->{'keywords'}       = formvalue('keywords');
    $vars->{'dependson'}      = formvalue('dependson');
    $vars->{'blocked'}        = formvalue('blocked');
    $vars->{'deadline'}       = formvalue('deadline');

    $vars->{'cc'}             = join(', ', $cgi->param('cc'));

    $vars->{'comment'}        = formvalue('comment');
    $vars->{'commentprivacy'} = formvalue('commentprivacy');

} # end of normal/bookmarked entry form


# IF this is a cloned bug,
# AND the clone's product is the same as the parent's
#   THEN use the version from the parent bug
# ELSE IF a version is supplied in the URL
#   THEN use it
# ELSE IF there is a version in the cookie
#   THEN use it (Posting a bug sets a cookie for the current version.)
# ELSE
#   The default version is the last one in the list (which, it is
#   hoped, will be the most recent one).
#
# Eventually maybe each product should have a "current version"
# parameter.
$vars->{'version'} = $::versions{$product} || [];

if ( ($cloned_bug_id) &&
     ("$product" eq "$cloned_bug->{'product'}" ) ) {
    $default{'version'} = $cloned_bug->{'version'};
} elsif (formvalue('version')) {
    $default{'version'} = formvalue('version');
} elsif (defined $cgi->cookie("VERSION-$product") &&
    lsearch($vars->{'version'}, $cgi->cookie("VERSION-$product")) != -1) {
    $default{'version'} = $cgi->cookie("VERSION-$product");
} else {
    $default{'version'} = $vars->{'version'}->[$#{$vars->{'version'}}];
}

# Get list of milestones.
if ( Param('usetargetmilestone') ) {
    $vars->{'target_milestone'} = $::target_milestone{$product};
    if (formvalue('target_milestone')) {
       $default{'target_milestone'} = formvalue('target_milestone');
    } else {
       SendSQL("SELECT defaultmilestone FROM products WHERE " .
               "name = " . SqlQuote($product));
       $default{'target_milestone'} = FetchOneColumn();
    }
}


# List of status values for drop-down.
my @status;

# Construct the list of allowable values.  There are three cases:
# 
#  case                                 values
#  product does not have confirmation   NEW
#  confirmation, user cannot confirm    UNCONFIRMED
#  confirmation, user can confirm       NEW, UNCONFIRMED.

SendSQL("SELECT votestoconfirm FROM products WHERE name = " .
        SqlQuote($product));
if (FetchOneColumn()) {
    if (UserInGroup("editbugs") || UserInGroup("canconfirm")) {
        push(@status, "NEW");
    }
    push(@status, 'UNCONFIRMED');
} else {
    push(@status, "NEW");
}

$vars->{'bug_status'} = \@status; 

# Get the default from a template value if it is legitimate.
# Otherwise, set the default to the first item on the list.

if (formvalue('bug_status') && (lsearch(\@status, formvalue('bug_status')) >= 0)) {
    $default{'bug_status'} = formvalue('bug_status');
} else {
    $default{'bug_status'} = $status[0];
}
 
SendSQL("SELECT DISTINCT groups.id, groups.name, groups.description, " .
        "membercontrol, othercontrol " .
        "FROM groups LEFT JOIN group_control_map " .
        "ON group_id = id AND product_id = $product_id " .
        "WHERE isbuggroup != 0 AND isactive != 0 ORDER BY description");

my @groups;

while (MoreSQLData()) {
    my ($id, $groupname, $description, $membercontrol, $othercontrol) 
        = FetchSQLData();
    # Only include groups if the entering user will have an option.
    next if ((!$membercontrol) 
               || ($membercontrol == CONTROLMAPNA) 
               || ($membercontrol == CONTROLMAPMANDATORY)
               || (($othercontrol != CONTROLMAPSHOWN) 
                    && ($othercontrol != CONTROLMAPDEFAULT)
                    && (!UserInGroup($groupname)))
             );
    my $check;

    # If this is a cloned bug, 
    # AND the product for this bug is the same as for the original
    #   THEN set a group's checkbox if the original also had it on
    # ELSE IF this is a bookmarked template
    #   THEN set a group's checkbox if was set in the bookmark
    # ELSE
    #   set a groups's checkbox based on the group control map
    #
    if ( ($cloned_bug_id) &&
         ("$product" eq "$cloned_bug->{'product'}" ) ) {
        foreach my $i (0..(@{$cloned_bug->{'groups'}}-1) ) {
            if ($cloned_bug->{'groups'}->[$i]->{'bit'} == $id) {
                $check = $cloned_bug->{'groups'}->[$i]->{'ison'};
            }
        }
    }
    elsif(formvalue("maketemplate") ne "") {
        $check = formvalue("bit-$id", 0);
    }
    else {
        # Checkbox is checked by default if $control is a default state.
        $check = (($membercontrol == CONTROLMAPDEFAULT)
                 || (($othercontrol == CONTROLMAPDEFAULT)
                      && (!UserInGroup($groupname))));
    }

    my $group = 
    {
        'bit' => $id , 
        'checked' => $check , 
        'description' => $description 
    };

    push @groups, $group;        
}

$vars->{'group'} = \@groups;

$vars->{'default'} = \%default;

my $format = 
  GetFormat("bug/create/create", scalar $cgi->param('format'), 
            scalar $cgi->param('ctype'));

print $cgi->header($format->{'ctype'});
$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());          

