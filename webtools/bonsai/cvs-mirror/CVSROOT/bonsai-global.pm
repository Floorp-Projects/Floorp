#!/usr/local/bin/perl -w
#use diagnostics;
use DBI;
use strict;
use Time::HiRes qw(time);
use Text::Soundex;
#use Data::Dumper;

sub checkin {
	my ($cwd, $user, $time, $dir, $cvsroot, $log, $change, $hashref) = @_;
	my ($db, $ci_id, %ch_id, $branch, $file, $f_ref, $c_ref);
	unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
	unless ($::last_insert_id{$db}) { $::last_insert_id{$db} = $::dbh{$db}->prepare("select LAST_INSERT_ID()") }
	$::dbh{$db}->do("insert into checkin set user_id=" . &id('user', $user)
								   . ", directory_id=" . &id('directory', $dir)
										 . ", log_id=" . &id('log', $log)
									 . ", cvsroot_id=" . &id('cvsroot', $cvsroot)
										   . ", time=$time");
	$ci_id = $::dbh{$db}->selectrow_array($::last_insert_id{$db});
###
### The Following should be put into a subroutine
###
	my $insert_change = $::dbh{$db}->prepare("insert into `change` (checkin_id, file_id, oldrev, newrev, branch_id) values (?, ?, ?, ?, ?)");
#	my $insert_change_map = $::dbh{$db}->prepare("insert into checkin_change_map (checkin_id, change_id) values (?, ?)");
	while (($branch, $f_ref) = each %$change) {
		while (($file, $c_ref) = each %$f_ref) {
			$insert_change->execute($ci_id, &id('file', $file), ${$c_ref}{'old'}, ${$c_ref}{'new'}, &id('branch', $branch));
			$ch_id{$file} = $::dbh{$db}->selectrow_array($::last_insert_id{$db});
#			$insert_change_map->execute($ci_id, $::dbh{$db}->selectrow_array($::last_insert_id{$db}));
		}
	}
###
### End "needs to be in a subroutine"
###
	return ($ci_id, \%ch_id);
}

sub insert_mirror_object {
#	print "inserting mirror_object...\n";
	my ($db, $mirror_id, $bro, $hashref);
	$db = $::default_db;
	unless ($::last_insert_id{$db}) { $::last_insert_id{$db} = $::dbh{$db}->prepare("select LAST_INSERT_ID()") }
	my $insert_mirror = $::dbh{$db}->prepare("insert into mirror (checkin_id, branch_id, cvsroot_id, offset_id, status_id) values (?, ?, ?, ?, ?)");
	my $insert_mirror_map = $::dbh{$db}->prepare("insert into mirror_change_map (mirror_id, change_id, type_id, status_id) values (?, ?, ?, ?)");
	while (($bro, $hashref) = each %::mirror_object) {
		my ($branch, $cvsroot, $offset) = split /@/, $bro;
		$offset = '' unless $offset;
		$insert_mirror->execute($::id, &id('branch', $branch), &id('cvsroot', $cvsroot), &id('offset', $offset), &id('status', 'building_mirror'));
		my $mirror_id = $::dbh{$db}->selectrow_array($::last_insert_id{$db});
#		print "mirror_id: $mirror_id\nbranch: $branch\ncvsroot: $cvsroot\noffset: $offset\n";
		while (my($ch_id, $type) = each %$hashref) {
			$insert_mirror_map->execute($mirror_id, $ch_id, &id('type', $type), &id('status', &nomirrored($::logtext)?'nomirror':'pending'));
		$::dbh{$db}->do("UPDATE `mirror` SET status_id = ? WHERE id = ?", undef, &id('status', 'pending'), $mirror_id);

#			print "\t-- $ch_id --> $type\n";
		}
	}
}

sub pop_rev {
# used to migrate to a different schema
	my $db = $::default_db ;
	my $new_total = 0;
	my $old_total = 0;
	my $pop_newrev = $::dbh{$db}->prepare("update `change` set newrev = ? where newrev_id = ?");
	my $pop_oldrev = $::dbh{$db}->prepare("update `change` set oldrev = ? where oldrev_id = ?");
	my $revisions = $::dbh{$db}->selectall_arrayref("select id, value from revision");
	for my $row (@$revisions) {
		my ($id, $value) = @$row;
print "$id->$value: ";
		my $new = $pop_newrev->execute($value, $id);
		my $old = $pop_oldrev->execute($value, $id);
		$new_total += $new;
		$old_total += $old;
print "new($new) -- old($old)\n";
	}
	print "\nnew_total = $new_total\nold_total = $old_total\n";
}

sub pop_chid {
# used to migrate to a different schema
	my $db = $::default_db ;
	my $new_total = 0;
	my $old_total = 0;
	my $pop_chid = $::dbh{$db}->prepare("update `change` set checkin_id = ? where id = ?");
	my $map = $::dbh{$db}->selectall_arrayref("select checkin_id, change_id from checkin_change_map");
	for my $row (@$map) {
		my ($check, $change) = @$row;
print "$check->$change: ";
		my $new = $pop_chid->execute($check, $change);
		$new_total += $new;
print "changed($new)\n";
	}
	print "\nnew_total = $new_total\nold_total = $old_total\n";
}

sub bonsai_groups {
	my ($user) = @_;
	unless ($::groups{'bonsai'}) {
		my $db = $::default_db ;
		my $groups = $::dbh{$db}->selectcol_arrayref(
			"SELECT g.value FROM `group_user_map` m, `group` g, `user` u " .
			"WHERE m.user_id = u.id AND m.group_id = g.id AND u.value = ?",
			undef, $user);
		$::groups{'bonsai'} = [ uniq(@$groups), "#-all-#" ];
	}
	return $::groups{'bonsai'};
}

sub unix_groups {
	my ($user) = @_;
	unless ($::groups{'unix'}) {
		my @groups;
		my $gid = (getpwnam($user))[3];
		my $grp = scalar getgrgid($gid) if $gid;
		return @groups unless $grp;
		push @groups, $grp;
		while (my ($name, $passwd, $gid, $members) = getgrent) {
			for my $m (split /\s/, $members) {
				if ($m eq $user) {
					push @groups, $name;
					next;
				}
			}
		}
		endgrent;
		$::groups{'unix'} = [ &uniq(@groups), "#-all-#" ];
	}
	return $::groups{'unix'};
}

sub branch_eol {
	my ($r, @ba) = @_;
	my $where = 
	my $db = $::default_db ;
	return $::dbh{$db}->selectcol_arrayref(
		"SELECT b.value FROM `cvsroot_branch_map_eol` m, `cvsroot` r, `branch` b " .
		"WHERE m.cvsroot_id = r.id AND m.branch_id = b.id AND " .
		"(b.value = ?".(" OR b.value = ?" x $#ba).")",
		undef, @ba);
}

sub db_map {
	my ($table, @value) = @_;
	my ($set, $where);
	my $db = $::default_db ;
	$set = &db_map_clause($table, \@value, " , ");
	$where = &db_map_clause($table, \@value, " AND ");
	$::dbh{$db}->do("insert into `$table` set $set") unless &db_mapped($table, $where);
}

sub db_demap {
	my ($table, @value) = @_;
	my $where = &db_map_clause($table, \@value, " AND ");
	my $db = $::default_db ;
    $::dbh{$db}->do("delete from `$table` where $where");
}

sub db_mapped {
	my ($table, $where) = @_;
	my $db = $::default_db;
	my $mapped = $::dbh{$db}->selectrow_array("select count(*) from `$table` where $where");
	return $mapped;
}

sub db_map_clause {
	my ($table, $value, $joiner) = @_;
	my $string;
	$joiner = " " unless $joiner;
	my ($names, $rest) = split /_map/, $table;
	my (@name) = split /_/, $names;
	for my $i (0..$#name) {
		$string .= "`".$name[$i]."_id` = ".&id($name[$i], $$value[$i]).$joiner;
	}
	$string =~ s/$joiner$/ /;
#print "--> $string\n";
	return $string;
}

sub log_commit {
    my ($cwd, $user, $time, $dir, $cvsroot, $status, $files, $hashref) = @_;
    my $db;
    unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
#&debug_msg("\n###\n### ".$::dbh{$db}->quote($status)."\n###\n", 9);
    $::dbh{$db}->do("insert into temp_commitinfo set user_id=" . &id('user', $user)
                                   . ", directory_id=" . &id('directory', $dir)
                                     . ", cvsroot_id=" . &id('cvsroot', $cvsroot)
                                          . ", files=" . $::dbh{$db}->quote($files)
                                            . ", cwd=" . $::dbh{$db}->quote($cwd)
                                         . ", status=" . $::dbh{$db}->quote($status)
                                           . ", time=$time");
}

sub collapse_HOH {
	my ($HOHref, $arrayref) = @_;
#print "#" x 80, "\n", Dumper($HOHref);
	while (my ($key, $hashref) = each %$HOHref) {
		for my $subkey (@$arrayref) {
#			delete $HOHref->{$key} unless $hashref->{$subkey} ;
			delete $HOHref->{$key} unless defined $hashref->{$subkey} ;
		}
	}
#print Dumper($HOHref), "#" x 80 , "\n";
}

sub collapse_HOHOH {
	my ($HOHOHref, $arrayref) = @_;
#print "#" x 80, "\n", Dumper($HOHOHref);
	while (my ($key, $hashref) = each %$HOHOHref) {
		&collapse_HOH($hashref, $arrayref);
	}
#print Dumper($HOHOHref), "#" x 80 , "\n";
}

sub update_commit {
	my ($cwd, $user, $time, $dir, $cvsroot, $status, $hashref) = @_;
	my $db;
	unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
	unless ($::update_temp_commitinfo{$db}) { 
		$::update_temp_commitinfo{$db} = $::dbh{$db}->prepare("update temp_commitinfo set status = ?"
																				   . " where cwd = ?"
																				 . " and user_id = ?"
																					. " and from_unixtime(time) > date_sub(from_unixtime(?), interval 2 hour)"
												 							. " and directory_id = ?"
																			  . " and cvsroot_id = ?")
	}
	$::update_temp_commitinfo{$db}->execute($status, $cwd, &id('user', $user), $time, &id('directory', $dir), &id('cvsroot', $cvsroot));
}

sub delete_commit {
	my ($cwd, $user, $time, $dir, $cvsroot, $hashref) = @_;
	my $db;
	unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
    $::dbh{$db}->do("delete from temp_commitinfo where cwd=" . $::dbh{$db}->quote($cwd)
										   . " and user_id=" . &id('user', $user)
											 . " and from_unixtime(time) > date_sub(from_unixtime($time), interval 2 hour)"
                                   	 . " and directory_id=" . &id('directory', $dir)
                                       . " and cvsroot_id=" . &id('cvsroot', $cvsroot));
}

sub log_performance {
	my ($table, $ci_id, $time, $hashref) =@_;
	my $db;
	unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
	$::dbh{$db}->do("insert into `$table` set checkin_id=" . $::dbh{$db}->quote($ci_id)
											 . ", time=$time");
}

sub store {
	my ($table, $value, $other_ref, $hashref) = @_;
	my ($db, $column, $other, $stored_value, @bind);
	unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
	unless ($column = ${$hashref}{"column"}) { $column = $::default_column }
    while (my ($col, $val) = each %$other_ref) {
        $other .= ", ".$col ." = ? ";
        if ($col =~ /.*_id$/) {
            $col =~ s/_id$//;
            push @bind, &id($col, $val);
        } else {
            push @bind, $val;
        }
    }
	$other .= " ";
	$Data::Dumper::Indent = 0;
	$Data::Dumper::Terse = 1;
	$stored_value = Dumper($value);
#print "insert into $table set value = ? $other\n";
#for my $i (@bind) { print "$i\n" }
    $::dbh{$db}->do("insert into `$table` set value = ? $other", undef, $stored_value, @bind);
}

sub retrieve {
	my ($table, $where_ref, $hashref) = @_;
	my ($db, $column, $value, $where, @bind);
	unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
	unless ($column = ${$hashref}{"column"}) { $column = $::default_column }
	while (my ($col, $val) = each %$where_ref) {
		$where .= $col ." = ? AND ";
		if ($col =~ /.*_id$/) {
			$col =~ s/_id$//;
			push @bind, &id($col, $val);
		} else {
			push @bind, $val;
		}
	}
	$where .= "1";
	$value = $::dbh{$db}->selectrow_array("SELECT $column FROM `$table` WHERE $where ORDER BY id DESC LIMIT 1", undef, @bind);
#print "SELECT $column FROM $table WHERE $where ORDER BY id DESC LIMIT 1", @bind;
#for my $i (@bind) { print "$i\n" }
	return $value;
}

sub id {
	my ($table, $value, $hashref) = @_;
	my ($db, $column, $key, $id);
	unless ($db = ${$hashref}{"db"}) { $db = $::default_db }
	unless ($column = ${$hashref}{"column"}) { $column = $::default_column }
	unless ($key = ${$hashref}{"key"}) { $key = $::default_key }
	unless ($::get_id{$db}{$table}) { $::get_id{$db}{$table} = $::dbh{$db}->prepare("select $key from `$table` where $column = ?")}
	unless ($id = $::dbh{$db}->selectrow_array($::get_id{$db}{$table}, "", ($value))) {
		unless ($::insert_value{$db}{$table}) { $::insert_value{$db}{$table} = $::dbh{$db}->prepare("insert into `$table` ($column) values (?)") }
		unless ($::last_insert_id{$db}) { $::last_insert_id{$db} = $::dbh{$db}->prepare("select LAST_INSERT_ID()") }
		$::insert_value{$db}{$table}->execute($value);
		$id = $::dbh{$db}->selectrow_array($::last_insert_id{$db});
	}
}

sub connect {
	my $db;
	unless (($db) = @_) { $db = $::default_db }
	$::dbh{$db} = DBI->connect($::db{$db}{'dsn'}, $::db{$db}{'user'}, $::db{$db}{'pass'}, { PrintError => 1, RaiseError => 1 })
}

sub disconnect {
    my $db;
    unless (($db) = @_) { $db = $::default_db }
    $::dbh{$db}->disconnect();
}

sub debug_msg {
    my ($msg, $level, $hashref) = @_;
    my ($showtime, $timeformat, $prefix);
    my %time = (
        "epoch" => time,
        "local" => scalar localtime,
    );
	return $time{'epoch'} unless $::debug;
    unless ($showtime = ${$hashref}{"showtime"}) { $showtime = 'no' }
    unless ($timeformat = ${$hashref}{"timeformat"}) { $timeformat = 'local' }
	unless ($prefix = ${$hashref}{"prefix"}) { $prefix = '#' }
    if ($::debug_level >= $level) {
        print $prefix x $level . " " unless $prefix =~ /none/i;
        print $time{$timeformat}." " unless $showtime =~ /no/i;
		print "$msg\n";
    }
    return $time{'epoch'};
}

sub get {
    my ($item, @line) = @_;
    my %i = (
        'code' => 0,
        'file' => 1,
         'rev' => 2,
        'time' => 3,
         'opt' => 4,
         'tag' => 5,
        );
#print "=== $item : $i{$item} : $line[$i{$item}] -- " , Dumper(\@line);
    if ($item eq "tag") {
        $line[$i{$item}] = "TTRUNK" unless (defined $line[$i{$item}]);
        $line[$i{$item}] =~ s/^T//;
    }
#    if ($item eq "rev") {
#        $line[$i{$item}] = "NONE" if $line[$i{$item}] eq "0";
##       $line[$i{$item}] =~ s/^-//;
#    }
    return $line[$i{$item}];
}

sub BuildModuleHash {
   my ($modules) = @_;
   my $modules_hash;
#print "$modules";
   chomp $modules;
   $modules =~ s/\s*#.*\n?/\n/g ;     # remove commented lines
   $modules =~ s/^\s*(.*)/$1/ ;       # remove blank lines before module definitions
   $modules =~ s/\s*\\\s*\n\s*/ /g ;  # join lines continued with \
   $modules =~ s/\n\s+/\n/g ;         # remove leading whitespace
   $modules =~ s/\s+-[^la]\s+\S+//g ; # get rid of the arguments (and flags) to flags other than 'a' and 'l'
   $modules =~ s/\s+-[la]\s+/ /g ;    # get rid of the 'a' and 'l' **** FIXME: l needs an ending or something ****
#print "---\n$modules\n---\n";
   my @modules = split(/\n/, $modules);
   for my $line (@modules) {
      my @line = split(" ", $line);
      my $name = shift @line ;
      $modules_hash->{$name} = [ @line ];
      undef $name;
      undef $line;
      undef @line;
   }
#print Dumper($modules_hash);
return $modules_hash;
}

sub uniq {
	my @list = @_;
	my %hash;
	for my $item (@list) { $hash{$item}++ }
	return keys(%hash);
}

sub FlattenHash {
# Remove duplicate entries in hash-arrays
	my $hashref = $_[0];
	my ($key, $value);
	while (my ($key, $value) = each %$hashref) {
		$$hashref{$key} = [ &uniq(@$value) ];
	}
}

sub FormatModules {
	my ($hashref, $cvsroot) = @_ ;
	my %new_hash;
	while (my ($module, $list) = each %$hashref) {
		for my $item (@$list) {
			if ( $item =~ s/^!// ) {
				&make_module_regex(\%new_hash, $item, "exclude", $module, $cvsroot);
			} else {
				&make_module_regex(\%new_hash, $item, "include", $module, $cvsroot);
			}
		}
	}
	return \%new_hash;
}

sub make_module_regex {
	my ($hash, $item, $type, $module, $cvsroot) = @_;
	if ( -d "$cvsroot/$item" ) {
		push @{$$hash{$module}{$type."_directory"}}, "^\Q$item\E/.+\$";
	} else {
#       $item =~ s/^(.*\/)?(.*)$/$1(Attic\/)?$2/;
		push @{$$hash{$module}{$type."_file"}}, "^\Q$item\E\$";
	}
}

sub format_mirrorconfig {
	my ($mirrors) = @_;
	for my $m (@$mirrors) {
		for my $t ("mirror", "overwrite", "exclude") {
			next unless $m->{$t};
			while (my ($c, $a) = each %{$m->{$t}}) {
				next unless ($c eq "directory" || $c eq "file");
				my %n;
				for my $i (@$a) {
					if ($c eq "directory") {
						push @{$n{"include_".$c}}, "^\Q$i\E/.+\$";
					} else {
						push @{$n{"include_".$c}}, "^\Q$i\E\$";
					}
				}
				&FlattenHash(\%n);
				$m->{$t}->{$c} = \%n;
			}
		}
	}
}

sub format_accessconfig {
	my ($aa) = @_;
	for my $ah (@$aa) {
		next unless $ah->{'location'};
		while (my ($c, $a) = each %{$ah->{'location'}}) {
			next unless ($c eq "directory" || $c eq "file");
			my %n;
			for my $i (@$a) {
				if ($c eq "directory") {
					push @{$n{"include_".$c}}, "^\Q$i\E/.+\$";
				} else {
					push @{$n{"include_".$c}}, "^\Q$i\E\$";
				}
			}
			&FlattenHash(\%n);
			$ah->{'location'}->{$c} = \%n;
		}
	}
}

sub expand_mirror_modules {
	my ($mirrors) = @_;
	my $modules_hashref;
	for my $m (@$mirrors) {
		my $r = $m->{'from'}->{'cvsroot'};
		$modules_hashref->{$r} = eval &retrieve("modules", {"cvsroot_id" => $r});
		for my $t ("mirror", "overwrite", "exclude") {
			next unless $m->{$t};
			my $a = $m->{$t}->{'module'};
			next unless defined $a;
			my %n;
			for my $i (@$a) {
				if (\%n) {
					while (my ($inc, $inc_array) = each %{$modules_hashref->{$r}->{$i}}) {
						push @{$n{$inc}}, @$inc_array;
					}
				} else {
					\%n = $modules_hashref->{$r}->{$i};
				}
			}
			&FlattenHash(\%n);
			$m->{$t}->{'module'} = \%n;
		}
	}
}

sub expand_access_modules {
	my ($aa) = @_;
	my $modules_hashref;
	for my $ah (@$aa) {
		next unless $ah->{'location'};
		my $r = $ah->{'cvsroot'};
		$modules_hashref->{$r} = eval &retrieve("modules", {"cvsroot_id" => $r}) unless $r eq "#-all-#";
		my $a = $ah->{'location'}->{'module'};
		next unless defined $a;
		my %n;
		for my $i (@$a) {
			if (\%n) {
				while (my ($inc, $inc_array) = each %{$modules_hashref->{$r}->{$i}}) {
					push @{$n{$inc}}, @$inc_array;
				}
			} else {
				\%n = $modules_hashref->{$r}->{$i};
			}
		}
		&FlattenHash(\%n);
		$ah->{'location'}->{'module'} = \%n;
	}
}

sub ExpandHash {
   my $hash = $_[0] ;
   my $hash2 = $_[1] ? $_[1] : $_[0] ;
#   &CheckCircularity($hash2);
#   print "not circular\n";
   my $done = 0 ;
   until ($done) {
      $done = 1 ;
      for my $key (keys %$hash) {
         for my $i (0..$#{$$hash{$key}}) {
            if (exists ($$hash2{$$hash{$key}[$i]})) {
               $done = 0 ;
               splice ( @{$$hash{$key}}, $i, 1, @{$$hash2{$$hash{$key}[$i]}} ) ;
            }
         }
      }
   }
}

sub CheckCircularity {
   my $hash = $_[0] ;
   my @LHS ;
   my @RHS ;
   my $count = 0 ;
   for my $k (keys(%$hash)) {
      $LHS[$count]=$k ;
	  $RHS[$count]=join(':', @{$hash->{$k}}) ;
      $count++ ;
   }
#---------------------------------------------#
# check for, and report,  circular references #
#---------------------------------------------#
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
#for (my $i=0; $i<=$#LHS; ++$i) {print "$LHS[$i] = $RHS[$i]\n";} #
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
   my $sort_count = 0 ;
   my $unsorted_end = $#RHS ;
   SORT: for (my $i=$unsorted_end; $i>=0; --$i) {
      my $search_name = $LHS[$i] ;
      for (my $j=$i; $j>=0; --$j) {
         if ($RHS[$j] =~ /^$search_name:|:$search_name:|:$search_name$|^$search_name$/){
            unshift @LHS, $LHS[$i] ;
            unshift @RHS, $RHS[$i] ;
            splice @LHS, $i+1, 1 ;
            splice @RHS, $i+1, 1 ;
            ++$sort_count ;
            if ($sort_count == $i+1) { 
               print "\ncircular reference involving the following:\n\n" ;
               print "\t$LHS[0]" ;
               for my $x (1..$i) { print " : $LHS[$x]" }
               print "\n" ;
                  for my $x (0..$i) {
                     $RHS[$x] =~ s/:/  &  /g ;
                     print "\n\t$LHS[$x]  -->  $RHS[$x]" ;
               }
              print "\n\nyou suck, try again.\n\n" ;
              exit 1;
            }
            goto SORT ;
         }
      }
      --$unsorted_end ;
      $sort_count = 0 ;
   }
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
#print "\n";                                                     #
#for (my $i=0; $i<=$#LHS; ++$i) {print "$LHS[$i] = $RHS[$i]\n";} #
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~#
}

sub mirrored_checkin {
	my ($log) = @_;
#	print "$log\n========\n";
	for my $regex (@::mirrored_checkin_reqex) {
#		print "-- $regex\n";
#		print "\nNO MIRROR FOR YOU\n\n" if ($log =~ $regex);
		return 1 if ($log =~ $regex);
	}
	return 0;
}

sub nomirrored {
	my ($directive) = @_;
	if ($directive =~ /[#-]\s*[#-]([^-#]+)[#-]?\s*[#-]?/) {
	###if ($directive =~ /#\s*-(.*)-\s*#/) {
		$directive = $1
	} else {
		$directive = "none"
	}
	print "directive --> $directive (", soundex($directive), ")\n" if $directive;
	return 1 if (soundex($directive) eq soundex("nomirror"));
	return 0;
}

sub create_mirrors {
	my ($change_ref, $mirror_ref) = @_ ;
	my (@bro_array, %bro_hash);
	while (my ($from, $branch_change_ref) = each %$change_ref) {
		my $start_bro = $from."@".$::cvsroot."@";
#		$Data::Dumper::Indent = 0 ;
#		print "$start_bro -- $::directory -> " . Dumper([keys(%$branch_change_ref)]) , "\n";
#		$Data::Dumper::Indent = 2 ;
		$bro_hash{$start_bro} = {'start' => $start_bro, 'files' => [keys(%$branch_change_ref)], 'offset' => ''};
	}
#	print Dumper(\%bro_hash);
	&make_mirror(\%bro_hash);
#	print Dumper(\%::mirror_object);
#	print "\n\nlogging mirror_object to database\n\n";
	&insert_mirror_object;
}

sub make_mirror {
	my ($hash) = @_;
	my (%sub);
	my %x = ("exclude" => 0, "overwrite" => 1, "mirror" => 2);
	while (my ($bro, $details) = each %$hash) {
		my ($bro_branch, $bro_root, $bro_offset, @old_bro) = split ("@", $bro);
		$bro_offset = "" unless $bro_offset;
		my $new_bro = $bro_branch."@".$bro_root."@".$bro_offset;
		for my $m (@$::mirror) {
			for my $to (@{$m->{to}}) {
				my @sub_files;
				if ($m->{from}->{branch} eq $bro_branch && $m->{from}->{cvsroot} eq $bro_root) {
					my $adjusted_offset = &adjust_offset($bro_offset, $to->{'offset'});
					my $sub_bro = "$to->{branch}\@$to->{cvsroot}\@$adjusted_offset";
					if ($sub_bro ne $details->{'start'}) {
						for my $f (@{$details->{'files'}}) {
							my $type = &check_mirror($f, $m, $details->{'offset'});
							if ($type && $x{$type} > 0) {
								unless (defined $::mirror_object{$sub_bro}->{$::ch_id_ref->{$f}}) {
									push @{$sub{$sub_bro."@".$new_bro}->{'files'}}, $f;
								}
								unless ( defined $::mirror_object{$sub_bro}->{$::ch_id_ref->{$f}} &&
								         $x{$type} < $x{$::mirror_object{$sub_bro}->{$::ch_id_ref->{$f}}} ) {
									$::mirror_object{$sub_bro}->{$::ch_id_ref->{$f}} = $type;
								}
							}
						}
						if (defined $sub{$sub_bro."@".$new_bro}) {
							$sub{$sub_bro."@".$new_bro}{'start'} = $details->{'start'};
							$sub{$sub_bro."@".$new_bro}{'offset'} = $adjusted_offset;
						}
					}
				}
			}
		}
	}
#	print "SUB-" x 19, "SUB\n", Dumper(\%sub), "SUB-" x 19, "SUB\n";
#	print "\n###\n### Making more mirrors\n###\n" if %sub;
#	if ($::COUNT++ < 100) {
#		print "$::COUNT-" x 39, "$::COUNT\n", Dumper(\%::mirror_object);
	&make_mirror(\%sub) if %sub;
#	}
}

sub adjust_offset {
    my ($p, $o) = @_;
    my ($pL, $pR, $oL, $oR);
    ($pL, $pR) = split /\|/, $p;
    ($oL, $oR) = split /\|/, $o;
    ($pL, $oR) = &shorten($pL, $oR, 1);
    ($pR, $oL) = &shorten($pR, $oL, 0);
    my $n = $pL.$oL."|".$oR.$pR;
    return $n ne "|" ? $n : "" ;
}

sub shorten {
    my ($a, $b, $e) = @_;
	my $x;
    $a = '' unless $a;
    $b = '' unless $b;
    if (length $a < length $b) {
        $x = $a;
    } else {
        $x = $b;
    }
    if ($e) {
        $x = "\Q$x\E\$" ;
    } else {
        $x = "^\Q$x\E" ;
    }
    if ($a =~ /$x/ && $b =~ /$x/) {
        $a =~ s/$x//;
        $b =~ s/$x//;
    }
    return ($a, $b);
}

sub check_mirror {
	my ($file, $mirror_hashref, $offset) = @_;
	my $type = undef;
	for my $t ("mirror", "overwrite", "exclude") {
#		print "$file -- $t -- $mirror_hashref->{'from'}->{'branch'} --> ";
#		print Dumper($mirror_hashref->{'to'}), "\n";
		if (defined $mirror_hashref->{$t}) {
			$type = $t if &allowed($file, $mirror_hashref->{$t}, $offset);
		}
	}
#	print Dumper($mirror_hashref);
	return $type;
}

sub allowed {
	my ($file, $type_hashref, $offset) = @_;
	my %x = ('exclude' => 0, 'include' => 1);
#	for my $st ("module", "directory", "file") {
	for my $st ("file", "directory", "module") {
		if (defined $type_hashref->{$st}) {
#			print "=== $st", Dumper($type_hashref->{$st});
			for my $type ("file", "directory") {
				for my $clude ("exclude", "include") {
					if (defined $type_hashref->{$st}->{$clude."_".$type}) {
						return $x{$clude} if &match_array($file, $type_hashref->{$st}->{$clude."_".$type}, $offset) ;
					}
				}
			}
		}
	}
	return 0;
}

sub match_array {
	my ($file, $regex_arrayref, $offset) = @_;
	$file = $::directory."/".$file if $::directory;
	$offset = "|" unless $offset;
	my ($from_offset, $to_offset) = split /\|/, $offset;
	$file =~ s/\Q$from_offset\E/$to_offset/;
	for my $r (@$regex_arrayref) {
##
#		print "#####".$file."#####".$r."#####";
#		if ($file =~ /$r/) {
#			print " MATCH\n";
#			return 1;
#		} else {
#			print " NO-MATCH\n";
#		}
##
		return 1 if $file =~ /$r/ ;
	}
	return 0;
}

return 1;
