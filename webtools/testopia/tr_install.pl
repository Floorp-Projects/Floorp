#!/usr/bin/perl -w
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
# The Original Code is the Bugzilla Testopia System.
#
# The Initial Developer of the Original Code is Maciej Maczynski.
# Portions created by Maciej Maczynski are Copyright (C) 2001
# Maciej Maczynski. All Rights Reserved.
#
# Contributor(s): Maciej Maczynski <macmac@xdsnet.pl>
#                 Ed Fuentetaja <efuentetaja@acm.org>

use DBI;
use File::Copy;

require Bugzilla;
use Bugzilla::Constants;

my $VERSION = "1.0-beta";

#check that we are on the Bugzilla's directory:

if (! -e "testopia") {
  # I don't know where the heck I am
  DieWithStyle("Please unpack Testopia's distribution file and run this script from within Bugzilla's directory.\n".
    "Can't continue.\n");
}

if (! -e "localconfig") {
  DieWithStyle("Can't find file: localconfig\nBugzilla is not installed in this directory.\n".
    "Can't continue.\n");
}

print "Now installing Testopia ...\n\n";

do 'localconfig';

my $rollbackPatchOnFail = 0;
my $patchSuccessFile = "testopia/tr_patch_successful";
my @files = ();

my $patchFile = $ARGV[0];

if ($patchFile && $patchFile eq "-nopatch") {
  #skip patching
  print "Skip patching Bugzilla's files.\n";
} else {
  doPatch($patchFile);
}

print "\nSetting file permissions ...\n";
SetupPermissions();
print "Done.\n\n";

print "Checking Testopia database ...\n";
my ($dbh, $my_db_base, $my_db_host, $my_db_port, $my_db_name, $my_db_user, $my_db_sock) = DBConnect();
print "  Make  : $my_db_base\n";
print "  Host  : $my_db_host\n";
print "  Port  : $my_db_port\n";
print "  Name  : $my_db_name\n";
print "  User  : $my_db_user\n";
print "  Socket: $my_db_sock\n";
print "\n";
UpdateDB($dbh);
print "Done.\n\n";

print "Adding Testopia groups ...\n";
# Identify admin group.
my $adminid = GetAdminGroup($dbh);
#Create groups if needed and grant permissions to the admin group over them
my $groupid;
$groupid = tr_AddGroup($dbh, 'managetestplans', 'Can create, destroy, run and edit test plans.', $adminid);
tr_AssignAdminGrants($dbh, $groupid, $adminid);
$groupid = tr_AddGroup($dbh, 'edittestcases', 'Can add, delete and edit test cases.', $adminid);
tr_AssignAdminGrants($dbh, $groupid, $adminid);
$groupid = tr_AddGroup($dbh, 'runtests', 'Can add, delete and edit test runs.', $adminid);
tr_AssignAdminGrants($dbh, $groupid, $adminid);

print "Done.\n\n";

DBDisconnect($dbh);

#print "Copying Testopia templates ...\n";
#CopyTemplates('testopia');
#CopyTemplates('hook');
#print "Done.\n\n";

print "Cleaning up Testopia cache ...\n";
unlink "data/plancache";
print "Done.\n\n";

print "Running checksetup.pl ...\n";
`perl checksetup.pl`;
print "Done.\n\n";

#now recover permissions over this script:
chmod(0750, "tr_install.pl");

print "Testopia installed successfully!\n";

print "\n";
print "Please follow now the 'Users' link at the bottom of your Bugzilla's home page to assign users to groups 'Edittestcases' and 'Managetestplans'.\n";
print "\n";

1;


###############################################################################

sub CopyTemplates {

  my ($d) = @_;
  
  my $src = "testopia/template/en/default/$d";
  my $dst = "template/en/default/$d";
  
  _copyRec($src, $dst);  
}

sub _copyRec {

  my ($src, $dst) = (@_);
  
  opendir DIRH, $src;
  my @files = grep( $_ ne "." && $_ ne "..", readdir(DIRH));
  closedir DIRH;
      
  mkdir $dst;
  foreach my $f (@files) {
    if (-d "$src/$f") {
      # A directory
      _copyRec("$src/$f", "$dst/$f");
    } else {
      copy("$src/$f", "$dst/$f");
    }
  }
}

sub doPatch {

    my ($fPatch) = @_;
        
    if (-e $patchSuccessFile) {
    
      #TODO: check the version run, in case we are upgrading
      #since version 0.6.2 is the first version using this system
      #this won't be needed until next version (0.6.3 ?) comes along
      
      print "Patch already run successfully. Skipping.\n";
      return 1;
    }
    
    print "Patching Bugzilla's files ...\n";
        
    if ($fPatch) {
      # check that the patch file exists:
      if (!-e $fPatch) {
        DieWithStyle("Cannot find patch file: $fPatch\n".
            "Please, double-check the patch's file name and try again.");
      }
    
      print "\nUsing patch file: $fPatch\n\n";
      
    } else {
      # Guess the patch file to use:      
      print "\nNo patch file especified, trying to determine the right one to use ...\n\n";
      
      print "Bugzilla version ".$Bugzilla::Config::VERSION." detected.\n";
      
      # This piece of code needs to be modified when a new patch file is added to the distribution:
    
      if ($Bugzilla::Config::VERSION =~ /^2\.18.*/) {
        # version 2.18.* detected
        $fPatch = "patch-2.18";
      } elsif ($Bugzilla::Config::VERSION =~ /^2\.19\.2$/) {
        # version 2.19.2 detected
        $fPatch = "patch-2.19.2";
      } elsif ($Bugzilla::Config::VERSION =~ /^2\.19\.3$/) {
        # version 2.19.3 detected
        $fPatch = "patch-2.19.3";
      } elsif ($Bugzilla::Config::VERSION =~ /^2\.20.*/) {
        # version 2.20.* detected
        $fPatch = "patch-2.20";
      } else {
        # no suitable version available
        DieWithStyle("No suitable patch detected for your Bugzilla. Patch cannot continue.\n".
                     "Still, there is a chance that a manual patch might work on your installation. Please, check out Testopia's installation manual for details.");
      }
      
      print "Patch file chosen: $fPatch\n\n";
      
      if (isWindows()) {
        $fPatch = "testopia\\".$fPatch;
      } else {
        $fPatch = "testopia/".$fPatch;
      }
    }
    
    
    # Read the files involved in the patch and put them in @files
    readPatchFiles($fPatch, \@files);
    
    # Now, let's go ahead and run the patch command:
    
    print "Now patching ...\n\n";

    my $patchPath = "patch";
    if (isWindows()) {
      $patchPath = "testopia\\tools\\patch.exe";
    }

    my $output;
        
    # Check out if patch is installed:
    $output = `$patchPath -v` || DieWithStyle("Patch failed. Is 'patch' installed and in your path?");

    # Patch doesn't really run yet: --dry-run
#        print "$patchPath -s --dry-run -bp 1 -i $fPatch 2>&1 \n";
#    exit;
    
    $output = `$patchPath -s -l --dry-run -bp 2 -i $fPatch 2>&1`;

    # If the output is empty, everything was perfect (the -s argument)
    chomp $output;
    if ($output) {

      # Nop... the patch didn't apply correctly:
          
      # Print out the output:
      print $output;

      DieWithStyle ("\n*** Patching of Bugzilla's files failed. None of your files has been modified.\n".
                 "\n".
                 "Some possible reasons:\n".
                 "\n".
                 "- Your Bugzilla installation has been modified from the original.\n".
                 "> Please scroll up and see if the patch command output you can see above gives you any clue. Consult Testopia's installation manual and get ready for some manual patching.\n".
                 "\n".
                 "- The patch file used was the wrong version.\n".
                 "> If you didn't especify the patch file to use, maybe this installation script picked up the wrong one. ".
                 "In this case, please, check out in the testopia directory the list of available patch files (files named as patch*) and run the install script especifying the file to use, according to the Bugzilla's version you are using. ".
                 "For instance, if you are using Bugzilla version 2.19.2, this is the command line you should use:\n".
                 "    tr_install.pl testopia/patch-2.19.2\n".
                 "> If you did in fact especify a patch file on the command line, please, double-check your Bugzilla version and the patch file you used and try again.\n".
                 "\n".
                 "- Testopia is not prepared to work with this particular version of Bugzilla.\n".
                 "> Please, check the documentation. Verify that this version of Bugzilla is supported.\n".
                 "\n".
                 "If you still have problems, please, check out Testopia's installation manual again and, if nothing helps, report the problem to Testopia sourceforge forum at http://sourceforge.net/projects/testopia.");
      
    }

    $rollbackPatchOnFail = 1;
        
    #Now let's really run the patch:
    $output = `$patchPath -s -l -z .orig -bp 2 -i $fPatch`;
    
    # If the output is empty, everything was perfect (the -s argument)
    chomp $output;
    if ($output) {
      
      # Nop... the patch didn't apply correctly:
            
      # Print out the output:
      print $output;
          
      DieWithStyle ("\n*** Unexpected condition. Patch failed, but I was pretty sure it should have worked.\n".
        "Try to repeat the installation.\n");
    }
    
    $rollbackPatchOnFail = 0;
        
    print "\nDone.\n";
        
    restorePermissions();
    
    open(MYOUTPUTFILE, ">$patchSuccessFile") or DieWithStyle("Couldn't write file: $patchSuccessFile");
    print MYOUTPUTFILE $VERSION;
    close(MYOUTPUTFILE);
      
    print "\n";
    print "Congratulations, patch worked flawless!\n";
    print "\n";
    print "A backup copy of the modified files has been saved with the .orig sufix.\n";
    
    1;
}

sub isWindows {

  return ($^O eq "MSWin32" || $^O eq "cygwin");
}

sub SetupPermissions {

  if (isWindows()) {
    SetupPermissions_windows();
  } else {
    SetupPermissions_unix();
  }
}

sub SetupPermissions_windows {

  print "Running on Windows or Cygwin... skipping permissions...\n";
}

sub SetupPermissions_unix {
  `chmod 750 tr_*.cgi`;
  `chmod 640 tr_*.pl`;
  `chmod 750 tr_install.pl`;
  `chmod 750 testopia`;
  `chmod 750 testopia/tr_*`;
  `chmod -R 750 testopia/doc`;
  `chmod -R 750 testopia/img`;
  `chmod -R 750 testopia/htmlarea30b`;
  `chmod -R 750 testopia/scripts`;
  `chmod 770 testopia/temp`;
  if (defined $webservergroup && ($webservergroup ne '')) {
    print "  Webserver group: $webservergroup\n";
    `chown :$webservergroup tr_*`;
    `chown :$webservergroup testopia`;
    `chown :$webservergroup testopia/tr_*`;
    `chown -R :$webservergroup testopia/doc`;
    `chown -R :$webservergroup testopia/img`;
    `chown -R :$webservergroup testopia/htmlarea30b`;
    `chown -R :$webservergroup testopia/scripts`;
    `chown :$webservergroup testopia/temp`;
  }
}

sub UpdateDB {

  my ($dbh) = (@_);
  
  my $testRunnerDBPresent = 0;
  
  my $qh;
  
  $qh = $dbh->prepare("SHOW TABLES LIKE 'test_cases'");
  $qh->execute;
  if ($qh->rows) {
    #assume that the BTR 0.4.5 db was created successfuly
    $testRunnerDBPresent = 1;
  }
  $qh->finish;
  
  if ($testRunnerDBPresent) {
    print "  Testopia database already present.\n";
    
    
  } else {
    print "  No Testopia database was detected. Need to create it ...\n";
    
    #Run the create db script:
    RunSQLScript('testopia/testopia.create.sql');
    print "  Done.\n";
  }
}

# Copied from checksetup.pl
#
## This subroutine checks if a group exist. If not, it will be automatically
## created with the next available groupid
##
#

sub tr_AddGroup {

  my ($dbh, $name, $desc) = @_;
  
  my ($id) = GroupDoesExist($dbh, $name);

  return $id if $id;

  print "Adding group $name ...\n";
  my $sth = $dbh->prepare('INSERT INTO groups
                          (name, description, userregexp, isbuggroup)
                          VALUES (?, ?, ?, ?)');
  $sth->execute($name, $desc, '', 0);
  $sth = $dbh->prepare("SELECT last_insert_id()");
  $sth->execute();
  ($id) = $sth->fetchrow_array();

  return $id;
}

sub GetAdminGroup {

  my ($dbh) = @_;
  
  my $sth = $dbh->prepare("SELECT id FROM groups WHERE name = 'admin'");
  $sth->execute();
  my ($adminid) = $sth->fetchrow_array();
  
  return $adminid;
}

sub tr_AssignAdminGrants {

  my ($dbh, $id, $adminid) = @_;

  #clean up first:
  $dbh->do("DELETE FROM group_group_map ".
           "WHERE member_id=$adminid AND grantor_id=$id");

  #Assign privileges to the admin group over this group:
  
  my $blessColumn;
  my $group_membership;
  my $group_bless;
  if ($Bugzilla::Config::VERSION =~ /^2\.18.*/) {
    # version 2.18.* detected
    $blessColumn = 'isbless';
    $group_membership = 0;
    $group_bless = 1;
  } else {
    $blessColumn = 'grant_type';
    $group_membership = GROUP_MEMBERSHIP;
    $group_bless = GROUP_BLESS;
  }
  
  # Admins can bless.
  $dbh->do("INSERT INTO group_group_map ".
           "(member_id, grantor_id, $blessColumn) ".
           "VALUES ($adminid, $id," . $group_bless . ")");
  # Admins can see.
  #$dbh->do("INSERT INTO group_group_map 
  #    (member_id, grantor_id, grant_type) 
  #    VALUES ($adminid, $last," . GROUP_VISIBLE . ")");
  # Admins are initially members.
  $dbh->do("INSERT INTO group_group_map ".
           "(member_id, grantor_id, $blessColumn) ".
           "VALUES ($adminid, $id," . $group_membership . ")");
}

sub GroupDoesExist {

  my ($dbh, $name) = @_;
  my $sth = $dbh->prepare("SELECT id FROM groups WHERE name='$name'");
  $sth->execute;
  my $id = 0;
  if ($sth->rows) {
    ($id) = $sth->fetchrow_array();
  }
  return $id;
}

sub GetDBConnectionInfo {

  my $my_db_base = 'mysql';
  # code taken from checksetup.pl:
  my $my_db_host = ${*{$main::{'db_host'}}{SCALAR}};
  my $my_db_port = ${*{$main::{'db_port'}}{SCALAR}};
  my $my_db_name = ${*{$main::{'db_name'}}{SCALAR}};
  my $my_db_user = ${*{$main::{'db_user'}}{SCALAR}};
  my $my_db_pass = ${*{$main::{'db_pass'}}{SCALAR}};
  my $my_db_sock = '';
  if (defined $main::{'db_sock'}) {
    $my_db_sock = ${*{$main::{'db_sock'}}{SCALAR}};
  }

  ($my_db_base, $my_db_host, $my_db_port, $my_db_name,
   $my_db_user, $my_db_pass, $my_db_sock);
}

sub DBConnect {

  my ($my_db_base, $my_db_host, $my_db_port, $my_db_name,
      $my_db_user, $my_db_pass, $my_db_sock) = GetDBConnectionInfo();

  my $dsn = "DBI:$my_db_base:$my_db_name:host=$my_db_host:port=$my_db_port";
  if ($my_db_sock ne '') {
    $dsn .= ";mysql_socket=$my_db_sock";
  }
  my $dbh = DBI->connect($dsn, $my_db_user, $my_db_pass)
    || DieWithStyle("Can't connect to the $my_db_base database. Is the database " .
           "installed and up and running?  Do you have the correct username " .
           "and password selected in\nlocalconfig?");
  ($dbh, $my_db_base, $my_db_host, $my_db_port, $my_db_name,
  $my_db_user, $my_db_sock);
}

sub DBDisconnect {

  my ($dbh) = (@_);
  $dbh->disconnect if $dbh;
}

sub RunSQLScript {

  my ($script) = (@_);

  my ($my_db_base, $my_db_host, $my_db_port, $my_db_name,
      $my_db_user, $my_db_pass, $my_db_sock) = GetDBConnectionInfo();

  my $cmdline = "mysql -h$my_db_host -P$my_db_port -u $my_db_user $my_db_name -p$my_db_pass";
  if ($my_db_sock ne '') {
    $cmdline.= " -S$my_db_sock";
  }
  $cmdline.= ' < '.$script;
  
  my $output;
  
  #Check out that mysql is installed and can be reached:
  $output = `mysql -V` || DieWithStyle("MySql command failed. Is 'mysql' installed and in your path?");

  $output = `$cmdline 2>&1`;
  print $output;
  # If the output is empty, everything was perfect
  chomp $output;
  if ($output) {
    DieWithStyle("Could not create Testopia's tables. See previous errors.");
  } else {
    print "    Testopia's tables created successfully.\n";
  }
}

sub DieWithStyle {

  my($message) = @_;
  
  print "\n$message\n";

  if ($rollbackPatchOnFail) {
    rollbackPatch();
  }
  
  die "\nInstall failed. See details above.\n";
}

sub readPatchFiles {

  my($path, $f) = (@_);
  
  open(MYINPUTFILE, "<$path") or DieWithStyle("Couldn't find file: $path");
  while (<MYINPUTFILE>) {
    my $line = $_;
    if($line =~ /^diff -[\w]* \.\/([^ ]*) .*$/) {
        push @$f, $1;
    }
  }
  close(MYINPUTFILE);
}

sub rollbackPatch {

  # Recoved files from the .orig copies made by patch:
  
  print "\n";
  print "Restoring original files ...\n";
  foreach my $file (@files) {
    if (-e $file.".orig") {
      rename($file.".orig", $file);
    } else {
       print "  Couldnt't restore file: $file because $file.orig doesn't exit!.\n";
    }
  }
  print "Done.\n";
}

sub restorePermissions {

  # Restore permissions from the .orig files:
  
  print "\n";
  print "Recovering original permissions ...\n";
  foreach my $file (@files) {
    perm_cp($file.".orig", $file);
  }
  print "Done.\n";
}

######################################
# perm_cp lifted from Ben Okopnik's
# http://www.linuxgazette.com/issue87/misc/tips/cpmod.pl.txt

sub perm_cp {

  my $perms = perm_get($_[0]);
  perm_set($_[1], $perms);
}

sub perm_get {

  my($filename) = @_;

  my @stats = (stat $filename)[2,4,5] or
    DieWithStyle("Cannot stat $filename ($!)");

  return \@stats;
}

sub perm_set {

  my($filename, $perms) = @_;

  chown($perms->[1], $perms->[2], $filename) or 
    DieWithStyle("Cannot chown $filename ($!)");

  chmod($perms->[0] & 07777, $filename) or
    DieWithStyle("Cannot chmod $filename ($!)");
}

######################################
