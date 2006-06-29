# -*- mode: cperl; c-basic-offset: 8; indent-tabs-mode: nil; -*-

=head1 COPYRIGHT

 # ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1
 #
 # The contents of this file are subject to the Mozilla Public License
 # Version 1.1 (the "License"); you may not use this file except in
 # compliance with the License. You may obtain a copy of the License
 # at http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS"
 # basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 # the License for the specific language governing rights and
 # limitations under the License.
 #
 # The Original Code is Litmus.
 #
 # The Initial Developer of the Original Code is
 # the Mozilla Corporation.
 # Portions created by the Initial Developer are Copyright (C) 2006
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #   Zach Lipton <zach@zachlipton.com>
 #
 # ***** END LICENSE BLOCK *****

=cut

package Litmus::XML; 

use strict;

use XML::XPath;
use XML::XPath::XMLParser;

use Litmus::DB::User;
use Litmus::UserAgentDetect;
use Date::Manip;

use CGI::Carp qw(set_message fatalsToBrowser);
BEGIN { 
	set_message(sub { 
		print "Fatal error: internal server error\n";
	});
}

no warnings;
no diagnostics;

sub new {
	my $self = {};
	bless($self);
	return $self;
}

# process XML test result data as described by 
# the spec at http://wiki.mozilla.org/Litmus:Web_Services
sub processResults {
	my $self = shift;
	my $data = shift;
	
	$self->parseResultFile($data) ? 1 : return 0;
	
	unless ($self->authenticate()) { return 0} # login failure
	
	$self->validateResults() ? 1 : return 0;
	
	# at this point, everything is valid, so if we're just validating the 
	# results, we can return an ok:
	if ($self->{'action'} eq 'validate') {
		unless  ($self->{'response'}) { $self->respOk() }
		return 1;
	}
	
	# add so-called 'global logs' that apply to all the results
	# we save them in @globallogs so we can map them to the results later
	my @globallogs;
	foreach my $log (@{$self->{'logs'}}) {
		# the submission time is the timestamp of the first testresult:
		my $newlog = Litmus::DB::Log->create({
			submission_time => $self->{'results'}->[0]->{'timestamp'},
			log_type => $log->{'type'},
			log_text => $log->{'data'},
		});
		push(@globallogs, $newlog);
	}
	
	# now actually add the new results to the db:
	foreach my $result (@{$self->{'results'}}) {
		my $newres = Litmus::DB::Testresult->create({
			testcase      => $result->{'testid'},
			user_agent    => new Litmus::UserAgentDetect($self->{'useragent'}),
			user 	      => $self->{'user'}, 
			opsys 	      => $self->{'sysconfig'}->{'opsys'},
			branch 	      => $self->{'sysconfig'}->{'branch'},
			locale	      => $self->{'sysconfig'}->{'locale'},
			build_id      => $self->{'sysconfig'}->{'buildid'},
			machine_name  => $self->{'machinename'},
			result_status => $result->{'resultstatus'},
			timestamp     => $result->{'timestamp'},
			exit_status   => $result->{'exitstatus'},
			duration_ms   => $result->{'duration'},
			valid		  => 1,
			isAutomated   => $result->{'isAutomated'},
		});
		
		if (!$newres) { $self->respErrResult($result->{'testid'}); next; }
		
		# add any bug ids:
		foreach my $bug (@{$result->{'bugs'}}) {
			my $newbug = Litmus::DB::Resultbug->create({
				test_result_id  => $newres,
				bug_id 		    => $bug, 
				submission_time => $result->{'timestamp'},
				user		    => $self->{'user'},
			});
		}
		
		# add any comments:
		foreach my $comment (@{$result->{'comments'}}) {
			my $newcomment = Litmus::DB::Comment->create({
				test_result     => $newres,
				submission_time => $result->{'timestamp'},
				user            => $self->{'user'},
				comment         => $comment,
			});
		}
	
		# add logs:
		my @resultlogs;
		push(@resultlogs, @globallogs); # all results get the global logs
		foreach my $log (@{$result->{'logs'}}) {
			my $newlog = Litmus::DB::Log->create({
				submission_time => $result->{'timestamp'},
				log_type => $log->{'type'},
				log_text => $log->{'data'},
			});
			push(@resultlogs, $newlog);
		}
		
		# now we map the logs to the current result:
		foreach my $log (@resultlogs) {
			Litmus::DB::LogTestresult->create({
				test_result => $newres,
				log_id => $log,
			});
		}
	}
	
	unless  ($self->{'response'}) { $self->respOk() }
	
	#$self->{'response'} = $self->{'results'}->[0]->{'resultstatus'};
	
}

sub parseResultFile {
	my $self = shift;
	my $data = shift;
	my $x = XML::XPath->new(xml => $data, standalone => 1);
	$self->{'useragent'} = $x->findvalue('/litmusresults/@useragent');
	$self->{'machinename'} = $x->findvalue('litmusresults/@machinename');
	$self->{'action'} = $x->findvalue('/litmusresults/@action');
	$self->{'user'}->{'username'} = $x->findvalue(
									'/litmusresults/testresults/@username');
	$self->{'user'}->{'token'} = $x->findvalue(
								   '/litmusresults/testresults/@authtoken');
	
	$self->{'sysconfig'}->{'product'} = $x->findvalue('/litmusresults/testresults/@product');
	$self->{'sysconfig'}->{'platform'} = $x->findvalue('/litmusresults/testresults/@platform');
	$self->{'sysconfig'}->{'opsys'} = $x->findvalue('/litmusresults/testresults/@opsys');
	$self->{'sysconfig'}->{'branch'} = $x->findvalue('/litmusresults/testresults/@branch');
	$self->{'sysconfig'}->{'buildid'} = $x->findvalue('/litmusresults/testresults/@buildid');
	$self->{'sysconfig'}->{'locale'} = $x->findvalue('/litmusresults/testresults/@locale');
	
	my @glogs = $x->find('/litmusresults/testresults/log')->get_nodelist();
	my $l_ct = 0;
	foreach my $log (@glogs) {
		my $type = $x->findvalue('@logtype', $log);
			my $logdata = stripWhitespace($log->string_value());
			$self->{'logs'}->[$l_ct]->{'type'} = $type;
			$self->{'logs'}->[$l_ct]->{'data'} = $logdata;
			$l_ct++;
	}
	
	my @results = $x->find('/litmusresults/testresults/result')->get_nodelist();
	my $c = 0;
	foreach my $result (@results) {
		$self->{'results'}->[$c]->{'testid'} = $x->findvalue('@testid', $result);
		$self->{'results'}->[$c]->{'isAutomated'} = $x->findvalue('@is_automated_result', $result);
		$self->{'results'}->[$c]->{'resultstatus'} = $x->findvalue('@resultstatus', $result);
		$self->{'results'}->[$c]->{'exitstatus'} = $x->findvalue('@exitstatus', $result);
		$self->{'results'}->[$c]->{'duration'} = $x->findvalue('@duration', $result);
		$self->{'results'}->[$c]->{'timestamp'} = 
			&Date::Manip::UnixDate($x->findvalue('@timestamp', $result), "%q");
		
		
		my @comments = $x->find('comment', $result)->get_nodelist();
		my $com_ct = 0;
		foreach my $comment (@comments) {
			$comment = stripWhitespace($comment->string_value());
			$self->{'results'}->[$c]->{'comments'}->[$com_ct] = $comment;
			$com_ct++;
		}
		
		my @bugs = $x->find('bugnumber', $result)->get_nodelist();
		my $bug_ct = 0;
		foreach my $bug (@bugs) {
			$bug = stripWhitespace($bug->string_value());
			$self->{'results'}->[$c]->{'bugs'}->[$bug_ct];
			$bug_ct++;
		}
		
		my @logs = $x->find('log', $result)->get_nodelist();
		my $log_ct = 0;
		foreach my $log (@logs) {
			my $type = $x->findvalue('@logtype', $log);
			my $logdata = stripWhitespace($log->string_value());
			$self->{'results'}->[$c]->{'logs'}->[$log_ct]->{'type'} = $type;
			$self->{'results'}->[$c]->{'logs'}->[$log_ct]->{'data'} = $logdata;
			$log_ct++;
		}	
		$c++;
	}
	
	$self->{'x'} = $x;
}

# validate the result data, and resolve references to various tables
# the correct objects, looking up id numbers as needed
sub validateResults {
	my $self = shift;

	my $action = $self->{'action'};
	if ($action ne 'submit' && $action ne 'validate') {
		$self->respErrFatal("Action must be either 'submit' or 'validate'");
		return 0;
	}
	
	my @users = Litmus::DB::User->search(email => $self->{'user'}->{'username'});
	$self->{'user'} = $users[0];
	
	
	unless ($self->{'useragent'}) { 
		$self->respErrFatal("You must specify a useragent");
		return 0;
	}
	
	my @prods = Litmus::DB::Product->search(name => $self->{'sysconfig'}->{'product'});
	unless ($prods[0]) { 
		$self->respErrFatal("Invalid product: ".$self->{'sysconfig'}->{'product'});
		return 0;
	}
	$self->{'sysconfig'}->{'product'} = $prods[0];
	
	my @platforms = Litmus::DB::Platform->search_ByProductAndName(
											$self->{'sysconfig'}->{'product'},
											$self->{'sysconfig'}->{'platform'});
	unless ($platforms[0]) { 
		$self->respErrFatal("Invalid platform: ".$self->{'sysconfig'}->{'platform'});
		return 0;
	}
	$self->{'sysconfig'}->{'platform'} = $platforms[0];
	
	my @opsyses = Litmus::DB::Opsys->search(
								name => $self->{'sysconfig'}->{'opsys'}, 
							    platform => $self->{'sysconfig'}->{'platform'});
	unless ($opsyses[0]) { 
		$self->respErrFatal("Invalid opsys: ".$self->{'sysconfig'}->{'opsys'});
		return 0;
	}
	$self->{'sysconfig'}->{'opsys'} = $opsyses[0];
	
	my @branches = Litmus::DB::Branch->search(
										name => $self->{'sysconfig'}->{'branch'},
										product => $self->{'sysconfig'}->{'product'});
	unless ($branches[0]) { 
		$self->respErrFatal("Invalid branch: ".$self->{'sysconfig'}->{'branch'});
		return 0;
	}
	$self->{'sysconfig'}->{'branch'} = $branches[0];
	
	unless ($self->{'sysconfig'}->{'buildid'}) {
		$self->respErrFatal("Invalid build id: ".$self->{'sysconfig'}->{'buildid'});
		return 0;
	}
	
	my @locales = Litmus::DB::Locale->search(
									locale => $self->{'sysconfig'}->{'locale'});
	unless ($locales[0]) { 
		$self->respErrFatal("Invalid locale: ".$self->{'sysconfig'}->{'locale'});
		return 0;
	}
	$self->{'sysconfig'}->{'locale'} = $locales[0];
	
	foreach my $log (@{$self->{'logs'}}) {
		my @types = Litmus::DB::LogType->search(name => $log->{'type'});
		unless ($types[0]) {
			$self->respErrFatal("Invalid log type: ".$log->{'type'});
			return 0;
		}
		$log->{'type'} = $types[0];
	}
	
	foreach my $result (@{$self->{'results'}}) {
		my @tests = Litmus::DB::Testcase->search(
									test_id => $result->{'testid'});
		unless ($tests[0]) { 
			$self->respErrResult('unknown', "Invalid test id");
			next;
		}
		$result->{'testid'} = $tests[0];
		
		# assume it's an automated test result if not specified
		($result->{'isAutomated'} eq '0' || $result->{'isAutomated'} ne undef) ? 
			$result->{'isAutomated'} = 0 : $result->{'isAutomated'} = 1;
		
		my @results = Litmus::DB::ResultStatus->search(
									name => $result->{'resultstatus'});
		unless ($results[0]) { 
			$self->respErrResult($result->{'testid'}, "Invalid resultstatus");
			next;
		}
		$result->{'resultstatus'} = $results[0];
		
		my @es = Litmus::DB::ExitStatus->search(
									name => $result->{'exitstatus'});
		unless ($es[0]) { 
			$self->respErrResult($result->{'testid'}, "Invalid exitstatus");
			next;
		}
		$result->{'exitstatus'} = $es[0];
		
		# if there's no duration, then it's just 0:
		unless ($result->{'duration'}) {
			$result->{'duration'} = 0;
		}
		
		# if there's no timestamp, then it's now:
		unless ($result->{'timestamp'}) {
			$result->{'timestamp'} = &Date::Manip::UnixDate("now","%q");
		}
		
		foreach my $log (@{$result->{'logs'}}) {
			my @types = Litmus::DB::LogType->search(name => $log->{'type'});
			unless ($types[0]) {
				$self->respErrResult($result->{'testid'}, 
									 "Invalid log type: ".$log->{'type'});
				next;
			}
			$log->{'type'} = $types[0];
		}		
	}
	return 1;
}

sub response {
	my $self = shift;
	return $self->{'response'};
}


# ONLY NON-PUBLIC API BELOW THIS POINT

sub authenticate {
	my $self = shift;
	
	my @users = Litmus::DB::User->search(email => $self->{'user'}->{'username'});
	my $user = $users[0];
	
	unless ($user) { $self->respErrFatal("User does not exist"); return 0 }
	
	unless ($user->enabled()) { $self->respErrFatal("User disabled"); return 0 }
	
	if ($user->authtoken() ne $self->{'user'}->{'token'}) {
		respErrFatal("Invalid authentication token for user ".
			$self->{'user'}->{'username'});
		return 0;
	}
	return 1;
}

sub respOk {
	my $self = shift;
	$self->{'response'} = 'ok';
}

sub respErrFatal {
	my $self = shift;
	my $error = shift;
	$self->{'response'} = "Fatal error: $error\n";
}

sub respErrResult {
	my $self = shift;
	my $testid = shift;
	my $error = shift;
	$self->{'response'} .= "Error processing result for test $testid: $error\n";
}

# remove leading and trailing whitespace from logs and comments
sub stripWhitespace {
	my $txt = shift;
	$txt =~ s/^\s+//;
	$txt =~ s/\s+$//;
	return $txt;
}



1;
