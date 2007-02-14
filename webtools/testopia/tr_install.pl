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
#                 Vance Baarda <vrb@novell.com>

use File::Copy;
use Bugzilla;
use Bugzilla::Constants;
use Bugzilla::DB;

my $VERSION = "1.1";

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

##############################################################################
print "Now installing Testopia ...\n\n";
do 'localconfig';
##############################################################################
# Patching.
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
##############################################################################
print "\nSetting file permissions ...\n";
SetupPermissions();
print "Done.\n\n";
##############################################################################
print "Checking Testopia database ...\n";
my $dbh = Bugzilla::DB::connect_main();
UpdateDB($dbh);
print "Done.\n\n";
##############################################################################
print "Checking Testopia group ...\n";
# Create group if needed and grant permissions to the admin group over it.
my $adminid = GetAdminGroup($dbh);
my $groupid;
if (!GroupExists($dbh, "Testers")) {
    $groupid = tr_AddGroup($dbh, 'Testers',
        'Can read, write, and delete all test plans, runs, and cases.', $adminid);
    tr_AssignAdminGrants($dbh, $groupid, $adminid);
}
updateACLs($dbh);
migrateAttachments($dbh);
print "Done.\n\n";
##############################################################################
print "Cleaning up Testopia cache ...\n";
unlink "data/plancache";
print "Done.\n\n";

print "Testopia installed successfully!\n";
##############################################################################
1;
##############################################################################

sub doPatch {
    my ($fPatch) = @_;

    if (-e $patchSuccessFile) {
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
      print "\nNo patch file specified, trying to determine the right one to use ...\n\n";

      print "Bugzilla version ".$Bugzilla::Config::VERSION." detected.\n";

      # This piece of code needs to be modified when a new patch file is added to the distribution:

      if ($Bugzilla::Config::VERSION =~ /^2\.20.*/) {
        # version 2.20.* detected
        $fPatch = "patch-2.20";
      } elsif ($Bugzilla::Config::VERSION =~ /^2\.22\.1$/) {
        # version 2.20.* detected
        $fPatch = "patch-2.22.1";
      } elsif ($Bugzilla::Config::VERSION =~ /^2\.22.*/) {
        # version 2.20.* detected
        $fPatch = "patch-2.22";
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

    my $patchPath = isWindows() ?
        "testopia\\tools\\patch.exe --binary" : "patch";
    my $no_patch_msg = isWindows() ?
        "Cannot find patch.exe. Please see testopia\\tools\\readme for " .
            "instructions.\n" :
        "Cannot find the patch command. Is it installed and in your PATH?\n";
    `$patchPath -v` || DieWithStyle($no_patch_msg);

    my $output = `$patchPath -s -l --dry-run -bp 2 -i $fPatch 2>&1`;

    # If the output is empty, everything was perfect (the -s argument)
    chomp $output;
    if ($output) {

      # Nope... the patch didn't apply correctly:

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

      # Nope... the patch didn't apply correctly:

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

    print "\nCongratulations, patch worked flawlessly!\n\n";
    print "A backup copy of the modified files has been saved with the .orig sufix.\n";
    print "\nBecause tr_install has patched some Bugzilla files, please run tr_install\n";
    print "again to finish the installation.\n";
    exit 0;
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
  `chmod -R 750 testopia/doc`;
  `chmod -R 750 testopia/img`;
  `chmod -R 750 testopia/css`;
  `chmod -R 750 testopia/js`;
  `chmod -R 750 testopia/dojo`;
  `chmod -R 750 testopia/scripts`;
  `chmod 770 testopia/temp`;
  if (defined $::webservergroup && ($::webservergroup ne '')) {
    `chown :$::webservergroup tr_*`;
    `chown :$::webservergroup testopia`;
    `chown -R :$::webservergroup testopia/doc`;
    `chown -R :$::webservergroup testopia/img`;
    `chown -R :$::webservergroup testopia/css`;
    `chown -R :$::webservergroup testopia/js`;
    `chown -R :$::webservergroup testopia/dojo`;
    `chown -R :$::webservergroup testopia/scripts`;
    `chown :$::webservergroup testopia/temp`;
  }
}

sub UpdateDB {
    my ($dbh) = (@_);

    # If the database contains Testopia tables but bz_schema doesn't
    # know about them, then we need to update bz_schema.
    if (grep(/^test_cases$/, $dbh->bz_table_list_real) and
            !$dbh->_bz_real_schema->get_table_abstract('test_cases')) {
        my $msg = "Sorry, we cannot upgrade from Testopia 1.0 using this " .
            "database. Upgrades are supported only with MySQL.";
        DieWithStyle($msg) unless $dbh->isa('Bugzilla::DB::Mysql');
        my $built_schema = $dbh->_bz_build_schema_from_disk;
        foreach my $table (grep(/^test_/, $built_schema->get_table_list())) {
            $dbh->_bz_real_schema->add_table($table,
                $built_schema->get_table_abstract($table));
        }
        $dbh->_bz_store_real_schema;
    }

    $dbh->bz_setup_database();

    $dbh->bz_drop_table('test_case_group_map');
    $dbh->bz_drop_table('test_category_templates');
    $dbh->bz_drop_table('test_plan_testers');
    $dbh->bz_drop_table('test_plan_group_map');
    $dbh->bz_drop_column('test_plans', 'editor_id');

    $dbh->bz_add_column('test_case_bugs', 'case_id', {TYPE => 'INT4', UNSIGNED => 1});
    $dbh->bz_add_column('test_case_runs', 'environment_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1}, 0);
    $dbh->bz_add_column('test_case_tags', 'userid', {TYPE => 'INT3', NOTNULL => 1}, 0);
    $dbh->bz_add_column('test_case_texts', 'setup', {TYPE => 'MEDIUMTEXT'});
    $dbh->bz_add_column('test_case_texts', 'breakdown', {TYPE => 'MEDIUMTEXT'});
    $dbh->bz_add_column('test_environments', 'product_id', {TYPE => 'INT2', NOTNULL => 1}, 0);
    $dbh->bz_add_column('test_environments', 'isactive', {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'}, 1);
    $dbh->bz_add_column('test_plan_tags', 'userid', {TYPE => 'INT3', NOTNULL => 1}, 0);
    $dbh->bz_add_column('test_runs', 'default_tester_id', {TYPE => 'INT3'});
    $dbh->bz_add_column('test_run_tags', 'userid', {TYPE => 'INT3', NOTNULL => 1}, 0);
    $dbh->bz_add_column('test_builds', 'isactive', {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '1'}, 1);
    $dbh->bz_add_column('test_cases', 'estimated_time', {TYPE => 'TIME'}, 0);
    $dbh->bz_add_column('test_case_runs', 'running_date', {TYPE => 'DATETIME'}, 0);
    
    $dbh->bz_alter_column('test_attachment_data', 'attachment_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_attachments', 'attachment_id', {TYPE => 'INTSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_attachments', 'case_id', {TYPE => 'INT4', UNSIGNED => 1});
    $dbh->bz_alter_column('test_attachments', 'creation_ts', {TYPE => 'DATETIME', NOTNULL => 1});
    $dbh->bz_alter_column('test_attachments', 'plan_id', {TYPE => 'INT4', UNSIGNED => 1});
    $dbh->bz_alter_column('test_builds', 'build_id', {TYPE => 'INTSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_activity', 'case_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_bugs', 'case_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_bugs', 'case_run_id', {TYPE => 'INT4', UNSIGNED => 1});
    $dbh->bz_alter_column('test_case_components', 'case_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_dependencies', 'blocked', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_dependencies', 'dependson', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_plans', 'case_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_plans', 'plan_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_runs', 'build_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_runs', 'case_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_runs', 'case_run_id', {TYPE => 'INTSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_runs', 'environment_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_runs', 'iscurrent', {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => '0'});
    $dbh->bz_alter_column('test_case_runs', 'run_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_run_status', 'case_run_status_id', {TYPE => 'TINYSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_cases', 'case_id', {TYPE => 'INTSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_status', 'case_status_id', {TYPE => 'TINYSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_tags', 'case_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_texts', 'case_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_case_texts', 'creation_ts', {TYPE => 'DATETIME', NOTNULL => 1});
    $dbh->bz_alter_column('test_environment_map', 'environment_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_environments', 'environment_id', {TYPE => 'INTSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_named_queries', 'isvisible', {TYPE => 'BOOLEAN', NOTNULL => 1, DEFAULT => 1});
    $dbh->bz_alter_column('test_plan_activity', 'plan_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_plans', 'plan_id', {TYPE => 'INTSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_plan_tags', 'plan_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_plan_texts', 'creation_ts', {TYPE => 'DATETIME', NOTNULL => 1});
    $dbh->bz_alter_column('test_plan_texts', 'plan_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_plan_texts', 'plan_text', {TYPE => 'MEDIUMTEXT'});
    $dbh->bz_alter_column('test_run_activity', 'run_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_run_cc', 'run_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_runs', 'build_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_runs', 'environment_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_runs', 'plan_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_runs', 'run_id', {TYPE => 'INTSERIAL', PRIMARYKEY => 1, NOTNULL => 1});
    $dbh->bz_alter_column('test_runs', 'start_date', {TYPE => 'DATETIME', NOTNULL => 1});
    $dbh->bz_alter_column('test_run_tags', 'run_id', {TYPE => 'INT4', UNSIGNED => 1, NOTNULL => 1});

    $dbh->bz_drop_index('test_attachments', 'AI_attachment_id');
    $dbh->bz_drop_index('test_attachments', 'attachment_id');
    $dbh->bz_drop_index('test_builds', 'build_id');
    $dbh->bz_drop_index('test_case_bugs', 'case_run_bug_id_idx');
    $dbh->bz_drop_index('test_case_bugs', 'case_run_id_idx');
    $dbh->bz_drop_index('test_case_categories', 'AI_category_id');
    $dbh->bz_drop_index('test_case_categories', 'category_name_idx');
    $dbh->bz_drop_index('test_case_categories', 'category_name_indx');
    $dbh->bz_drop_index('test_case_components', 'case_commponents_component_id_idx');
    $dbh->bz_drop_index('test_case_components', 'case_components_case_id_idx');
    $dbh->bz_drop_index('test_case_components', 'case_components_component_id_idx');
    $dbh->bz_drop_index('test_case_plans', 'case_plans_case_id_idx');
    $dbh->bz_drop_index('test_case_plans', 'case_plans_plan_id_idx');
    $dbh->bz_drop_index('test_case_runs', 'AI_case_run_id');
    $dbh->bz_drop_index('test_case_runs', 'case_run_build_idx');
    $dbh->bz_drop_index('test_case_runs', 'case_run_env_idx');
    $dbh->bz_drop_index('test_case_runs', 'case_run_id');
    $dbh->bz_drop_index('test_case_runs', 'case_run_id_2');
    $dbh->bz_drop_index('test_case_runs', 'case_run_run_id_idx');
    $dbh->bz_drop_index('test_case_runs', 'case_run_shortkey_idx');
    $dbh->bz_drop_index('test_case_runs', 'case_run_sortkey_idx');
    $dbh->bz_drop_index('test_case_run_status', 'AI_case_run_status_id');
    $dbh->bz_drop_index('test_case_run_status', 'case_run_status_name_idx');
    $dbh->bz_drop_index('test_case_run_status', 'case_run_status_sortkey_idx');
    $dbh->bz_drop_index('test_case_run_status', 'sortkey');
    $dbh->bz_drop_index('test_cases', 'AI_case_id');
    $dbh->bz_drop_index('test_cases', 'alias');
    $dbh->bz_drop_index('test_cases', 'case_id');
    $dbh->bz_drop_index('test_cases', 'case_id_2');
    $dbh->bz_drop_index('test_case_status', 'AI_case_status_id');
    $dbh->bz_drop_index('test_case_status', 'case_status_id');
    $dbh->bz_drop_index('test_case_status', 'test_case_status_name_idx');
    $dbh->bz_drop_index('test_cases', 'test_case_requirment_idx');
    $dbh->bz_drop_index('test_case_tags', 'case_tags_case_id_idx');
    $dbh->bz_drop_index('test_case_tags', 'case_tags_case_id_idx_v2');
    $dbh->bz_drop_index('test_case_tags', 'case_tags_tag_id_idx');
    $dbh->bz_drop_index('test_case_tags', 'case_tags_user_idx');
    $dbh->bz_drop_index('test_email_settings', 'test_event_user_event_dx');
    $dbh->bz_drop_index('test_email_settings', 'test_event_user_event_idx');
    $dbh->bz_drop_index('test_environments', 'environment_id');
    $dbh->bz_drop_index('test_environments', 'environment_name_idx');
    $dbh->bz_drop_index('test_fielddefs', 'AI_fieldid');
    $dbh->bz_drop_index('test_fielddefs', 'fielddefs_name_idx') if $dbh->isa('Bugzilla::DB::Mysql');
    $dbh->bz_drop_index('test_fielddefs', 'test_fielddefs_name_idx');
    $dbh->bz_drop_index('test_plans', 'AI_plan_id');
    $dbh->bz_drop_index('test_plans', 'plan_id');
    $dbh->bz_drop_index('test_plans', 'plan_id_2');
    $dbh->bz_drop_index('test_plan_tags', 'plan_tags_idx');
    $dbh->bz_drop_index('test_plan_tags', 'plan_tags_user_idx');
    $dbh->bz_drop_index('test_plan_types', 'AI_type_id');
    $dbh->bz_drop_index('test_plan_types', 'plan_type_name_idx');
    $dbh->bz_drop_index('test_run_cc', 'run_cc_run_id_who_idx');
    $dbh->bz_drop_index('test_runs', 'AI_run_id');
    $dbh->bz_drop_index('test_runs', 'run_id');
    $dbh->bz_drop_index('test_runs', 'run_id_2');
    $dbh->bz_drop_index('test_runs', 'test_run_plan_id_run_id__idx');
    $dbh->bz_drop_index('test_run_tags', 'run_tags_idx');
    $dbh->bz_drop_index('test_run_tags', 'run_tags_user_idx');
    $dbh->bz_drop_index('test_tags', 'AI_tag_id');
    $dbh->bz_drop_index('test_tags', 'tag_name');
    $dbh->bz_drop_index('test_tags', 'test_tag_name_idx');
    $dbh->bz_drop_index('test_tags', 'test_tag_name_indx');

    $dbh->bz_add_index('test_builds', 'build_milestone_idx', ['milestone']);
    $dbh->bz_add_index('test_builds', 'build_name_idx', ['name']);
    $dbh->bz_add_index('test_builds', 'build_prod_idx', {FIELDS => [qw(build_id product_id)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_builds', 'build_product_id_name_idx', {FIELDS => [qw(product_id name)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_case_bugs', 'case_bugs_bug_id_idx', ['bug_id']);
    $dbh->bz_add_index('test_case_bugs', 'case_bugs_case_id_idx', ['case_id']);
    $dbh->bz_add_index('test_case_bugs', 'case_bugs_case_run_id_idx', ['case_run_id']);
    $dbh->bz_add_index('test_case_categories', 'category_name_idx_v2', ['name']);
    $dbh->bz_add_index('test_case_categories', 'category_product_id_name_idx', {FIELDS => [qw(product_id name)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_case_categories', 'category_product_idx', {FIELDS => [qw(category_id product_id)], TYPE => 'UNIQUE'} );
    $dbh->bz_add_index('test_case_components', 'components_case_id_idx', {FIELDS => [qw(case_id component_id)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_case_components', 'components_component_id_idx', ['component_id']);
    $dbh->bz_add_index('test_case_dependencies', 'case_dependencies_blocked_idx', ['blocked']);
    $dbh->bz_add_index('test_case_dependencies', 'case_dependencies_primary_idx', {FIELDS => [qw(dependson blocked)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_case_plans', 'test_case_plans_case_idx', [qw(case_id)]);
    $dbh->bz_add_index('test_case_plans', 'test_case_plans_primary_idx', {FIELDS => [qw(plan_id case_id)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_case_runs', 'case_run_build_env_idx', {FIELDS => [qw(run_id case_id build_id environment_id)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_cases', 'test_case_requirement_idx', ['requirement']);
    $dbh->bz_add_index('test_case_tags', 'case_tags_case_id_idx_v3', [qw(case_id)]);
    $dbh->bz_add_index('test_case_tags', 'case_tags_primary_idx', {FIELDS => [qw(tag_id case_id userid)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_case_tags', 'case_tags_secondary_idx', {FIELDS => [qw(tag_id case_id)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_case_tags', 'case_tags_userid_idx', [qw(userid)]);
    $dbh->bz_add_index('test_environments', 'environment_name_idx_v2', ['name']);
    $dbh->bz_add_index('test_plan_tags', 'plan_tags_plan_id_idx', [qw(plan_id)]);
    $dbh->bz_add_index('test_plan_tags', 'plan_tags_primary_idx', {FIELDS => [qw(tag_id plan_id userid)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_plan_tags', 'plan_tags_secondary_idx', {FIELDS => [qw(tag_id plan_id)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_plan_tags', 'plan_tags_userid_idx', [qw(userid)]);
    $dbh->bz_add_index('test_run_cc', 'test_run_cc_primary_idx', {FIELDS => [qw(run_id who)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_run_cc', 'test_run_cc_who_idx', [qw(who)]);
    $dbh->bz_add_index('test_runs', 'test_run_plan_id_run_id_idx', [qw(plan_id run_id)]);
    $dbh->bz_add_index('test_runs', 'test_runs_summary_idx', {FIELDS => ['summary'], TYPE => 'FULLTEXT'});
    $dbh->bz_add_index('test_run_tags', 'run_tags_primary_idx', {FIELDS => [qw(tag_id run_id userid)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_run_tags', 'run_tags_run_id_idx', [qw(run_id)]);
    $dbh->bz_add_index('test_run_tags', 'run_tags_secondary_idx', {FIELDS => [qw(tag_id run_id)], TYPE => 'UNIQUE'});
    $dbh->bz_add_index('test_run_tags', 'run_tags_userid_idx', [qw(userid)]);
    $dbh->bz_add_index('test_tags', 'test_tag_name_idx_v2', [qw(tag_name)]);

    populateMiscTables($dbh);
    populateEnvTables($dbh);
    migrateEnvData($dbh);
}

sub updateACLs {
    my $dbh = shift;
    return unless $dbh->selectrow_array("SELECT COUNT(*) FROM test_plan_permissions") == 0;

    print "Populating plan ACLs ...\n"; 
    my $ref = $dbh->selectall_arrayref("SELECT plan_id, author_id FROM test_plans", {'Slice' =>{}});
    foreach my $plan (@$ref){
        my ($finished) = $dbh->selectrow_array(
            "SELECT COUNT(*) FROM test_plan_permissions 
              WHERE plan_id = ? AND userid = ?", 
              undef, ($plan->{'plan_id'}, $plan->{'author_id'}));
        next if ($finished);
        $dbh->do("INSERT INTO test_plan_permissions(userid, plan_id, permissions) 
                  VALUES(?,?,?)",
                  undef, ($plan->{'author_id'}, $plan->{'plan_id'}, 15));
    }
}

sub migrateAttachments {
    my $dbh = shift;
    return unless grep /case_id/, @{$dbh->selectcol_arrayref("DESCRIBE test_attachments")};
    print "Migrating attachments...\n";
    
    my $rows = $dbh->selectall_arrayref(
        "SELECT attachment_id, case_id, plan_id 
           FROM test_attachments", {'Slice' => {}});
    
    foreach my $row (@$rows){
        if ($row->{'case_id'}){
            $dbh->do("INSERT INTO test_case_attachments (attachment_id, case_id)
                      VALUES (?,?)", undef, ($row->{'attachment_id'}, $row->{'case_id'}));
        }
        elsif ($row->{'plan_id'}){
            $dbh->do("INSERT INTO test_plan_attachments (attachment_id, plan_id)
                      VALUES (?,?)", undef, ($row->{'attachment_id'}, $row->{'plan_id'}));
        }
    }
    $dbh->bz_drop_column('test_attachments', 'case_id');
    $dbh->bz_drop_column('test_attachments', 'plan_id');
}

sub populateMiscTables {
    my ($dbh) = (@_);

    if ($dbh->selectrow_array("SELECT COUNT(*) FROM test_fielddefs " .
            "WHERE table_name = 'test_cases' " .
            "AND name = 'estimated_time'") == 0) {
        $dbh->do("INSERT INTO test_fielddefs " .
                "(fieldid, name, description, table_name) " .
                "VALUES (24, 'estimated_time', 'Estimated Time', 'test_cases')");
    }

    # Insert initial values in static tables. Going out on a limb and
    # assuming that if one table is empty, they all are.
    return unless $dbh->selectrow_array("SELECT COUNT(*) FROM test_case_run_status") == 0;

    print "Populating test_case_run_status table ...\n";
    print "Populating test_case_status table ...\n";
    print "Populating test_plan_types table ...\n";
    print "Populating test_fielddefs table ...\n";
    open FH, "< testopia/testopia.insert.sql" or die;
    $dbh->do($_) while (<FH>);
    close FH;
}

sub populateEnvTables {
    my ($dbh) = (@_);
    my $sth;
    my $ary_ref;
    my $value;

    return unless $dbh->selectrow_array("SELECT COUNT(*) FROM test_environment_category") == 0;
    if ($dbh->selectrow_array("SELECT COUNT(*) FROM test_environment_element") != 0) {
        print STDERR "\npopulateEnv: Fatal Error: test_environment_category " .
            "is empty but\ntest_environment_element is not. This ought " .
            "to be impossible.\n\n";
        return;
    }

    $dbh->bz_lock_tables(
        'test_environment_category WRITE',
        'test_environment_element WRITE',
        'op_sys READ',
        'rep_platform READ');

    print "Populating test_environment_category table ...\n";
    $dbh->do("INSERT INTO test_environment_category " .
        "(env_category_id, product_id, name) " .
        "VALUES (1, 0, 'Operating System')");
    $dbh->do("INSERT INTO test_environment_category " .
        "(env_category_id, product_id, name) " .
        "VALUES (2, 0, 'Hardware')");

    print "Populating test_environment_element table ...\n";
    $sth = $dbh->prepare("INSERT INTO test_environment_element " .
        "(env_category_id, name, parent_id, isprivate) " .
        "VALUES (?, ?, ?, ?)");
    $ary_ref = $dbh->selectcol_arrayref("SELECT value FROM op_sys");
    foreach $value (@$ary_ref) {
        $sth->execute(1, $value, 0, 0);
    }
    $ary_ref = $dbh->selectcol_arrayref("SELECT value FROM rep_platform");
    foreach $value (@$ary_ref) {
        $sth->execute(2, $value, 0, 0);
    }

    $dbh->bz_unlock_tables();
}

sub migrateEnvData {
    my ($dbh) = (@_);
    my $os_mapping;
    my $platform_mapping;
    my $ary_ref;
    my $i;

    return unless $dbh->bz_column_info('test_environments', 'op_sys_id');

    # Map between IDs in op_sys table and IDs in
    # test_environment_element table.
    $os_mapping = $dbh->selectall_hashref("SELECT " .
        "os.id AS op_sys_id, " .
        "env_elem.element_id AS element_id " .
        "FROM op_sys os, test_environment_element env_elem " .
        "WHERE os.value = env_elem.name " .
        "AND env_elem.env_category_id = 1",
        'op_sys_id');

    # Map between IDs in rep_platform table and IDs in
    # test_environment_element table.
    $platform_mapping = $dbh->selectall_hashref("SELECT " .
        "platform.id AS rep_platform_id, " .
        "env_elem.element_id AS element_id " .
        "FROM rep_platform platform, test_environment_element env_elem " .
        "WHERE platform.value = env_elem.name " .
        "AND env_elem.env_category_id = 2",
        'rep_platform_id');

    $dbh->bz_lock_tables(
        'test_environment_map WRITE',
        'test_environments READ');
    print "Migrating data from test_environments to test_environment_map ...\n";
    $sth = $dbh->prepare("INSERT INTO test_environment_map " .
        "(environment_id, property_id, element_id, value_selected) " .
        "VALUES (?, ?, ?, ?)");
    $ary_ref = $dbh->selectall_arrayref("SELECT environment_id, op_sys_id " .
        "FROM test_environments");
    foreach $i (@$ary_ref) {
        $sth->execute(@$i[0], 0, $os_mapping->{@$i[1]}->{'element_id'}, '');
    }
    $ary_ref = $dbh->selectall_arrayref("SELECT environment_id, rep_platform_id " .
        "FROM test_environments");
    foreach $i (@$ary_ref) {
        $sth->execute(@$i[0], 0, $platform_mapping->{@$i[1]}->{'element_id'}, '');
    }
    $dbh->bz_unlock_tables();

    print "Saving data from test_environments.xml column into text files ...\n";
    $ary_ref = $dbh->selectall_arrayref("SELECT environment_id, name, xml " .
        "FROM test_environments WHERE xml != ''");
    foreach $value (@$ary_ref) {
        open(FH, ">environment_" . @$value[0] . "_xml.txt");
        print FH "environment ID: @$value[0]\n";
        print FH "environment name: @$value[1]\n";
        print FH "environment xml:\n@$value[2]\n";
        close(FH);
    }

    $dbh->bz_drop_column('test_environments', 'op_sys_id');
    $dbh->bz_drop_column('test_environments', 'rep_platform_id');
    $dbh->bz_drop_column('test_environments', 'xml');
}

# Copied from checksetup.pl
#
## This subroutine checks if a group exist. If not, it will be automatically
## created with the next available groupid
##
#

sub tr_AddGroup {
    my ($dbh, $name, $desc) = @_;

    print "Adding group $name ...\n";
    my $sth = $dbh->prepare("INSERT INTO groups " .
        "(name, description, userregexp, isbuggroup, last_changed) " .
        "VALUES (?, ?, ?, ?, NOW())");
    $sth->execute($name, $desc, "", 0);
    return $dbh->bz_last_key("groups", "id");
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
  # Admins are initially members.
  $dbh->do("INSERT INTO group_group_map ".
           "(member_id, grantor_id, $blessColumn) ".
           "VALUES ($adminid, $id," . $group_membership . ")");
}

sub GroupExists {
  my ($dbh, $name) = @_;
  return $dbh->selectrow_array("SELECT COUNT(*) FROM groups WHERE name = ?",
      undef, $name);
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
  # Recovered files from the .orig copies made by patch:

  print "\n";
  print "Restoring original files ...\n";
  foreach my $file (@files) {
    if (-e $file.".orig") {
      rename($file.".orig", $file);
    } else {
       print "  Couldn't restore file: $file because $file.orig doesn't exist!.\n";
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
