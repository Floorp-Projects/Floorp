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
# Contributor(s): Terry Weissman <terry@mozilla.org>,
#                 Harrison Page <harrison@netscape.com>

# Run me out of cron at midnight to collect Bugzilla statistics.

use diagnostics;
use strict;
use vars @::legal_product;

require "globals.pl";

ConnectToDatabase();
GetVersionTable();

foreach (@::legal_product)
	{
	my $dir = "data/mining";
	&check_data_dir ($dir);
	&collect_stats ($dir, $_);
	}

sub check_data_dir
	{
	my $dir = shift;

	if (! -d)
		{
		mkdir $dir, 0777;
		chmod 0777, $dir;
		}
	}

sub collect_stats
	{
	my $dir = shift;
	my $product = shift;
	my $when = localtime (time);

	my $query = <<FIN;
select count(bug_status) from bugs where 
(bug_status='NEW' or  bug_status='ASSIGNED' or bug_status='REOPENED')
and product='$product' group by bug_status
FIN
	
	my $file = join '/', $dir, $product;
	my $exists = -f $file;

	if (open DATA, ">>$file")
		{
		SendSQL ($query);

		my %count;
		push my @row, &today;
		
		while (my @n = FetchSQLData())
			{
			push @row, @n;
			}
		
		if (! $exists)
			{
			print DATA <<FIN;
# Bugzilla daily bug stats
#
# do not edit me! this file is generated.
# 
# fields: date|new|assigned|reopened
# product: $product
# created: $when
FIN
			}

		print DATA (join '|', @row) . "\n";
		close DATA;
		}
	else
		{
		print "$0: $file, $!";
		}
	}

sub today
  {
  my ($dom, $mon, $year) = (localtime(time))[3, 4, 5];
  return sprintf "%04d%02d%02d", 1900 + $year, ++$mon, $dom;
  }

