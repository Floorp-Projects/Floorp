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
# Contributor(s): Harrison Page <harrison@netscape.com>
#                 Terry Weissman <terry@mozilla.org>

# harrison@netscape.com
#
# 1.0 10/16/98 

package Tinderbox;

require Exporter;

use strict 'vars';
use vars qw (@ISA @EXPORT $VERSION);
use LWP::Simple;
# use HTML::Parse;
use Carp;

@ISA = qw (Exporter);
@EXPORT = qw (status statuz);

my $VERSION = "1.0";

# status wants a reference to a list of tinderbox trees
# and a url ending with tree=, default to mozilla.org's
# server if not provided. status returns two references
# to hashes. the first contains tree names as key, 
# tree status as value. second hash contains trees to 
# whether or tree is open or closed.
#
# tree status can be horked or success.
#
# barf.

sub status
	{
	my $trees = shift;
	my $url = shift;
	my %info; my %tree_state;
  
	# maybe this is too helpful

  if (ref ($trees) ne "ARRAY")
    {
    carp "status method wants a reference to a list, not a " . ref ($trees);
    return;
    }

  $url = $url || "http://cvs-mirror.mozilla.org/webtools/tinderbox/" .
    "showbuilds.cgi?quickparse=1&tree="; 
	
  my $output = get $url . join ',', @$trees;
	return if (! $output); 
	
	my @qp = split /\n/, $output;
	
	# loop through quickparse output

	foreach my $op (@qp)
		{
		my ($type, $tree, $build, $state) = split /\|/, $op;

		if ($type eq "State")
			{
			$tree_state{$tree} = $state;
			}
		elsif ($type eq "Build")
			{
				if ($state =~ /success/i) {
					$state = "Success";
				} else {
					$state = "Horked";
				}
				$info{$tree}{$build} = $state;
			}
		}
	
	return (\%info, \%tree_state);
	}

1;
