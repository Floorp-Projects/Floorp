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
# Contributor(s): Harrison Page <harrison@netscape.com>,
# Terry Weissman <terry@mozilla.org>,
# Dawn Endico <endico@mozilla.org>
# Bryce Nesbitt <bryce@nextbus.COM>,
#    Added -All- report, change "nobanner" to "banner" (it is strange to have a
#    list with 2 positive and 1 negative choice), default links on, add show
#    sql comment.
# Joe Robins <jmrobins@tgix.com>,
#    If using the usebuggroups parameter, users shouldn't be able to see
#    reports for products they don't have access to.
# Gervase Markham <gerv@gerv.net> and Adam Spiers <adam@spiers.net>
#    Added ability to chart any combination of resolutions/statuses.
#    Derive the choice of resolutions/statuses from the -All- data file
#    Removed hardcoded order of resolutions/statuses when reading from
#    daily stats file, so now works independently of collectstats.pl
#    version
#    Added image caching by date and datasets
# Myk Melez <myk@mozilla.org):
#    Implemented form field validation and reorganized code.

use diagnostics;
use strict;

eval "use GD";
my $use_gd = $@ ? 0 : 1;
eval "use Chart::Lines";
$use_gd = 0 if $@;

require "CGI.pl";
use vars qw(%FORM); # globals from CGI.pl

require "globals.pl";
use vars qw(@legal_product); # globals from er, globals.pl

my $dir = "data/mining";
my $graph_dir = "graphs";

my @status = qw (NEW ASSIGNED REOPENED);
my %bugsperperson;

# while this looks odd/redundant, it allows us to name
# functions differently than the value passed in
my %reports = 
    ( 
    "most_doomed" => \&most_doomed,
    "most_doomed_for_milestone" => \&most_doomed_for_milestone,
    "most_recently_doomed" => \&most_recently_doomed,
    "show_chart" => \&show_chart,
    );

# If we're using bug groups for products, we should apply those restrictions
# to viewing reports, as well.  Time to check the login in that case.
ConnectToDatabase(1);
quietly_check_login();

GetVersionTable();

# If the usebuggroups parameter is set, we don't want to list all products.
# We only want those products that the user has permissions for.
my @myproducts;
if(Param("usebuggroups")) {
    push( @myproducts, "-All-");
    foreach my $this_product (@legal_product) {
        if(GroupExists($this_product) && !UserInGroup($this_product)) {
            next;
        } else {
            push( @myproducts, $this_product )
        }
    }
} else {
    push( @myproducts, "-All-", @legal_product );
}

if (! defined $FORM{'product'}) {

    print "Content-type: text/html\n\n";
    PutHeader("Bug Reports");
    &choose_product;
    PutFooter();

} else {

    # For security and correctness, validate the value of the "product" form variable.
    # Valid values are those products for which the user has permissions which appear
    # in the "product" drop-down menu on the report generation form.
    grep($_ eq $FORM{'product'}, @myproducts)
      || DisplayError("You entered an invalid product name.") && exit;

    # If usebuggroups is on, we don't want people to be able to view
    # reports for products they don't have permissions for...
    Param("usebuggroups") 
      && GroupExists($FORM{'product'}) 
      && !UserInGroup($FORM{'product'})
      && DisplayError("You do not have the permissions necessary to view reports for this product.")
      && exit;
          
    # For security and correctness, validate the value of the "output" form variable.
    # Valid values are the keys from the %reports hash defined above which appear in
    # the "output" drop-down menu on the report generation form.
    $FORM{'output'} ||= "most_doomed"; # a reasonable default
    grep($_ eq $FORM{'output'}, keys %reports)
      || DisplayError("You entered an invalid output type.") 
      && exit;

    # Output appropriate HTTP response headers
    print "Content-type: text/html\n";
    # Changing attachment to inline to resolve 46897 - zach@zachlipton.com
    print "Content-disposition: inline; filename=bugzilla_report.html\n\n";

    if ($FORM{'banner'}) {
      PutHeader("Bug Reports");
    } 
    else {
      print("<html><head><title>Bug Reports</title></head><body bgcolor=\"#FFFFFF\">");
    }

    # Execute the appropriate report generation function 
    # (the one whose name is the same as the value of the "output" form variable).
    &{$reports{$FORM{'output'}}};

    # ??? why is this necessary? formatting looks fine without it
    print "<p>";

    PutFooter() if $FORM{banner};

}



##################################
# user came in with no form data #
##################################

sub choose_product {
    my $product_popup = make_options (\@myproducts, $myproducts[0]);
  
    my $datafile = daily_stats_filename('-All-');

    # Can we do bug charts?  
         my $do_charts = ($use_gd && -d $dir && -d $graph_dir &&
                              open(DATA, "$dir/$datafile"));
 
     my $charts = $do_charts ? "<option value=\"show_chart\">Bug Charts" : "";
 
    print <<FIN;
<center>
<h1>Welcome to the Bugzilla Query Kitchen</h1>
<form method=get action=reports.cgi>
<table border=1 cellpadding=5>
<tr>
<td align=center><b>Product:</b></td>
<td align=center>
<select name="product">
$product_popup
</select>
</td>
</tr>
<tr>
<td align=center><b>Output:</b></td>
<td align=center>
<select name="output">
<option value="most_doomed">Bug Counts
FIN
    if (Param('usetargetmilestone')) {
        print "<option value=\"most_doomed_for_milestone\">Most Doomed";
    }
    print "<option value=\"most_recently_doomed\">Most Recently Doomed";
    print <<FIN;
$charts
</select>
</tr>
FIN

  if ($do_charts) {
      print <<FIN;
      <tr>
      <td align=center><b>Chart datasets:</b></td>
      <td align=center>
      <select name="datasets" multiple size=5>
FIN

      my @datasets = ();

      while (<DATA>) {
          if (/^# fields?: (.+)\s*$/) {
              @datasets = grep ! /date/i, (split /\|/, $1);
              last;
          }
      }

      close(DATA);

      my %default_sel = map { $_ => 1 }
                            qw/UNCONFIRMED NEW ASSIGNED REOPENED/;
      foreach my $dataset (@datasets) {
          my $sel = $default_sel{$dataset} ? ' selected' : '';
          print qq{<option value="$dataset:"$sel>$dataset</option>\n};
      }

      print <<FIN;
      </select>
      </td>
      </tr>
FIN
  }

print <<FIN;
<tr>
<td align=center><b>Switches:</b></td>
<td align=left>
<input type=checkbox name=links checked value=1>&nbsp;Links to Bugs<br>
<input type=checkbox name=banner checked value=1>&nbsp;Banner<br>
FIN
  
    if (Param('usequip')) {
        print "<input type=checkbox name=quip value=1>&nbsp;Quip<br>";
    } else {
        print "<input type=hidden name=quip value=0>";
    }
    print <<FIN;
</td>
</tr>
<tr>
<td colspan=2 align=center>
<input type=submit value=Continue>
</td>
</tr>
</table>
</form>
<p>
FIN
#Add this above to get a control for showing the SQL query:
#<input type=checkbox name=showsql value=1>&nbsp;Show SQL<br>
}

sub most_doomed {
    my $when = localtime (time);

    print <<FIN;
<center>
<h1>
Bug Report for $FORM{'product'}
</h1>
$when<p>
FIN

# Build up $query string
    my $query;
    $query = <<FIN;
select 
    bugs.bug_id, bugs.assigned_to, bugs.bug_severity,
    bugs.bug_status, bugs.product, 
    assign.login_name,
    report.login_name,
    unix_timestamp(date_format(bugs.creation_ts, '%Y-%m-%d %h:%m:%s'))

from   bugs,
       profiles assign,
       profiles report,
       versions projector
where  bugs.assigned_to = assign.userid
and    bugs.reporter = report.userid
FIN

    if ($FORM{'product'} ne "-All-" ) {
        $query .= "and    bugs.product=".SqlQuote($FORM{'product'});
    }

    $query .= <<FIN;
and      
    ( 
    bugs.bug_status = 'NEW' or 
    bugs.bug_status = 'ASSIGNED' or 
    bugs.bug_status = 'REOPENED'
    )
FIN
# End build up $query string

    print "<font color=purple><tt>$query</tt></font><p>\n" 
        unless (! exists $FORM{'showsql'});

    SendSQL ($query);
    
    my $c = 0;

    my $quip = "Summary";
    my $bugs_count = 0;
    my $bugs_new_this_week = 0;
    my $bugs_reopened = 0;
    my %bugs_owners;
    my %bugs_summary;
    my %bugs_status;
    my %bugs_totals;
    my %bugs_lookup;

    #############################
    # suck contents of database # 
    #############################

    my $week = 60 * 60 * 24 * 7;
    while (my ($bid, $a, $sev, $st, $prod, $who, $rep, $ts) = FetchSQLData()) {
        next if (exists $bugs_lookup{$bid});
        
        $bugs_lookup{$bid} ++;
        $bugs_owners{$who} ++;
        $bugs_new_this_week ++ if (time - $ts <= $week);
        $bugs_status{$st} ++;
        $bugs_count ++;
        
        push @{$bugs_summary{$who}{$st}}, $bid;
        
        $bugs_totals{$who}{$st} ++;
    }

    if ($FORM{'quip'}) {
        if (open (COMMENTS, "<data/comments")) {
            my @cdata;
            while (<COMMENTS>) {
                push @cdata, $_;
            }
            close COMMENTS;
            $quip = "<i>" . $cdata[int(rand($#cdata + 1))] . "</i>";
        }
    } 

    #########################
    # start painting report #
    #########################

    $bugs_status{'NEW'}      ||= '0';
    $bugs_status{'ASSIGNED'} ||= '0';
    $bugs_status{'REOPENED'} ||= '0';
    print <<FIN;
<h1>$quip</h1>
<table border=1 cellpadding=5>
<tr>
<td align=right><b>New Bugs This Week</b></td>
<td align=center>$bugs_new_this_week</td>
</tr>

<tr>
<td align=right><b>Bugs Marked New</b></td>
<td align=center>$bugs_status{'NEW'}</td>
</tr>

<tr>
<td align=right><b>Bugs Marked Assigned</b></td>
<td align=center>$bugs_status{'ASSIGNED'}</td>
</tr>

<tr>
<td align=right><b>Bugs Marked Reopened</b></td>
<td align=center>$bugs_status{'REOPENED'}</td>
</tr>

<tr>
<td align=right><b>Total Bugs</b></td>
<td align=center>$bugs_count</td>
</tr>

</table>
<p>
FIN

    if ($bugs_count == 0) {
        print "No bugs found!\n";
                PutFooter() if $FORM{banner};
        exit;
    }
    
    print <<FIN;
<h1>Bug Count by Engineer</h1>
<table border=3 cellpadding=5>
<tr>
<td align=center bgcolor="#DDDDDD"><b>Owner</b></td>
<td align=center bgcolor="#DDDDDD"><b>New</b></td>
<td align=center bgcolor="#DDDDDD"><b>Assigned</b></td>
<td align=center bgcolor="#DDDDDD"><b>Reopened</b></td>
<td align=center bgcolor="#DDDDDD"><b>Total</b></td>
</tr>
FIN

    foreach my $who (sort keys %bugs_summary) {
        my $bugz = 0;
        print <<FIN;
<tr>
<td align=left><tt>$who</tt></td>
FIN
        
        foreach my $st (@status) {
            $bugs_totals{$who}{$st} += 0;
            print <<FIN;
<td align=center>$bugs_totals{$who}{$st}
FIN
            $bugz += $#{$bugs_summary{$who}{$st}} + 1;
        }
        
        print <<FIN;
<td align=center>$bugz</td>
</tr>
FIN
    }
    
    print <<FIN;
</table>
<p>
FIN

    ###############################
    # individual bugs by engineer #
    ###############################

    print <<FIN;
<h1>Individual Bugs by Engineer</h1>
<table border=1 cellpadding=5>
<tr>
<td align=center bgcolor="#DDDDDD"><b>Owner</b></td>
<td align=center bgcolor="#DDDDDD"><b>New</b></td>
<td align=center bgcolor="#DDDDDD"><b>Assigned</b></td>
<td align=center bgcolor="#DDDDDD"><b>Reopened</b></td>
</tr>
FIN

    foreach my $who (sort keys %bugs_summary) {
        print <<FIN;
<tr>
<td align=left><tt>$who</tt></td>
FIN

        foreach my $st (@status) {
            my @l;

            foreach (sort { $a <=> $b } @{$bugs_summary{$who}{$st}}) {
                if ($FORM{'links'}) {
                    push @l, "<a href=\"show_bug.cgi?id=$_\">$_</a>\n"; 
                }
                else {
                    push @l, $_;
                }
            }
                
            my $bugz = join ' ', @l;
            $bugz = "&nbsp;" unless ($bugz);
            
            print <<FIN
<td align=left>$bugz</td>
FIN
        }

        print <<FIN;
</tr>
FIN
    }

    print <<FIN;
</table>
<p>
FIN
}

sub daily_stats_filename {
    my ($prodname) = @_;
    $prodname =~ s/\//-/gs;
    return $prodname;
}

sub show_chart {
    # if we don't have the graphic mouldes don't even try to go
    # here. Should probably return some decent error message.
    return unless $use_gd;

    if (! $FORM{datasets}) {
        die_politely("You didn't select any datasets to plot");
    }

  print <<FIN;
<center>
FIN

    my $type = chart_image_type();
    my $data_file = daily_stats_filename($FORM{product});
    my $image_file = chart_image_name($data_file, $type);
    my $url_image = "$graph_dir/" . url_quote($image_file);

    if (! -e "$graph_dir/$image_file") {
        generate_chart("$dir/$data_file", "$graph_dir/$image_file", $type);
    }
    
    print <<FIN;
<img src="$url_image">
<br clear=left>
<br>
FIN
}

sub chart_image_type {
    # what chart type should we be generating?
    my $testimg = Chart::Lines->new(2,2);
    my $type = $testimg->can('gif') ? "gif" : "png";

    undef $testimg;
    return $type;
}

sub chart_image_name {
    my ($data_file, $type) = @_;

    # Cache charts by generating a unique filename based on what they
    # show. Charts should be deleted by collectstats.pl nightly.
    my $id = join ("_", split (":", $FORM{datasets}));

    return "${data_file}_${id}.$type";
}

sub day_of_year {
    my ($mday, $month, $year) = (localtime())[3 .. 5];
    $month += 1;
    $year += 1900;
    my $date = sprintf "%02d%02d%04d", $mday, $month, $year;
}

sub generate_chart {
    my ($data_file, $image_file, $type) = @_;
    
    if (! open FILE, $data_file) {
        &die_politely ("The tool which gathers bug counts has not been run yet.");
    }

    my @fields;
    my @labels = qw(DATE);
    my %datasets = map { $_ => 1 } split /:/, $FORM{datasets};

    my %data = ();
    while (<FILE>) {
        chomp;
        next unless $_;
        if (/^#/) {
            if (/^# fields?: (.*)\s*$/) {
                @fields = split /\||\r/, $1;
                &die_politely("`# fields: ' line didn't start with DATE, but with $fields[0]")
                  unless $fields[0] =~ /date/i;
                push @labels, grep($datasets{$_}, @fields);
            }
            next;
        }

        &die_politely("`# fields: ' line was not found before start of data")
          unless @fields;
        
        my @line = split /\|/;
        my $date = $line[0];
        my ($yy, $mm, $dd) = $date =~ /^\d{2}(\d{2})(\d{2})(\d{2})$/;
        push @{$data{DATE}}, "$mm/$dd/$yy";
        
        for my $i (1 .. $#fields) {
            my $field = $fields[$i];
            if (! defined $line[$i] or $line[$i] eq '') {
                # no data point given, don't plot (this will probably
                # generate loads of Chart::Base warnings, but that's not
                # our fault.)
                push @{$data{$field}}, undef;
            }
            else {
                push @{$data{$field}}, $line[$i];
            }
        }
    }
    
    shift @labels;

    close FILE;

    if (! @{$data{DATE}}) {
        &die_politely ("We don't have enough data points to make a graph (yet)");
    }
    
    my $img = Chart::Lines->new (800, 600);
    my $i = 0;

    my $MAXTICKS = 20;      # Try not to show any more x ticks than this.
    my $skip = 1;
    if (@{$data{DATE}} > $MAXTICKS) {
        $skip = int((@{$data{DATE}} + $MAXTICKS - 1) / $MAXTICKS);
    }

    my %settings =
        (
         "title" => "Status Counts for $FORM{'product'}",
         "x_label" => "Dates",
         "y_label" => "Bug Counts",
         "legend_labels" => \@labels,
         "skip_x_ticks" => $skip,
         "y_grid_lines" => "true",
         "grey_background" => "false",
         "colors" => {
                      # default dataset colours are too alike
                      dataset4 => [0, 0, 0], # black
                     },
        );
    
    $img->set (%settings);
    $img->$type($image_file, [ @data{('DATE', @labels)} ]);
}

sub die_politely {
    my $msg = shift;

    print <<FIN;
<p>
<table border=1 cellpadding=10>
<tr>
<td align=center>
<font color=blue>Sorry, but ...</font>
<p>
There is no graph available for <b>$FORM{'product'}</b><p>

$msg
<p>
</td>
</tr>
</table>
<p>
FIN
    
    PutFooter() if $FORM{banner};
    exit;
}

sub bybugs {
   $bugsperperson{$a} <=> $bugsperperson{$b}
}

sub most_doomed_for_milestone {
    my $when = localtime (time);
    my $ms = "M" . Param("curmilestone");
    my $quip = "Summary";

    print "<center>\n<h1>";
    if( $FORM{'product'} ne "-All-" ) {
        SendSQL("SELECT defaultmilestone FROM products WHERE product = " .
                SqlQuote($FORM{'product'}));
        $ms = FetchOneColumn();
        print "Most Doomed for $ms ($FORM{'product'})";
    } else {
        print "Most Doomed for $ms";
    }
    print "</h1>\n$when<p>\n";

    #########################
    # start painting report #
    #########################

    if ($FORM{'quip'}) {
        if (open (COMMENTS, "<data/comments")) {
            my @cdata;
            while (<COMMENTS>) {
                push @cdata, $_;
            }
            close COMMENTS;
            $quip = "<i>" . $cdata[int(rand($#cdata + 1))] . "</i>";
        }
    }
    
    # Build up $query string
    my $query;
    $query = "select distinct assigned_to from bugs where target_milestone=\"$ms\"";
    if ($FORM{'product'} ne "-All-" ) {
        $query .= "and    bugs.product=".SqlQuote($FORM{'product'});
    }
    $query .= <<FIN;
and      
    ( 
    bugs.bug_status = 'NEW' or 
    bugs.bug_status = 'ASSIGNED' or 
    bugs.bug_status = 'REOPENED'
    )
FIN
# End build up $query string

    SendSQL ($query);
    my @people = ();
    while (my ($person) = FetchSQLData()) {
        push @people, $person;
    }

    #############################
    # suck contents of database # 
    #############################
    my $person = "";
    my $bugtotal = 0;
    foreach $person (@people) {
        my $query = "select count(bug_id) from bugs,profiles where target_milestone=\"$ms\" and userid=assigned_to and userid=\"$person\"";
        if( $FORM{'product'} ne "-All-" ) {
            $query .= "and    bugs.product=".SqlQuote($FORM{'product'});
        }
        $query .= <<FIN;
and      
    ( 
    bugs.bug_status = 'NEW' or 
    bugs.bug_status = 'ASSIGNED' or 
    bugs.bug_status = 'REOPENED'
    )
FIN
        SendSQL ($query);
        my $bugcount = FetchSQLData();
        $bugsperperson{$person} = $bugcount;
        $bugtotal += $bugcount;
    }

#   sort people by the number of bugs they have assigned to this milestone
    @people = sort bybugs @people;
    my $totalpeople = @people;
                
    print "<TABLE>\n";
    print "<TR><TD COLSPAN=2>\n";
    print "$totalpeople engineers have $bugtotal $ms bugs and features.\n";
    print "</TD></TR>\n";

    while (@people) {
        $person = pop @people;
        print "<TR><TD>\n";
        SendSQL("select login_name from profiles where userid=$person");
        my $login_name= FetchSQLData();
        print("<A HREF=\"buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&target_milestone=$ms&assigned_to=$login_name");
        if ($FORM{'product'} ne "-All-" ) {
            print "&product=" . url_quote($FORM{'product'});
        }
        print("\">\n");
        print("$bugsperperson{$person}  bugs and features");
        print("</A>");
        print(" for \n");
        print("<A HREF=\"mailto:$login_name\">");
        print("$login_name");
        print("</A>\n");
        print("</TD><TD>\n");

        $person = pop @people;
        if ($person) {
            SendSQL("select login_name from profiles where userid=$person");
            my $login_name= FetchSQLData();
            print("<A HREF=\"buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&target_milestone=$ms&assigned_to=$login_name\">\n");
            print("$bugsperperson{$person}  bugs and features");
            print("</A>");
            print(" for \n");
            print("<A HREF=\"mailto:$login_name\">");
            print("$login_name");
            print("</A>\n");
            print("</TD></TR>\n\n");
        }
    }
    print "</TABLE>\n";
}


sub most_recently_doomed {
    my $when = localtime (time);
    my $ms = "M" . Param("curmilestone");
    my $quip = "Summary";
                                                     
    print "<center>\n<h1>";
    if( $FORM{'product'} ne "-All-" ) {
        SendSQL("SELECT defaultmilestone FROM products WHERE product = " .
                SqlQuote($FORM{'product'}));
        $ms = FetchOneColumn();
        print "Most Recently Doomed ($FORM{'product'})";
    } else {
        print "Most Recently Doomed";
    }
    print "</h1>\n$when<p>\n";

    #########################
    # start painting report #
    #########################

    if ($FORM{'quip'}) {
        if (open (COMMENTS, "<data/comments")) {
            my @cdata;
            while (<COMMENTS>) {
                push @cdata, $_;
            }
            close COMMENTS;
            $quip = "<i>" . $cdata[int(rand($#cdata + 1))] . "</i>";
        }
    }


    # Build up $query string
    my $query = "select distinct assigned_to from bugs where bugs.bug_status='NEW' and target_milestone='' and bug_severity!='enhancement' and status_whiteboard='' and (product='Browser' or product='MailNews')";
    if ($FORM{'product'} ne "-All-" ) {
        $query .= "and    bugs.product=".SqlQuote($FORM{'product'});
    }

# End build up $query string

    SendSQL ($query);
    my @people = ();
    while (my ($person) = FetchSQLData()) {
        push @people, $person;
    }

    #############################
    # suck contents of database # 
    #############################
    my $person = "";
    my $bugtotal = 0;
    foreach $person (@people) {
        my $query = "select count(bug_id) from bugs,profiles where bugs.bug_status='NEW' and userid=assigned_to and userid='$person' and target_milestone='' and bug_severity!='enhancement' and status_whiteboard='' and (product='Browser' or product='MailNews')";
        if( $FORM{'product'} ne "-All-" ) {
            $query .= "and    bugs.product='$FORM{'product'}'";
        }
        SendSQL ($query);
        my $bugcount = FetchSQLData();
        $bugsperperson{$person} = $bugcount;
        $bugtotal += $bugcount;
    }

#   sort people by the number of bugs they have assigned to this milestone
    @people = sort bybugs @people;
    my $totalpeople = @people;
    
    if ($totalpeople > 20) {
        splice @people, 0, $totalpeople-20;
    }
                
    print "<TABLE>\n";
    print "<TR><TD COLSPAN=2>\n";
    print "$totalpeople engineers have $bugtotal untouched new bugs.\n";
    if ($totalpeople > 20) {
        print "These are the 20 most doomed.";
    }
    print "</TD></TR>\n";

    while (@people) {
        $person = pop @people;
        print "<TR><TD>\n";
        SendSQL("select login_name from profiles where userid=$person");
        my $login_name= FetchSQLData();
        print("<A HREF=\"buglist.cgi?bug_status=NEW&email1=$login_name&emailtype1=substring&emailassigned_to1=1&product=Browser&product=MailNews&target_milestone=---&status_whiteboard=.&status_whiteboard_type=notregexp&bug_severity=blocker&bug_severity=critical&bug_severity=major&bug_severity=normal&bug_severity=minor&bug_severity=trivial\">\n"); 
        print("$bugsperperson{$person}  bugs");
        print("</A>");
        print(" for \n");
        print("<A HREF=\"mailto:$login_name\">");
        print("$login_name");
        print("</A>\n");
        print("</TD><TD>\n");

        $person = pop @people;
        if ($person) {
            SendSQL("select login_name from profiles where userid=$person");
            my $login_name= FetchSQLData();
            print("<A HREF=\"buglist.cgi?bug_status=NEW&email1=$login_name&emailtype1=substring&emailassigned_to1=1&product=Browser&product=MailNews&target_milestone=---&status_whiteboard=.&status_whiteboard_type=notregexp&bug_severity=blocker&bug_severity=critical&bug_severity=major&bug_severity=normal&bug_severity=minor&bug_severity=trivial\">\n"); 
            print("$bugsperperson{$person}  bugs");
            print("</A>");
            print(" for \n");
            print("<A HREF=\"mailto:$login_name\">");
            print("$login_name");
            print("</A>\n");
            print("</TD></TR>\n\n");
        }
    }
    print "</TABLE>\n";

}
