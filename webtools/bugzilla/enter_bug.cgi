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
require "CGI.pl";

use vars qw(
  $unconfirmedstate
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
  $proddesc
  $classdesc
);

# If we're using bug groups to restrict bug entry, we need to know who the 
# user is right from the start. 
Bugzilla->login(LOGIN_REQUIRED) if AnyEntryGroups();

my $cgi = Bugzilla->cgi;

my $product = $cgi->param('product');

if (!defined $product) {
    GetVersionTable();
    Bugzilla->login();

   if ( ! Param('useclassification') ) {
      # just pick the default one
      $::FORM{'classification'}=(keys %::classdesc)[0];
   }

   if (!defined $::FORM{'classification'}) {
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
           $vars->{'format'} = $::FORM{'format'};
           
           print "Content-type: text/html\n\n";
           $template->process("global/choose-classification.html.tmpl", $vars)
             || ThrowTemplateError($template->error());
           exit;        
       }
       $::FORM{'classification'} = (keys %classdesc)[0];
       $::MFORM{'classification'} = [$::FORM{'classification'}];
   }

    my %products;
    foreach my $p (@enterable_products) {
        if (CanEnterProduct($p)) {
            if (IsInClassification($::FORM{'classification'},$p) ||
                $::FORM{'classification'} eq "__all") {
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
        elsif ($::FORM{'classification'} eq "__all") {
            %classifications = %::classifications;
        } else {
            $classifications{$::FORM{'classification'}} =
                $::classifications{$::FORM{'classification'}};
        }
        $vars->{'proddesc'} = \%products;
        $vars->{'classifications'} = \%classifications;
        $vars->{'classdesc'} = \%::classdesc;

        $vars->{'target'} = "enter_bug.cgi";
        $vars->{'format'} = $cgi->param('format');
        
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

# We need to check and make sure
# that the user has permission to enter a bug against this product.
if(!CanEnterProduct($product))
{
    ThrowUserError("entry_access_denied", { product => $product});         
}

GetVersionTable();

if (lsearch(\@::enterable_products, $product) == -1) {
    ThrowUserError("invalid_product_name", { product => $product});
}

my $product_id = get_product_id($product);

if (0 == @{$::components{$product}}) {        
    ThrowUserError("no_components", {product => $product});   
} 
elsif (1 == @{$::components{$product}}) {
    # Only one component; just pick it.
    $cgi->param('component', $::components{$product}->[0]);
}

my @components;
SendSQL("SELECT name, description, login_name, realname
             FROM components, profiles
             WHERE product_id = $product_id
             AND initialowner=userid
             ORDER BY name");
while (MoreSQLData()) {
    my ($name, $description, $login, $realname) = FetchSQLData();

    push @components, {
        name => $name,
        description => $description,
        default_login => $login,
        default_realname => $realname,
    };
}

my %default;

$vars->{'component_'} = \@components;
$default{'component_'} = formvalue('component');

$vars->{'assigned_to'} = formvalue('assigned_to');
$vars->{'cc'} = formvalue('cc');
$vars->{'product'} = $product;
$vars->{'bug_file_loc'} = formvalue('bug_file_loc', "http://");
$vars->{'short_desc'} = formvalue('short_desc');
$vars->{'comment'} = formvalue('comment');

$vars->{'priority'} = \@legal_priority;
$default{'priority'} = formvalue('priority', Param('defaultpriority'));

$vars->{'bug_severity'} = \@legal_severity;
$default{'bug_severity'} = formvalue('bug_severity', 'normal');

$vars->{'rep_platform'} = \@legal_platform;
$default{'rep_platform'} = pickplatform();

$vars->{'op_sys'} = \@legal_opsys; 
$default{'op_sys'} = pickos();

$vars->{'keywords'} = formvalue('keywords');
$vars->{'dependson'} = formvalue('dependson');
$vars->{'blocked'} = formvalue('blocked');

# Use the version specified in the URL, if one is supplied. If not,
# then use the cookie-specified value. (Posting a bug sets a cookie
# for the current version.) If no URL or cookie version, the default
# version is the last one in the list (hopefully the latest one).
# Eventually maybe each product should have a "current version"
# parameter.
$vars->{'version'} = $::versions{$product} || [];
if (formvalue('version')) {
    $default{'version'} = formvalue('version');
} elsif (defined $cgi->cookie("VERSION-$product") &&
    lsearch($vars->{'version'}, $cgi->cookie("VERSION-$product")) != -1) {
    $default{'version'} = $cgi->cookie("VERSION-$product");
} else {
    $default{'version'} = $vars->{'version'}->[$#{$vars->{'version'}}];
}

# There must be at least one status in @status.
my @status = "NEW";

if (UserInGroup("editbugs") || UserInGroup("canconfirm")) {
    SendSQL("SELECT votestoconfirm FROM products WHERE name = " .
            SqlQuote($product));
    push(@status, $unconfirmedstate) if (FetchOneColumn());
}

$vars->{'bug_status'} = \@status; 
$default{'bug_status'} = $status[0];

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

    # If this is the group for this product, make it checked.
    if(formvalue("maketemplate") ne "") 
    {
        # If this is a bookmarked template, then we only want to set the
        # bit for those bits set in the template.        
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

$vars->{'use_keywords'} = 1 if (@::legal_keywords);

my $format = 
  GetFormat("bug/create/create", scalar $cgi->param('format'), 
            scalar $cgi->param('ctype'));

print $cgi->header($format->{'ctype'});
$template->process($format->{'template'}, $vars)
  || ThrowTemplateError($template->error());          

