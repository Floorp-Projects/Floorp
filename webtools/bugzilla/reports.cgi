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
# Contributor(s): Harrison Page <harrison@netscape.com>,
# Terry Weissman <terry@mozilla.org>,
# Dawn Endico <endico@mozilla.org>
# Bryce Nesbitt <bryce@nextbus.COM>,
#    Added -All- report, change "nobanner" to "banner" (it is strange to have a
#    list with 2 positive and 1 negative choice), default links on, add show
#    sql comment.

use diagnostics;
use strict;
use Chart::Lines;

require "CGI.pl";
require "globals.pl";

use vars @::legal_product;

my $dir = "data/mining";
my $week = 60 * 60 * 24 * 7;
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

print "Content-type: text/html\n\n";

# If we're here for the first time, give a banner.  Else respect the banner flag.
if ( (!defined $::FORM{'product'}) || ($::FORM{'banner'})  )
        {
        PutHeader ("Bug Reports")
        }
else
        {
	print("<html><head><title>Bug Reports</title></head><body bgcolor=\"#FFFFFF\">");
        }

ConnectToDatabase();
GetVersionTable();

my @myproducts;
push( @myproducts, "-All-", @::legal_product );

$::FORM{'output'} = $::FORM{'output'} || "most_doomed"; # a reasonable default

if (! defined $::FORM{'product'})
	{
	&choose_product;
	}
else
	{
	# we want to be careful about what subroutines 
	# can be called from outside. modify %reports
	# accordingly when a new report type is added

	if (! exists $reports{$::FORM{'output'}})
		{
		$::FORM{'output'} = "most_doomed"; # a reasonable default
		}
	
	my $f = $reports{$::FORM{'output'}};

	if (! defined $f)
		{
		print "start over, your form data was all messed up.<p>\n";
		foreach (keys %::FORM)
			{
			print "<font color=blue>$_</font> : " . 
				($::FORM{$_} ? $::FORM{$_} : "undef") . "<br>\n";
			}
		exit;
		}

	&{$f};
	}

print <<FIN;
<p>
</body>
</html>
FIN

##################################
# user came in with no form data #
##################################

sub choose_product
	{
	my $product_popup = make_options (\@myproducts, $myproducts[0]);
	my $charts = (-d $dir) ? "<option value=\"show_chart\">Bug Charts" : "";

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
<tr>
<td align=center><b>Switches:</b></td>
<td align=left>
<input type=checkbox name=links checked value=1>&nbsp;Links to Bugs<br>
<input type=checkbox name=banner checked value=1>&nbsp;Banner<br>
<input type=checkbox name=quip value=1>&nbsp;Quip<br>
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

sub most_doomed
	{
	my $when = localtime (time);

	print <<FIN;
<center>
<h1>
Bug Report for $::FORM{'product'}
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

	if( $::FORM{'product'} ne "-All-" ) {
		$query .= "and    bugs.product='$::FORM{'product'}'";
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
		unless (! exists $::FORM{'showsql'});

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

	while (my ($bid, $a, $sev, $st, $prod, $who, $rep, $ts) = FetchSQLData())
		{
		next if (exists $bugs_lookup{$bid});
		
		$bugs_lookup{$bid} ++;
		$bugs_owners{$who} ++;
		$bugs_new_this_week ++ if (time - $ts <= $week);
		$bugs_status{$st} ++;
		$bugs_count ++;
		
		push @{$bugs_summary{$who}{$st}}, $bid;
		
		$bugs_totals{$who}{$st} ++;
		}

	if ($::FORM{'quip'})
		{
		if (open (COMMENTS, "<data/comments")) 
			{
    	my @cdata;
			while (<COMMENTS>) 
				{
				push @cdata, $_;
				}
			close COMMENTS;
			$quip = "<i>" . $cdata[int(rand($#cdata + 1))] . "</i>";
			}
		} 

	#########################
	# start painting report #
	#########################

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

	if ($bugs_count == 0)
		{
		print "No bugs found!\n";
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

	foreach my $who (sort keys %bugs_summary)
		{
		my $bugz = 0;
	 	print <<FIN;
<tr>
<td align=left><tt>$who</tt></td>
FIN
		
		foreach my $st (@status)
			{
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

	foreach my $who (sort keys %bugs_summary)
		{
		print <<FIN;
<tr>
<td align=left><tt>$who</tt></td>
FIN

		foreach my $st (@status)
			{
			my @l;

			foreach (sort { $a <=> $b } @{$bugs_summary{$who}{$st}})
				{
				if ($::FORM{'links'})
					{
					push @l, "<a href=\"show_bug.cgi?id=$_\">$_</a>\n"; 
					}
				else
					{
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

sub is_legal_product
	{
	my $product = shift;
	return grep { $_ eq $product} @myproducts;
	}

sub show_chart
	{
  my $when = localtime (time);

	if (! is_legal_product ($::FORM{'product'}))
		{
		&die_politely ("Unknown product: $::FORM{'product'}");
		}

  print <<FIN;
<center>
FIN
	
	my @dates;
	my @open; my @assigned; my @reopened;

        my $prodname = $::FORM{'product'};

        $prodname =~ s/\//-/gs;

        my $file = join '/', $dir, $prodname;
	my $image = "$file.gif";

	if (! open FILE, $file)
		{
		&die_politely ("The tool which gathers bug counts has not been run yet.");
		}
	
	while (<FILE>)
		{
		chomp;
		next if ($_ =~ /^#/ or ! $_);
		my ($date, $open, $assigned, $reopened) = split /\|/, $_;
		my ($yy, $mm, $dd) = $date =~ /^\d{2}(\d{2})(\d{2})(\d{2})$/;

		push @dates, "$mm/$dd/$yy";
		push @open, $open;
		push @assigned, $assigned;
		push @reopened, $reopened;
		}
	
	close FILE;

	if ($#dates < 1)
		{
		&die_politely ("We don't have enough data points to make a graph (yet)");
		}
	
	my $img = Chart::Lines->new (800, 600);
	my @labels = qw (New Assigned Reopened);
	my @when;
	my $i = 0;
	my @data;

	push @data, \@dates;
	push @data, \@open;
	push @data, \@assigned;
	push @data, \@reopened;

    my $MAXTICKS = 20;      # Try not to show any more x ticks than this.
    my $skip = 1;
    if (@dates > $MAXTICKS) {
        $skip = int((@dates + $MAXTICKS - 1) / $MAXTICKS);
    }

	my %settings =
		(
		"title" => "Bug Charts for $::FORM{'product'}",
		"x_label" => "Dates",
		"y_label" => "Bug Count",
		"legend_labels" => \@labels,
        "skip_x_ticks" => $skip,
		);
	
	$img->set (%settings);
	
	open IMAGE, ">$image" or die "$image: $!";
	$img->gif (*IMAGE, \@data);
	close IMAGE;

	print <<FIN;
<img src="$image">
<br clear=left>
<br>
FIN
	}

sub die_politely
	{
	my $msg = shift;

	print <<FIN;
<p>
<table border=1 cellpadding=10>
<tr>
<td align=center>
<font color=blue>Sorry, but ...</font>
<p>
There is no graph available for <b>$::FORM{'product'}</b><p>

<font size=-1>
$msg
<p>
</font>
</td>
</tr>
</table>
<p>
FIN
	
	exit;
	}

sub bybugs {
   $bugsperperson{$a} <=> $bugsperperson{$b}
   }

sub most_doomed_for_milestone
	{
	my $when = localtime (time);
        my $ms = "M" . Param("curmilestone");
        my $quip = "Summary";

	print "<center>\n<h1>";
        if( $::FORM{'product'} ne "-All-" ) {
            print "Most Doomed for $ms ($::FORM{'product'})";
        } else {
            print "Most Doomed for $ms";
            }
        print "</h1>\n$when<p>\n";

	#########################
	# start painting report #
	#########################

	if ($::FORM{'quip'})
                {
                if (open (COMMENTS, "<data/comments"))
                        {
                        my @cdata;
                        while (<COMMENTS>)
                                {
                                push @cdata, $_;
                                }
                        close COMMENTS;
                        $quip = "<i>" . $cdata[int(rand($#cdata + 1))] . "</i>";                        }
                }


        # Build up $query string
	my $query;
	$query = "select distinct assigned_to from bugs where target_milestone=\"$ms\"";
	if( $::FORM{'product'} ne "-All-" ) {
		$query .= "and    bugs.product='$::FORM{'product'}'";
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
        while (my ($person) = FetchSQLData())
            {
            push @people, $person;
            }

        #############################
        # suck contents of database # 
        #############################
        my $person = "";
        my $bugtotal = 0;
        foreach $person (@people)
                {
                my $query = "select count(bug_id) from bugs,profiles where target_milestone=\"$ms\" and userid=assigned_to and userid=\"$person\"";
	        if( $::FORM{'product'} ne "-All-" ) {
                    $query .= "and    bugs.product='$::FORM{'product'}'";
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

#       sort people by the number of bugs they have assigned to this milestone
        @people = sort bybugs @people;
        my $totalpeople = @people;
                
        print "<TABLE>\n";
        print "<TR><TD COLSPAN=2>\n";
        print "$totalpeople engineers have $bugtotal $ms bugs and features.\n";
        print "</TD></TR>\n";

        while (@people)
                {
                $person = pop @people;
                print "<TR><TD>\n";
                SendSQL("select login_name from profiles where userid=$person");
                my $login_name= FetchSQLData();
                print("<A HREF=\"buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&target_milestone=M9&assigned_to=$login_name\">\n");
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
                    print("<A HREF=\"buglist.cgi?bug_status=NEW&bug_status=ASSIGNED&bug_status=REOPENED&target_milestone=M9&assigned_to=$login_name\">\n");
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



sub most_recently_doomed
	{
	my $when = localtime (time);
        my $ms = "M" . Param("curmilestone");
        my $quip = "Summary";

	print "<center>\n<h1>";
        if( $::FORM{'product'} ne "-All-" ) {
            print "Most Recently Doomed ($::FORM{'product'})";
        } else {
            print "Most Recently Doomed";
            }
        print "</h1>\n$when<p>\n";

	#########################
	# start painting report #
	#########################

	if ($::FORM{'quip'})
                {
                if (open (COMMENTS, "<data/comments"))
                        {
                        my @cdata;
                        while (<COMMENTS>)
                                {
                                push @cdata, $_;
                                }
                        close COMMENTS;
                        $quip = "<i>" . $cdata[int(rand($#cdata + 1))] . "</i>";                        }
                }


        # Build up $query string
	my $query;
        $query = "select distinct assigned_to from bugs where bugs.bug_status='NEW' and target_milestone='' and bug_severity!='enhancement' and status_whiteboard='' and (product='Browser' or product='MailNews')";
	if( $::FORM{'product'} ne "-All-" ) {
		$query .= "and    bugs.product='$::FORM{'product'}'";
	}

# End build up $query string

        SendSQL ($query);
        my @people = ();
        while (my ($person) = FetchSQLData())
            {
            push @people, $person;
            }

        #############################
        # suck contents of database # 
        #############################
        my $person = "";
        my $bugtotal = 0;
        foreach $person (@people)
                {
                my $query = "select count(bug_id) from bugs,profiles where bugs.bug_status='NEW' and userid=assigned_to and userid='$person' and target_milestone='' and bug_severity!='enhancement' and status_whiteboard='' and (product='Browser' or product='MailNews')";
	        if( $::FORM{'product'} ne "-All-" ) {
                    $query .= "and    bugs.product='$::FORM{'product'}'";
                    }
                SendSQL ($query);
	        my $bugcount = FetchSQLData();
                $bugsperperson{$person} = $bugcount;
                $bugtotal += $bugcount;
                }

#       sort people by the number of bugs they have assigned to this milestone
        @people = sort bybugs @people;
        my $totalpeople = @people;
                
        print "<TABLE>\n";
        print "<TR><TD COLSPAN=2>\n";
        print "$totalpeople engineers have $bugtotal untouched new bugs.\n";
        print "</TD></TR>\n";

        while (@people)
                {
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
