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
# Corporation. Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): Terry Weissman <terry@mozilla.org>
#                 David Gardiner <david.gardiner@unisa.edu.au>
#                 Matthias Radestock <matthias@sorted.org>
#                 Chris Lahey <clahey@ximian.com> [javascript fixes]
#                 Christian Reis <kiko@async.com.br> [javascript rewrite]

use diagnostics;
use strict;

require "CGI.pl";

$::CheckOptionValues = 0;       # It's OK if we have some bogus things in the
                                # pop-up lists here, from a remembered query
                                # that is no longer quite valid.  We don't
                                # want to crap out in the query page.

# Shut up misguided -w warnings about "used only once":

use vars
  @::CheckOptionValues,
  @::legal_resolution,
  @::legal_bug_status,
  @::legal_components,
  @::legal_keywords,
  @::legal_opsys,
  @::legal_platform,
  @::legal_priority,
  @::legal_product,
  @::legal_severity,
  @::legal_target_milestone,
  @::legal_versions,
  @::log_columns,
  %::versions,
  %::components,
  %::FORM;


if (defined $::FORM{"GoAheadAndLogIn"}) {
    # We got here from a login page, probably from relogin.cgi.  We better
    # make sure the password is legit.
    confirm_login();
} else {
    quietly_check_login();
}
my $userid = 0;
if (defined $::COOKIE{"Bugzilla_login"}) {
    $userid = DBNameToIdAndCheck($::COOKIE{"Bugzilla_login"});
}

# Backwards compatability hack -- if there are any of the old QUERY_*
# cookies around, and we are logged in, then move them into the database
# and nuke the cookie.
if ($userid) {
    my @oldquerycookies;
    foreach my $i (keys %::COOKIE) {
        if ($i =~ /^QUERY_(.*)$/) {
            push(@oldquerycookies, [$1, $i, $::COOKIE{$i}]);
        }
    }
    if (defined $::COOKIE{'DEFAULTQUERY'}) {
        push(@oldquerycookies, [$::defaultqueryname, 'DEFAULTQUERY',
                                $::COOKIE{'DEFAULTQUERY'}]);
    }
    if (@oldquerycookies) {
        foreach my $ref (@oldquerycookies) {
            my ($name, $cookiename, $value) = (@$ref);
            if ($value) {
                my $qname = SqlQuote($name);
                SendSQL("SELECT query FROM namedqueries " .
                        "WHERE userid = $userid AND name = $qname");
                my $query = FetchOneColumn();
                if (!$query) {
                    SendSQL("REPLACE INTO namedqueries " .
                            "(userid, name, query) VALUES " .
                            "($userid, $qname, " . SqlQuote($value) . ")");
                }
            }
            print "Set-Cookie: $cookiename= ; path=" . Param("cookiepath"). "; expires=Sun, 30-Jun-1980 00:00:00 GMT\n";
        }
    }
}
                



if ($::FORM{'nukedefaultquery'}) {
    if ($userid) {
        SendSQL("DELETE FROM namedqueries " .
                "WHERE userid = $userid AND name = '$::defaultqueryname'");
    }
    $::buffer = "";
}


my $userdefaultquery;
if ($userid) {
    SendSQL("SELECT query FROM namedqueries " .
            "WHERE userid = $userid AND name = '$::defaultqueryname'");
    $userdefaultquery = FetchOneColumn();
}

my %default;
my %type;

sub ProcessFormStuff {
    my ($buf) = (@_);
    my $foundone = 0;
    foreach my $name ("bug_status", "resolution", "assigned_to",
                      "rep_platform", "priority", "bug_severity",
                      "product", "reporter", "op_sys",
                      "component", "version", "chfield", "chfieldfrom",
                      "chfieldto", "chfieldvalue", "target_milestone",
                      "email1", "emailtype1", "emailreporter1",
                      "emailassigned_to1", "emailcc1", "emailqa_contact1",
                      "emaillongdesc1",
                      "email2", "emailtype2", "emailreporter2",
                      "emailassigned_to2", "emailcc2", "emailqa_contact2",
                      "emaillongdesc2",
                      "changedin", "votes", "short_desc", "short_desc_type",
                      "long_desc", "long_desc_type", "bug_file_loc",
                      "bug_file_loc_type", "status_whiteboard",
                      "status_whiteboard_type", "bug_id",
                      "bugidtype", "keywords", "keywords_type") {
        $default{$name} = "";
        $type{$name} = 0;
    }


    foreach my $item (split(/\&/, $buf)) {
        my @el = split(/=/, $item);
        my $name = $el[0];
        my $value;
        if ($#el > 0) {
            $value = url_decode($el[1]);
        } else {
            $value = "";
        }
        if (defined $default{$name}) {
            $foundone = 1;
            if ($default{$name} ne "") {
                $default{$name} .= "|$value";
                $type{$name} = 1;
            } else {
                $default{$name} = $value;
            }
        }
    }
    return $foundone;
}


if (!ProcessFormStuff($::buffer)) {
    # Ah-hah, there was no form stuff specified.  Do it again with the
    # default query.
    if ($userdefaultquery) {
        ProcessFormStuff($userdefaultquery);
    } else {
        ProcessFormStuff(Param("defaultquery"));
    }
}


                 

if ($default{'chfieldto'} eq "") {
    $default{'chfieldto'} = "Now";
}



print "Set-Cookie: BUGLIST=
Content-type: text/html\n\n";

GetVersionTable();

sub GenerateEmailInput {
    my ($id) = (@_);
    my $defstr = value_quote($default{"email$id"});
    my $deftype = $default{"emailtype$id"};
    if ($deftype eq "") {
        $deftype = "substring";
    }
    my $assignedto = ($default{"emailassigned_to$id"} eq "1") ? "checked" : "";
    my $reporter = ($default{"emailreporter$id"} eq "1") ? "checked" : "";
    my $cc = ($default{"emailcc$id"} eq "1") ? "checked" : "";
    my $longdesc = ($default{"emaillongdesc$id"} eq "1") ? "checked" : "";

    my $qapart = "";
    my $qacontact = "";
    if (Param("useqacontact")) {
        $qacontact = ($default{"emailqa_contact$id"} eq "1") ? "checked" : "";
        $qapart = qq|
<tr>
<td></td>
<td>
<input type="checkbox" name="emailqa_contact$id" value=1 $qacontact>QA Contact
</td>
</tr>
|;
    }
    if ($assignedto eq "" && $reporter eq "" && $cc eq "" &&
          $qacontact eq "") {
        if ($id eq "1") {
            $assignedto = "checked";
        } else {
            $reporter = "checked";
        }
    }


    $default{"emailtype$id"} ||= "substring";

    return qq{
<table border=1 cellspacing=0 cellpadding=0>
<tr><td>
<table cellspacing=0 cellpadding=0>
<tr>
<td rowspan=2 valign=top><a href="queryhelp.cgi#peopleinvolved">Email:</a>
<input name="email$id" size="30" value="$defstr">&nbsp;matching as
} . BuildPulldown("emailtype$id",
                  [["regexp", "regexp"],
                   ["notregexp", "not regexp"],
                   ["substring", "substring"],
                   ["exact", "exact"]],
                  $default{"emailtype$id"}) . qq{
</td>
<td>
<input type="checkbox" name="emailassigned_to$id" value=1 $assignedto>Assigned To
</td>
</tr>
<tr>
<td>
<input type="checkbox" name="emailreporter$id" value=1 $reporter>Reporter
</td>
</tr>$qapart
<tr>
<td align=right>(Will match any of the selected fields)</td>
<td>
<input type="checkbox" name="emailcc$id" value=1 $cc>CC
</td>
</tr>
<tr>
<td></td>
<td>
<input type="checkbox" name="emaillongdesc$id" value=1 $longdesc>Added comment
</td>
</tr>
</table>
</table>
};
}


            


my $emailinput1 = GenerateEmailInput(1);
my $emailinput2 = GenerateEmailInput(2);

# if using usebuggroups, then we don't want people to see products they don't
# have access to.  remove them from the list.

@::product_list = ();
my %component_set;
my %version_set;
my %milestone_set;
foreach my $p (@::legal_product) {
  if(Param("usebuggroups")
     && GroupExists($p)
     && !UserInGroup($p)) {
    # If we're using bug groups to restrict entry on products, and
    # this product has a bug group, and the user is not in that
    # group, we don't want to include that product in this list.
    next;
  }
  push @::product_list, $p;
  if ($::components{$p}) {
    foreach my $c (@{$::components{$p}}) {
      $component_set{$c} = 1;
    }
  }
  foreach my $v (@{$::versions{$p}}) {
    $version_set{$v} = 1;
  }
  foreach my $m (@{$::target_milestone{$p}}) {
    $milestone_set{$m} = 1;
  }
}

@::component_list = ();
@::version_list = ();
@::milestone_list = ();
foreach my $c (@::legal_components) {
  if ($component_set{$c}) {
    push @::component_list, $c;
  }
}
foreach my $v (@::legal_versions) {
  if ($version_set{$v}) {
    push @::version_list, $v;
  }
}
foreach my $m (@::legal_target_milestone) {
  if ($milestone_set{$m}) {
    push @::milestone_list, $m;
  }
}

# SELECT box javascript handling. This is done to make the component,
# versions and milestone SELECTs repaint automatically when a product is
# selected. Refactored for bug 96534.
    
# make_js_array: iterates through the product array creating a
# javascript array keyed by product with an alphabetically ordered array
# for the corresponding elements in the components array passed in.
# return a string with javascript definitions for the product in a nice
# arrays which can be linearly appended later on.

# make_js_array ( \@products, \%[components/versions/milestones], $array )

sub make_js_array {
    my @prods = @{$_[0]};
    my %data = %{$_[1]};
    my $arr = $_[2];

    my $ret = "\nvar $arr = new Array();\n";
    foreach my $p ( @prods ) {
        # join each element with a "," case-insensitively alpha sorted
        if ( $data{$p} ) {
            $ret .= $arr."[".SqlQuote($p)."] = [";
            # the SqlQuote() protects our 's.
            my @tmp = map( SqlQuote( $_ ), @{ $data{$p} } );
            # do the join on a sorted, quoted list
            @tmp = sort { lc( $a ) cmp lc( $b ) } @tmp;
            $ret .= join( ", ", @tmp );
            $ret .= "];\n";
        }
    }
    return $ret;
}

my $jscript = '<script language="JavaScript" type="text/javascript">';
$jscript .= "\n<!--\n\n";

# Add the javascript code for the arrays of components and versions
# This is used in our javascript functions

$jscript .= "var usetms = 0; // do we have target milestone?\n";
$jscript .= "var first_load = 1; // is this the first time we load the page?\n";
$jscript .= "var last_sel = []; // caches last selection\n";
$jscript .= make_js_array( \@::product_list, \%::components, "cpts" );
$jscript .= make_js_array( \@::product_list, \%::versions, "vers" );

if ( Param( "usetargetmilestone" ) ) {
    $jscript .= make_js_array(\@::product_list, \%::target_milestone, "tms");
    $jscript .= "\nusetms = 1; // hooray, we use target milestones\n";
}

$jscript .= << 'ENDSCRIPT';

// Adds to the target select object all elements in array that
// correspond to the elements selected in source.
//     - array should be a array of arrays, indexed by product name. the
//       array should contain the elements that correspont to that
//       product. Example:
//         var array = Array();
//         array['ProductOne'] = [ 'ComponentA', 'ComponentB' ];
//         updateSelect(array, source, target);
//     - sel is a list of selected items, either whole or a diff
//       depending on sel_is_diff.
//     - sel_is_diff determines if we are sending in just a diff or the
//       whole selection. a diff is used to optimize adding selections.
//     - target should be the target select object.
//     - single specifies if we selected a single item. if we did, no
//       need to merge.

function updateSelect( array, sel, target, sel_is_diff, single ) {
        
    var i, comp;

    // if single, even if it's a diff (happens when you have nothing
    // selected and select one item alone), skip this.
    if ( ! single ) {

        // array merging/sorting in the case of multiple selections
        if ( sel_is_diff ) {
        
            // merge in the current options with the first selection
            comp = merge_arrays( array[sel[0]], target.options, 1 );

            // merge the rest of the selection with the results
            for ( i = 1 ; i < sel.length ; i++ ) {
                comp = merge_arrays( array[sel[i]], comp, 0 );
            }
        } else {
            // here we micro-optimize for two arrays to avoid merging with a
            // null array 
            comp = merge_arrays( array[sel[0]],array[sel[1]], 0 );

            // merge the arrays. not very good for multiple selections.
            for ( i = 2; i < sel.length; i++ ) {
                comp = merge_arrays( comp, array[sel[i]], 0 );
            }
        }
    } else {
        // single item in selection, just get me the list
        comp = array[sel[0]];
    }

    // clear select
    target.options.length = 0;

    // load elements of list into select
    for ( i = 0; i < comp.length; i++ ) {
        target.options[i] = new Option( comp[i], comp[i] );
    }
}

// Returns elements in a that are not in b.
// NOT A REAL DIFF: does not check the reverse.
//     - a,b: arrays of values to be compare.

function fake_diff_array( a, b ) {
    var newsel = new Array();

    // do a boring array diff to see who's new
    for ( var ia in a ) {
        var found = 0;
        for ( var ib in b ) {
            if ( a[ia] == b[ib] ) {
                found = 1;
            }
        }
        if ( ! found ) {
            newsel[newsel.length] = a[ia];
        }
        found = 0;
    }
    return newsel;
}

// takes two arrays and sorts them by string, returning a new, sorted
// array. the merge removes dupes, too.
//     - a, b: arrays to be merge.
//     - b_is_select: if true, then b is actually an optionitem and as
//       such we need to use item.value on it.

function merge_arrays( a, b, b_is_select ) {
    var pos_a = 0;
    var pos_b = 0;
    var ret = new Array();
    var bitem, aitem;

    // iterate through both arrays and add the larger item to the return
    // list. remove dupes, too. Use toLowerCase to provide
    // case-insensitivity.

    while ( ( pos_a < a.length ) && ( pos_b < b.length ) ) {

        if ( b_is_select ) {
            bitem = b[pos_b].value;
        } else {
            bitem = b[pos_b];
        }
        aitem = a[pos_a];

        // smaller item in list a
        if ( aitem.toLowerCase() < bitem.toLowerCase() ) {
            ret[ret.length] = aitem;
            pos_a++;
        } else {
            // smaller item in list b
            if ( aitem.toLowerCase() > bitem.toLowerCase() ) {
                ret[ret.length] = bitem;
                pos_b++;
            } else {
                // list contents are equal, inc both counters. 
                ret[ret.length] = aitem;
                pos_a++;
                pos_b++;
            }
        }
    }

    // catch leftovers here. these sections are ugly code-copying.
    if ( pos_a < a.length ) {
        for ( ; pos_a < a.length ; pos_a++ ) {
            ret[ret.length] = a[pos_a];
        }
    }

    if ( pos_b < b.length ) {
        for ( ; pos_b < b.length; pos_b++ ) {
            if ( b_is_select ) {
                bitem = b[pos_b].value;
            } else {
                bitem = b[pos_b];
            }
            ret[ret.length] = bitem;
        }
    }
    return ret;
}

// selectProduct reads the selection from f.product and updates
// f.version, component and target_milestone accordingly.
//     - f: a form containing product, component, varsion and
//       target_milestone select boxes. 
// globals (3vil!):
//     - cpts, vers, tms: array of arrays, indexed by product name. the
//       subarrays contain a list of names to be fed to the respective
//       selectboxes. For bugzilla, these are generated with perl code
//       at page start.
//     - usetms: this is a global boolean that is defined if the
//       bugzilla installation has it turned on. generated in perl too.
//     - first_load: boolean, specifying if it's the first time we load
//       the query page.
//     - last_sel: saves our last selection list so we know what has
//       changed, and optimize for additions.

function selectProduct( f ) {

    // this is to avoid handling events that occur before the form
    // itself is ready, which happens in buggy browsers.

    if ( ( !f ) || ( ! f.product ) ) {
        return;
    }

    // if this is the first load and nothing is selected, no need to
    // merge and sort all components; perl gives it to us sorted.

    if ( ( first_load ) && ( f.product.selectedIndex == -1 ) ) {
            first_load = 0;
            return;
    }
    
    // turn first_load off. this is tricky, since it seems to be
    // redundant with the above clause. It's not: if when we first load
    // the page there is _one_ element selected, it won't fall into that
    // clause, and first_load will remain 1. Then, if we unselect that
    // item, selectProduct will be called but the clause will be valid
    // (since selectedIndex == -1), and we will return - incorrectly -
    // without merge/sorting.

    first_load = 0;

    // - sel keeps the array of products we are selected. 
    // - is_diff says if it's a full list or just a list of products that
    //   were added to the current selection. 
    // - single indicates if a single item was selected
    var sel = Array();
    var is_diff = 0;
    var single;

    // if nothing selected, pick all
    if ( f.product.selectedIndex == -1 ) {
        for ( var i = 0 ; i < f.product.length ; i++ ) {
            sel[sel.length] = f.product.options[i].value;
        }
        single = 0;
    } else {

        for ( i = 0 ; i < f.product.length ; i++ ) {
            if ( f.product.options[i].selected ) {
                sel[sel.length] = f.product.options[i].value;
            }
        }

        single = ( sel.length == 1 );

        // save last_sel before we kill it
        var tmp = last_sel;
        last_sel = sel;
    
        // this is an optimization: if we've added components, no need
        // to remerge them; just merge the new ones with the existing
        // options.

        if ( ( tmp ) && ( tmp.length < sel.length ) ) {
            sel = fake_diff_array(sel, tmp);
            is_diff = 1;
        }
    }

    // do the actual fill/update
    updateSelect( cpts, sel, f.component, is_diff, single );
    updateSelect( vers, sel, f.version, is_diff, single );
    if ( usetms ) {
        updateSelect( tms, sel, f.target_milestone, is_diff, single );
    }
}

// -->
</script>
ENDSCRIPT

#
# End the fearsome Javascript section. 
#

# Muck the "legal product" list so that the default one is always first (and
# is therefore visibly selected.

# Commented out, until we actually have enough products for this to matter.

# set w [lsearch $legal_product $default{"product"}]
# if {$w >= 0} {
#    set legal_product [concat $default{"product"} [lreplace $legal_product $w $w]]
# }

PutHeader("Bugzilla Query Page", "Query", 
          "This page lets you search the database for recorded bugs.",
          q{onLoad="selectProduct(document.forms['queryform']);"}, $jscript);

push @::legal_resolution, "---"; # Oy, what a hack.

my @logfields = ("[Bug creation]", @::log_columns);

print"<P>Give me a <A HREF=\"queryhelp.cgi\">clue</A> about how to use this form.<P>";

print qq{
<FORM METHOD=GET ACTION="buglist.cgi" NAME="queryform">

<table>
<tr>
<th align=left><A HREF="queryhelp.cgi#status">Status</a>:</th>
<th align=left><A HREF="queryhelp.cgi#resolution">Resolution</a>:</th>
<th align=left><A HREF="queryhelp.cgi#platform">Platform</a>:</th>
<th align=left><A HREF="queryhelp.cgi#opsys">OpSys</a>:</th>
<th align=left><A HREF="queryhelp.cgi#priority">Priority</a>:</th>
<th align=left><A HREF="queryhelp.cgi#severity">Severity</a>:</th>
};

print "
</tr>
<tr>
<td align=left valign=top>

@{[make_selection_widget(\"bug_status\",\@::legal_bug_status,$default{'bug_status'}, $type{'bug_status'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"resolution\",\@::legal_resolution,$default{'resolution'}, $type{'resolution'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"rep_platform\",\@::legal_platform,$default{'platform'}, $type{'platform'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"op_sys\",\@::legal_opsys,$default{'op_sys'}, $type{'op_sys'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"priority\",\@::legal_priority,$default{'priority'}, $type{'priority'}, 1)]}

</td>
<td align=left valign=top>
@{[make_selection_widget(\"bug_severity\",\@::legal_severity,$default{'bug_severity'}, $type{'bug_severity'}, 1)]}

</tr>
</table>

<p>

<table>
<tr><td colspan=2>
$emailinput1<p>
</td></tr><tr><td colspan=2>
$emailinput2<p>
</td></tr>";

my $inclselected = "SELECTED";
my $exclselected = "";

    
if ($default{'bugidtype'} eq "exclude") {
    $inclselected = "";
    $exclselected = "SELECTED";
}
my $bug_id = value_quote($default{'bug_id'}); 

print qq{
<TR>
<TD COLSPAN="3">
<SELECT NAME="bugidtype">
<OPTION VALUE="include" $inclselected>Only
<OPTION VALUE="exclude" $exclselected>Exclude
</SELECT>
bugs numbered: 
<INPUT TYPE="text" NAME="bug_id" VALUE="$bug_id" SIZE=30>
</TD>
</TR>
};

print "
<tr>
<td>
Changed in the <NOBR>last <INPUT NAME=changedin SIZE=2 VALUE=\"$default{'changedin'}\"> days.</NOBR>
</td>
<td align=right>
At <NOBR>least <INPUT NAME=votes SIZE=3 VALUE=\"$default{'votes'}\"> votes.</NOBR>
</tr>
</table>


<table>
<tr>
<td rowspan=2 align=right>Where the field(s)
</td><td rowspan=2>
<SELECT NAME=\"chfield\" MULTIPLE SIZE=4>
@{[make_options(\@logfields, $default{'chfield'}, $type{'chfield'})]}
</SELECT>
</td><td rowspan=2>
changed.
</td><td>
<nobr>dates <INPUT NAME=chfieldfrom SIZE=10 VALUE=\"$default{'chfieldfrom'}\"></nobr>
<nobr>to <INPUT NAME=chfieldto SIZE=10 VALUE=\"$default{'chfieldto'}\"></nobr>
</td>
</tr>
<tr>
<td>changed to value <nobr><INPUT NAME=chfieldvalue SIZE=10> (optional)</nobr>
</td>
</table>


<P>

<table>
<tr>
<TH ALIGN=LEFT VALIGN=BOTTOM>Program:</th>
<TH ALIGN=LEFT VALIGN=BOTTOM>Version:</th>
<TH ALIGN=LEFT VALIGN=BOTTOM><A HREF=describecomponents.cgi>Component:</a></th>
";

if (Param("usetargetmilestone")) {
    print "<TH ALIGN=LEFT VALIGN=BOTTOM>Target Milestone:</th>";
}

print "
</tr>
<tr>

<td align=left valign=top>
<SELECT NAME=\"product\" MULTIPLE SIZE=5 onChange=\"selectProduct(this.form);\">
@{[make_options(\@::product_list, $default{'product'}, $type{'product'})]}
</SELECT>
</td>

<td align=left valign=top>
<SELECT NAME=\"version\" MULTIPLE SIZE=5>
@{[make_options(\@::version_list, $default{'version'}, $type{'version'})]}
</SELECT>
</td>

<td align=left valign=top>
<SELECT NAME=\"component\" MULTIPLE SIZE=5>
@{[make_options(\@::component_list, $default{'component'}, $type{'component'})]}
</SELECT>
</td>";

if (Param("usetargetmilestone")) {
    print "
<td align=left valign=top>
<SELECT NAME=\"target_milestone\" MULTIPLE SIZE=5>
@{[make_options(\@::milestone_list, $default{'target_milestone'}, $type{'target_milestone'})]}
</SELECT>
</td>";
}


sub StringSearch {
    my ($desc, $name) = (@_);
    my $type = $name . "_type";
    my $def = value_quote($default{$name});
    print qq{<tr>
<td align=right>$desc:</td>
<td><input name=$name size=30 value="$def"></td>
<td><SELECT NAME=$type>
};
    if ($default{$type} eq "") {
        $default{$type} = "allwordssubstr";
    }
    foreach my $i (["substring", "case-insensitive substring"],
                   ["casesubstring", "case-sensitive substring"],
                   ["allwordssubstr", "all words as substrings"],
                   ["anywordssubstr", "any words as substrings"],
                   ["allwords", "all words"],
                   ["anywords", "any words"],
                   ["regexp", "regular expression"],
                   ["notregexp", "not ( regular expression )"]) {
        my ($n, $d) = (@$i);
        my $sel = "";
        if ($default{$type} eq $n) {
            $sel = " SELECTED";
        }
        print qq{<OPTION VALUE="$n"$sel>$d\n};
    }
    print "</SELECT></TD>
</tr>
";
}

print "
</tr>
</table>

<table border=0>
";

StringSearch("Summary", "short_desc");
StringSearch("A description entry", "long_desc");
StringSearch("URL", "bug_file_loc");

if (Param("usestatuswhiteboard")) {
    StringSearch("Status whiteboard", "status_whiteboard");
}

if (@::legal_keywords) {
    my $def = value_quote($default{'keywords'});
    print qq{
<TR>
<TD ALIGN="right"><A HREF="describekeywords.cgi">Keywords</A>:</TD>
<TD><INPUT NAME="keywords" SIZE=30 VALUE="$def"></TD>
<TD>
};
    my $type = $default{"keywords_type"};
    if ($type eq "or") {        # Backward compatability hack.
        $type = "anywords";
    }
    print BuildPulldown("keywords_type",
                        [["anywords", "Any of the listed keywords set"],
                         ["allwords", "All of the listed keywords set"],
                         ["nowords", "None of the listed keywords set"]],
                        $type);
    print qq{</TD></TR>};
}

print "
</table>
<p>
";


my @fields;
push(@fields, ["noop", "---"]);
ConnectToDatabase();
SendSQL("SELECT name, description FROM fielddefs ORDER BY sortkey");
while (MoreSQLData()) {
    my ($name, $description) = (FetchSQLData());
    push(@fields, [$name, $description]);
}

my @types = (
	     ["noop", "---"],
	     ["equals", "equal to"],
	     ["notequals", "not equal to"],
	     ["casesubstring", "contains (case-sensitive) substring"],
	     ["substring", "contains (case-insensitive) substring"],
	     ["notsubstring", "does not contain (case-insensitive) substring"],
	     ["allwordssubstr", "all words as (case-insensitive) substrings"],
	     ["anywordssubstr", "any words as (case-insensitive) substrings"],
	     ["regexp", "contains regexp"],
	     ["notregexp", "does not contain regexp"],
	     ["lessthan", "less than"],
	     ["greaterthan", "greater than"],
	     ["anywords", "any words"],
	     ["allwords", "all words"],
	     ["nowords", "none of the words"],
	     ["changedbefore", "changed before"],
	     ["changedafter", "changed after"],
	     ["changedfrom", "changed from"],
	     ["changedto", "changed to"],
	     ["changedby", "changed by"],
	     );


print qq{<A NAME="chart"> </A>\n};

foreach my $cmd (grep(/^cmd-/, keys(%::FORM))) {
    if ($cmd =~ /^cmd-add(\d+)-(\d+)-(\d+)$/) {
	$::FORM{"field$1-$2-$3"} = "xyzzy";
    }
}
	
#  foreach my $i (sort(keys(%::FORM))) {
#      print "$i : " . value_quote($::FORM{$i}) . "<BR>\n";
#  }


if (!exists $::FORM{'field0-0-0'}) {
    $::FORM{'field0-0-0'} = "xyzzy";
}

my $jsmagic = qq{ONCLICK="document.forms[0].action='query.cgi#chart' ; document.forms[0].method='POST' ; return 1;"};

my $chart;
for ($chart=0 ; exists $::FORM{"field$chart-0-0"} ; $chart++) {
    my @rows;
    my $row;
    for ($row = 0 ; exists $::FORM{"field$chart-$row-0"} ; $row++) {
	my @cols;
	my $col;
	for ($col = 0 ; exists $::FORM{"field$chart-$row-$col"} ; $col++) {
	    my $key = "$chart-$row-$col";
	    my $deffield = $::FORM{"field$key"} || "";
	    my $deftype = $::FORM{"type$key"} || "";
	    my $defvalue = value_quote($::FORM{"value$key"} || "");
	    my $line = "";
	    $line .= "<TD>";
	    $line .= BuildPulldown("field$key", \@fields, $deffield);
	    $line .= BuildPulldown("type$key", \@types, $deftype);
	    $line .= qq{<INPUT NAME="value$key" VALUE="$defvalue">};
	    $line .= "</TD>\n";
	    push(@cols, $line);
	}
	push(@rows, "<TR>" . join(qq{<TD ALIGN="center"> or </TD>\n}, @cols) .
	     qq{<TD><INPUT TYPE="submit" VALUE="Or" NAME="cmd-add$chart-$row-$col" $jsmagic></TD></TR>});
    }
    print qq{
<HR>
<TABLE>
};
    print join('<TR><TD>And</TD></TR>', @rows);
    print qq{
<TR><TD><INPUT TYPE="submit" VALUE="And" NAME="cmd-add$chart-$row-0" $jsmagic>
};
    my $n = $chart + 1;
    if (!exists $::FORM{"field$n-0-0"}) {
        print qq{
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<INPUT TYPE="submit" VALUE="Add another boolean chart" NAME="cmd-add$n-0-0" $jsmagic>
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<NOBR><A HREF="queryhelp.cgi#advancedquerying">What is this stuff?</A></NOBR>
};
    }
    print qq{
</TD>
</TR>
</TABLE>
    };
}
print qq{<HR>};




if (!$userid) {
    print qq{<INPUT TYPE="hidden" NAME="cmdtype" VALUE="doit">};
} else {
    print "
<BR>
<INPUT TYPE=radio NAME=cmdtype VALUE=doit CHECKED> Run this query
<BR>
";

    my @namedqueries;
    if ($userid) {
        SendSQL("SELECT name FROM namedqueries " .
                "WHERE userid = $userid AND name != '$::defaultqueryname' " .
                "ORDER BY name");
        while (MoreSQLData()) {
            push(@namedqueries, FetchOneColumn());
        }
    }
    
    
    
    
    if (@namedqueries) {
        my $namelist = make_options(\@namedqueries);
        print qq{
<table cellspacing=0 cellpadding=0><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=editnamed> Load the remembered query:</td>
<td rowspan=3><select name=namedcmd>$namelist</select>
</tr><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=runnamed> Run the remembered query:</td>
</tr><tr>
<td><INPUT TYPE=radio NAME=cmdtype VALUE=forgetnamed> Forget the remembered query:</td>
</tr></table>};
    }

    print qq{
<INPUT TYPE=radio NAME=cmdtype VALUE=asdefault> Remember this as the default query
<BR>
<INPUT TYPE=radio NAME=cmdtype VALUE=asnamed> Remember this query, and name it:
<INPUT TYPE=text NAME=newqueryname>
<br>&nbsp;&nbsp;&nbsp;&nbsp;<INPUT TYPE="checkbox" NAME="tofooter" VALUE="1">
    and put it in my page footer.
<BR>
    };
}

print qq{
<NOBR><B>Sort By:</B>
<SELECT NAME=\"order\">
};

my $deforder = "'Importance'";
my @orders = ('Bug Number', $deforder, 'Assignee', 'Last Changed');

if ($::COOKIE{'LASTORDER'}) {
    $deforder = "Reuse same sort as last time";
    unshift(@orders, $deforder);
}

if ($::FORM{'order'}) { $deforder = $::FORM{'order'} }

my $defquerytype = $userdefaultquery ? "my" : "the";

print make_options(\@orders, $deforder);
print "</SELECT></NOBR>
<INPUT TYPE=\"submit\" VALUE=\"Submit query\">
<INPUT TYPE=\"reset\" VALUE=\"Reset back to $defquerytype default query\">
";

if ($userdefaultquery) {
    print qq{<BR><A HREF="query.cgi?nukedefaultquery=1">Set my default query back to the system default</A>};
}

print "
</FORM>
";

###
### I really hate this redudancy, but if somebody for some inexplicable reason doesn't like using
### the footer for these links, they can uncomment this section. 
###

# if (UserInGroup("tweakparams")) {
#     print "<a href=editparams.cgi>Edit Bugzilla operating parameters</a><br>\n";
# }
# if (UserInGroup("editcomponents")) {
#     print "<a href=editproducts.cgi>Edit Bugzilla products and components</a><br>\n";
# }
# if (UserInGroup("editkeywords")) {
#     print "<a href=editkeywords.cgi>Edit Bugzilla keywords</a><br>\n";
# }
# if ($userid) {
#     print "<a href=relogin.cgi>Log in as someone besides <b>$::COOKIE{'Bugzilla_login'}</b></a><br>\n";
# }
# print "<a href=userprefs.cgi>Change your password or preferences.</a><br>\n";
# print "<a href=\"enter_bug.cgi\">Report a new bug.</a><br>\n";
# print "<a href=\"createaccount.cgi\">Open a new Bugzilla account</a><br>\n";
# print "<a href=\"reports.cgi\">Bug reports</a><br>\n";


PutFooter();
