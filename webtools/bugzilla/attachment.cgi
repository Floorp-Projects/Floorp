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
#                 Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use diagnostics;
use strict;

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

# Establish a connection to the database backend.
ConnectToDatabase();

# Use the template toolkit (http://www.template-toolkit.org/) to generate
# the user interface (HTML pages and mail messages) using templates in the
# "template/" subdirectory.
use Template;

# Create the global template object that processes templates and specify
# configuration parameters that apply to all templates processed in this script.
my $template = Template->new(
  {
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => "template/default" ,
    # Allow templates to be specified with relative paths.
    RELATIVE => 1 
  }
);

# Define the global variables and functions that will be passed to the UI 
# template.  Individual functions add their own values to this hash before
# sending them to the templates they process.
my $vars = 
  {
    # Function for retrieving global parameters.
    'Param' => \&Param , 

    # Function for processing global parameters that contain references
    # to other global parameters.
    'PerformSubsts' => \&PerformSubsts
  };

# Check whether or not the user is logged in and, if so, set the $::userid 
# and $::usergroupset variables.
quietly_check_login();

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value of
# that variable and runs the appropriate code.

# Determine whether to use the action specified by the user or the default.
my $action = $::FORM{'action'} || 'view';

if ($action eq "view")  
{ 
  validateID();
  view(); 
}
elsif ($action eq "viewall") 
{ 
  ValidateBugID($::FORM{'bugid'});
  viewall(); 
}
elsif ($action eq "enter") 
{ 
  confirm_login();
  ValidateBugID($::FORM{'bugid'});
  enter(); 
}
elsif ($action eq "insert")
{
  confirm_login();
  ValidateBugID($::FORM{'bugid'});
  validateFilename();
  validateData();
  validateDescription();
  validateIsPatch();
  validateContentType() unless $::FORM{'ispatch'};
  validateObsolete() if $::FORM{'obsolete'};
  insert();
}
elsif ($action eq "edit") 
{ 
  validateID();
  edit(); 
}
elsif ($action eq "update") 
{ 
  confirm_login();
  UserInGroup("editbugs")
    || DisplayError("You are not authorized to edit attachments.")
    && exit;
  validateID();
  validateDescription();
  validateIsPatch();
  validateContentType() unless $::FORM{'ispatch'};
  validateIsObsolete();
  validateStatuses();
  update();
}
else 
{ 
  DisplayError("I could not figure out what you wanted to do.")
}

exit;

################################################################################
# Data Validation / Security Authorization
################################################################################

sub validateID
{
  # Validate the value of the "id" form field, which must contain a positive
  # integer that is the ID of an existing attachment.

  $::FORM{'id'} =~ /^[1-9][0-9]*$/
    || DisplayError("You did not enter a valid attachment number.") 
      && exit;
  
  # Make sure the attachment exists in the database.
  SendSQL("SELECT bug_id FROM attachments WHERE attach_id = $::FORM{'id'}");
  MoreSQLData()
    || DisplayError("Attachment #$::FORM{'id'} does not exist.") 
    && exit;

  # Make sure the user is authorized to access this attachment's bug.
  my ($bugid) = FetchSQLData();
  ValidateBugID($bugid);
}

sub validateDescription
{
  $::FORM{'description'}
    || DisplayError("You must enter a description for the attachment.")
      && exit;
}

sub validateIsPatch
{
  # Set the ispatch flag to zero if it is undefined, since the UI uses
  # an HTML checkbox to represent this flag, and unchecked HTML checkboxes
  # do not get sent in HTML requests.
  $::FORM{'ispatch'} = $::FORM{'ispatch'} ? 1 : 0;

  # Set the content type to text/plain if the attachment is a patch.
  $::FORM{'contenttype'} = "text/plain" if $::FORM{'ispatch'};
}

sub validateContentType
{
  if (!$::FORM{'contenttypemethod'})
  {
    DisplayError("You must choose a method for determining the content type,
      either <em>auto-detect</em>, <em>select from list</em>, or <em>enter 
      manually</em>.");
    exit;
  }
  elsif ($::FORM{'contenttypemethod'} eq 'autodetect')
  {
    # The user asked us to auto-detect the content type, so use the type
    # specified in the HTTP request headers.
    if ( !$::FILE{'data'}->{'contenttype'} )
    {
      DisplayError("You asked Bugzilla to auto-detect the content type, but
        your browser did not specify a content type when uploading the file, 
        so you must enter a content type manually.");
      exit;
    }
    $::FORM{'contenttype'} = $::FILE{'data'}->{'contenttype'};
  }
  elsif ($::FORM{'contenttypemethod'} eq 'list')
  {
    # The user selected a content type from the list, so use their selection.
    $::FORM{'contenttype'} = $::FORM{'contenttypeselection'};
  }
  elsif ($::FORM{'contenttypemethod'} eq 'manual')
  {
    # The user entered a content type manually, so use their entry.
    $::FORM{'contenttype'} = $::FORM{'contenttypeentry'};
  }
  else
  {
    my $htmlcontenttypemethod = html_quote($::FORM{'contenttypemethod'});
    DisplayError("Your form submission got corrupted somehow.  The <em>content
      method</em> field, which specifies how the content type gets determined,
      should have been either <em>autodetect</em>, <em>list</em>, 
      or <em>manual</em>, but was instead <em>$htmlcontenttypemethod</em>.");
    exit;
  }

  if ( $::FORM{'contenttype'} !~ /^(application|audio|image|message|model|multipart|text|video)\/.+$/ )
  {
    my $htmlcontenttype = html_quote($::FORM{'contenttype'});
    DisplayError("The content type <em>$htmlcontenttype</em> is invalid.
      Valid types must be of the form <em>foo/bar</em> where <em>foo</em> 
      is either <em>application, audio, image, message, model, multipart, 
      text,</em> or <em>video</em>.");
    exit;
  }
}

sub validateIsObsolete
{
  # Set the isobsolete flag to zero if it is undefined, since the UI uses
  # an HTML checkbox to represent this flag, and unchecked HTML checkboxes
  # do not get sent in HTML requests.
  $::FORM{'isobsolete'} = $::FORM{'isobsolete'} ? 1 : 0;
}

sub validateStatuses
{
  # Get a list of attachment statuses that are valid for this attachment.
  PushGlobalSQLState();
  SendSQL("SELECT  attachstatusdefs.id
           FROM    attachments, bugs, attachstatusdefs
           WHERE   attachments.attach_id = $::FORM{'id'}
           AND     attachments.bug_id = bugs.bug_id
           AND     attachstatusdefs.product = bugs.product");
  my @statusdefs;
  push(@statusdefs, FetchSQLData()) while MoreSQLData();
  PopGlobalSQLState();
  
  foreach my $status (@{$::MFORM{'status'}})
  {
    grep($_ == $status, @statusdefs)
      || DisplayError("One of the statuses you entered is not a valid status
                       for this attachment.")
        && exit;
  }
}

sub validateData
{
  $::FORM{'data'}
    || DisplayError("The file you are trying to attach is empty!")
      && exit;

  my $len = length($::FORM{'data'});

  my $maxpatchsize = Param('maxpatchsize');
  my $maxattachmentsize = Param('maxattachmentsize');
  
  # Makes sure the attachment does not exceed either the "maxpatchsize" or 
  # the "maxattachmentsize" parameter.
  if ( $::FORM{'ispatch'} && $maxpatchsize && $len > $maxpatchsize*1024 )
  {
    my $lenkb = sprintf("%.0f", $len/1024);
    DisplayError("The file you are trying to attach is ${lenkb} kilobytes (KB) in size.  
                  Patches cannot be more than ${maxpatchsize}KB in size.
                  Try breaking your patch into several pieces.");
    exit;
  } elsif ( !$::FORM{'ispatch'} && $maxattachmentsize && $len > $maxattachmentsize*1024 ) {
    my $lenkb = sprintf("%.0f", $len/1024);
    DisplayError("The file you are trying to attach is ${lenkb} kilobytes (KB) in size.  
                  Non-patch attachments cannot be more than ${maxattachmentsize}KB.
                  If your attachment is an image, try converting it to a compressable
                  format like JPG or PNG, or put it elsewhere on the web and
                  link to it from the bug's URL field or in a comment on the bug.");
    exit;
  }
}

sub validateFilename
{
  defined $::FILE{'data'}
    || DisplayError("You did not specify a file to attach.")
      && exit;
}

sub validateObsolete
{
  # When a user creates an attachment, they can request that one or more
  # existing attachments be made obsolete.  This function makes sure they
  # are authorized to make changes to attachments and that the IDs of the
  # attachments they selected for obsoletion are all valid.
  UserInGroup("editbugs")
    || DisplayError("You must be authorized to make changes to attachments 
         to make attachments obsolete when creating a new attachment.")
      && exit;

  # Make sure the attachment id is valid and the user has permissions to view
  # the bug to which it is attached.
  foreach my $attachid (@{$::MFORM{'obsolete'}}) {
    $attachid =~ /^[1-9][0-9]*$/
      || DisplayError("The attachment number of one of the attachments 
           you wanted to obsolete is invalid.") 
        && exit;
  
    SendSQL("SELECT bug_id, isobsolete, description 
             FROM attachments WHERE attach_id = $attachid");

    # Make sure the attachment exists in the database.
    MoreSQLData()
      || DisplayError("Attachment #$attachid does not exist.") 
        && exit;

    my ($bugid, $isobsolete, $description) = FetchSQLData();

    # Make sure the user is authorized to access this attachment's bug.
    ValidateBugID($bugid);

    if ($bugid != $::FORM{'bugid'})
    {
      $description = html_quote($description);
      DisplayError("Attachment #$attachid ($description) is attached 
        to bug #$bugid, but you tried to flag it as obsolete while
        creating a new attachment to bug #$::FORM{'bugid'}.");
      exit;
    }

    if ( $isobsolete )
    {
      $description = html_quote($description);
      DisplayError("Attachment #$attachid ($description) is already obsolete.");
      exit;
    }
  }

}

################################################################################
# Functions
################################################################################

sub view
{
  # Display an attachment.

  # Retrieve the attachment content and its content type from the database.
  SendSQL("SELECT mimetype, thedata FROM attachments WHERE attach_id = $::FORM{'id'}");
  my ($contenttype, $thedata) = FetchSQLData();
    
  # Return the appropriate HTTP response headers.
  print "Content-Type: $contenttype\n\n";

  print $thedata;
}


sub viewall
{
  # Display all attachments for a given bug in a series of IFRAMEs within one HTML page.

  # Retrieve the attachments from the database and write them into an array
  # of hashes where each hash represents one attachment.
  SendSQL("SELECT attach_id, creation_ts, mimetype, description, ispatch, isobsolete 
           FROM attachments WHERE bug_id = $::FORM{'bugid'} ORDER BY attach_id");
  my @attachments; # the attachments array
  while (MoreSQLData())
  {
    my %a; # the attachment hash
    ($a{'attachid'}, $a{'date'}, $a{'contenttype'}, 
     $a{'description'}, $a{'ispatch'}, $a{'isobsolete'}) = FetchSQLData();

    # Format the attachment's creation/modification date into something readable.
    if ($a{'date'} =~ /^(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)(\d\d)$/) {
        $a{'date'} = "$3/$4/$2&nbsp;$5:$6";
    }

    # Flag attachments as to whether or not they can be viewed (as opposed to
    # being downloaded).  Currently I decide they are viewable if their MIME type 
    # is either text/*, image/*, or application/vnd.mozilla.*.
    # !!! Yuck, what an ugly hack.  Fix it!
    $a{'isviewable'} = ( $a{'contenttype'} =~ /^(text|image|application\/vnd\.mozilla\.)/ );

    # Retrieve a list of status flags that have been set on the attachment.
    PushGlobalSQLState();
    SendSQL("SELECT    name 
             FROM      attachstatuses, attachstatusdefs 
             WHERE     attach_id = $a{'attachid'} 
             AND       attachstatuses.statusid = attachstatusdefs.id
             ORDER BY  sortkey");
    my @statuses;
    push(@statuses, FetchSQLData()) while MoreSQLData();
    $a{'statuses'} = \@statuses;
    PopGlobalSQLState();

    # Add the hash representing the attachment to the array of attachments.
    push @attachments, \%a;
  }

  # Retrieve the bug summary for displaying on screen.
  SendSQL("SELECT short_desc FROM bugs WHERE bug_id = $::FORM{'bugid'}");
  my ($bugsummary) = FetchSQLData();

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'bugid'} = $::FORM{'bugid'};
  $vars->{'bugsummary'} = $bugsummary;
  $vars->{'attachments'} = \@attachments;

  # Return the appropriate HTTP response headers.
  print "Content-Type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/viewall.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;
}


sub enter
{
  # Display a form for entering a new attachment.

  # Retrieve the attachments from the database and write them into an array
  # of hashes where each hash represents one attachment.
  SendSQL("SELECT attach_id, description 
           FROM attachments
           WHERE bug_id = $::FORM{'bugid'}
           AND isobsolete = 0
           ORDER BY attach_id");
  my @attachments; # the attachments array
  while ( MoreSQLData() ) {
    my %a; # the attachment hash
    ($a{'id'}, $a{'description'}) = FetchSQLData();

    # Add the hash representing the attachment to the array of attachments.
    push @attachments, \%a;
  }

  # Retrieve the bug summary for displaying on screen.
  SendSQL("SELECT short_desc FROM bugs WHERE bug_id = $::FORM{'bugid'}");
  my ($bugsummary) = FetchSQLData();

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'bugid'} = $::FORM{'bugid'};
  $vars->{'bugsummary'} = $bugsummary;
  $vars->{'attachments'} = \@attachments;

  # Return the appropriate HTTP response headers.
  print "Content-Type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/enter.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;
}


sub insert
{
  # Insert a new attachment into the database.

  # Escape characters in strings that will be used in SQL statements.
  my $filename = SqlQuote($::FILE{'data'}->{'filename'});
  my $description = SqlQuote($::FORM{'description'});
  my $contenttype = SqlQuote($::FORM{'contenttype'});
  my $thedata = SqlQuote($::FORM{'data'});

  # Insert the attachment into the database.
  SendSQL("INSERT INTO attachments (bug_id, filename, description, mimetype, ispatch, submitter_id, thedata) 
           VALUES ($::FORM{'bugid'}, $filename, $description, $contenttype, $::FORM{'ispatch'}, $::userid, $thedata)");

  # Retrieve the ID of the newly created attachment record.
  SendSQL("SELECT LAST_INSERT_ID()");
  my $attachid = FetchOneColumn();

  # Insert a comment about the new attachment into the database.
  my $comment = "Created an attachment (id=$attachid): $::FORM{'description'}\n";
  $comment .= ("\n" . $::FORM{'comment'}) if $::FORM{'comment'};

  use Text::Wrap;
  $Text::Wrap::columns = 80;
  $Text::Wrap::huge = 'overflow';
  $comment = Text::Wrap::wrap('', '', $comment);

  AppendComment($::FORM{'bugid'}, 
                $::COOKIE{"Bugzilla_login"},
                $comment);

  # Make existing attachments obsolete.
  my $fieldid = GetFieldID('attachments.isobsolete');
  foreach my $attachid (@{$::MFORM{'obsolete'}}) {
    SendSQL("UPDATE attachments SET isobsolete = 1 WHERE attach_id = $attachid");
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($::FORM{'bugid'}, $attachid, $::userid, NOW(), $fieldid, '0', '1')");
  }

  # Send mail to let people know the attachment has been created.  Uses a 
  # special syntax of the "open" and "exec" commands to capture the output of 
  # "processmail", which "system" doesn't allow, without running the command 
  # through a shell, which backticks (``) do.
  #system ("./processmail", $bugid , $::userid);
  #my $mailresults = `./processmail $bugid $::userid`;
  my $mailresults = '';
  open(PMAIL, "-|") or exec('./processmail', $::FORM{'bugid'}, $::COOKIE{'Bugzilla_login'});
  $mailresults .= $_ while <PMAIL>;
  close(PMAIL);
 
  # Define the variables and functions that will be passed to the UI template.
  $vars->{'bugid'} = $::FORM{'bugid'};
  $vars->{'attachid'} = $attachid;
  $vars->{'description'} = $description;
  $vars->{'mailresults'} = $mailresults;
  $vars->{'contenttypemethod'} = $::FORM{'contenttypemethod'};
  $vars->{'contenttype'} = $::FORM{'contenttype'};

  # Return the appropriate HTTP response headers.
  print "Content-Type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/created.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;
}


sub edit
{
  # Edit an attachment record.  Users with "editbugs" privileges can edit the 
  # attachment's description, content type, ispatch and isobsolete flags, and 
  # statuses, and they can also submit a comment that appears in the bug.  
  # Users cannot edit the content of the attachment itself.

  # Retrieve the attachment from the database.
  SendSQL("SELECT description, mimetype, bug_id, ispatch, isobsolete 
           FROM attachments WHERE attach_id = $::FORM{'id'}");
  my ($description, $contenttype, $bugid, $ispatch, $isobsolete) = FetchSQLData();

  # Flag attachment as to whether or not it can be viewed (as opposed to
  # being downloaded).  Currently I decide it is viewable if its content
  # type is either text/.* or application/vnd.mozilla.*.
  # !!! Yuck, what an ugly hack.  Fix it!
  my $isviewable = ( $contenttype =~ /^(text|image|application\/vnd\.mozilla\.)/ );

  # Retrieve a list of status flags that have been set on the attachment.
  my %statuses;
  SendSQL("SELECT  id, name 
           FROM    attachstatuses JOIN attachstatusdefs 
           WHERE   attachstatuses.statusid = attachstatusdefs.id 
           AND     attach_id = $::FORM{'id'}");
  while ( my ($id, $name) = FetchSQLData() )
  {
    $statuses{$id} = $name;
  }

  # Retrieve a list of statuses for this bug's product, and build an array 
  # of hashes in which each hash is a status flag record.
  # ???: Move this into versioncache or its own routine?
  my @statusdefs;
  SendSQL("SELECT   id, name 
           FROM     attachstatusdefs, bugs 
           WHERE    bug_id = $bugid 
           AND      attachstatusdefs.product = bugs.product 
           ORDER BY sortkey");
  while ( MoreSQLData() )
  {
    my ($id, $name) = FetchSQLData();
    push @statusdefs, { 'id' => $id , 'name' => $name };
  }

  # Retrieve a list of attachments for this bug as well as a summary of the bug
  # to use in a navigation bar across the top of the screen.
  SendSQL("SELECT attach_id FROM attachments WHERE bug_id = $bugid ORDER BY attach_id");
  my @bugattachments;
  push(@bugattachments, FetchSQLData()) while (MoreSQLData());
  SendSQL("SELECT short_desc FROM bugs WHERE bug_id = $bugid");
  my ($bugsummary) = FetchSQLData();

  # Define the variables and functions that will be passed to the UI template.
  $vars->{'attachid'} = $::FORM{'id'}; 
  $vars->{'description'} = $description; 
  $vars->{'contenttype'} = $contenttype; 
  $vars->{'bugid'} = $bugid; 
  $vars->{'bugsummary'} = $bugsummary; 
  $vars->{'ispatch'} = $ispatch; 
  $vars->{'isobsolete'} = $isobsolete; 
  $vars->{'isviewable'} = $isviewable; 
  $vars->{'statuses'} = \%statuses; 
  $vars->{'statusdefs'} = \@statusdefs; 
  $vars->{'attachments'} = \@bugattachments; 

  # Return the appropriate HTTP response headers.
  print "Content-Type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/edit.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;
}


sub update
{
  # Update an attachment record.

  # Get the bug ID for the bug to which this attachment is attached.
  SendSQL("SELECT bug_id FROM attachments WHERE attach_id = $::FORM{'id'}");
  my $bugid = FetchSQLData() 
    || DisplayError("Cannot figure out bug number.")
    && exit;

  # Lock database tables in preparation for updating the attachment.
  SendSQL("LOCK TABLES attachments WRITE , attachstatuses WRITE , 
           attachstatusdefs READ , fielddefs READ , bugs_activity WRITE");

  # Get a copy of the attachment record before we make changes
  # so we can record those changes in the activity table.
  SendSQL("SELECT description, mimetype, ispatch, isobsolete 
           FROM attachments WHERE attach_id = $::FORM{'id'}");
  my ($olddescription, $oldcontenttype, $oldispatch, $oldisobsolete) = FetchSQLData();

  # Get the list of old status flags.
  SendSQL("SELECT    attachstatusdefs.name 
           FROM      attachments, attachstatuses, attachstatusdefs
           WHERE     attachments.attach_id = $::FORM{'id'}
           AND       attachments.attach_id = attachstatuses.attach_id
           AND       attachstatuses.statusid = attachstatusdefs.id
           ORDER BY  attachstatusdefs.sortkey
          ");
  my @oldstatuses;
  while (MoreSQLData()) {
    push(@oldstatuses, FetchSQLData());
  }
  my $oldstatuslist = join(', ', @oldstatuses);

  # Update the database with the new status flags.
  SendSQL("DELETE FROM attachstatuses WHERE attach_id = $::FORM{'id'}");
  foreach my $statusid (@{$::MFORM{'status'}}) 
  {
    SendSQL("INSERT INTO attachstatuses (attach_id, statusid) VALUES ($::FORM{'id'}, $statusid)");
  }

  # Get the list of new status flags.
  SendSQL("SELECT    attachstatusdefs.name 
           FROM      attachments, attachstatuses, attachstatusdefs
           WHERE     attachments.attach_id = $::FORM{'id'}
           AND       attachments.attach_id = attachstatuses.attach_id
           AND       attachstatuses.statusid = attachstatusdefs.id
           ORDER BY  attachstatusdefs.sortkey
          ");
  my @newstatuses;
  while (MoreSQLData()) {
    push(@newstatuses, FetchSQLData());
  }
  my $newstatuslist = join(', ', @newstatuses);

  # Quote the description and content type for use in the SQL UPDATE statement.
  my $quoteddescription = SqlQuote($::FORM{'description'});
  my $quotedcontenttype = SqlQuote($::FORM{'contenttype'});

  # Update the attachment record in the database.
  # Sets the creation timestamp to itself to avoid it being updated automatically.
  SendSQL("UPDATE  attachments 
           SET     description = $quoteddescription , 
                   mimetype = $quotedcontenttype , 
                   ispatch = $::FORM{'ispatch'} , 
                   isobsolete = $::FORM{'isobsolete'} , 
                   creation_ts = creation_ts
           WHERE   attach_id = $::FORM{'id'}
         ");

  # Record changes in the activity table.
  if ($olddescription ne $::FORM{'description'}) {
    my $quotedolddescription = SqlQuote($olddescription);
    my $fieldid = GetFieldID('attachments.description');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, NOW(), $fieldid, $quotedolddescription, $quoteddescription)");
  }
  if ($oldcontenttype ne $::FORM{'contenttype'}) {
    my $quotedoldcontenttype = SqlQuote($oldcontenttype);
    my $fieldid = GetFieldID('attachments.mimetype');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, NOW(), $fieldid, $quotedoldcontenttype, $quotedcontenttype)");
  }
  if ($oldispatch ne $::FORM{'ispatch'}) {
    my $fieldid = GetFieldID('attachments.ispatch');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, NOW(), $fieldid, $oldispatch, $::FORM{'ispatch'})");
  }
  if ($oldisobsolete ne $::FORM{'isobsolete'}) {
    my $fieldid = GetFieldID('attachments.isobsolete');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, NOW(), $fieldid, $oldisobsolete, $::FORM{'isobsolete'})");
  }
  if ($oldstatuslist ne $newstatuslist) {
    my ($removed, $added) = DiffStrings($oldstatuslist, $newstatuslist);
    my $quotedremoved = SqlQuote($removed);
    my $quotedadded = SqlQuote($added);
    my $fieldid = GetFieldID('attachstatusdefs.name');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, NOW(), $fieldid, $quotedremoved, $quotedadded)");
  }

  # Unlock all database tables now that we are finished updating the database.
  SendSQL("UNLOCK TABLES");

  # If this installation has enabled the request manager, let the manager know
  # an attachment was updated so it can check for requests on that attachment
  # and fulfill them.  The request manager allows users to request database
  # changes of other users and tracks the fulfillment of those requests.  When
  # an attachment record is updated and the request manager is called, it will
  # fulfill those requests that were requested of the user performing the update
  # which are requests for the attachment being updated.
  #my $requests;
  #if (Param('userequestmanager'))
  #{
  #  use Request;
  #  # Specify the fieldnames that have been updated.
  #  my @fieldnames = ('description', 'mimetype', 'status', 'ispatch', 'isobsolete');
  #  # Fulfill pending requests.
  #  $requests = Request::fulfillRequest('attachment', $::FORM{'id'}, @fieldnames);
  #  $vars->{'requests'} = $requests; 
  #}

  # If the user submitted a comment while editing the attachment, 
  # add the comment to the bug.
  if ( $::FORM{'comment'} )
  {
    use Text::Wrap;
    $Text::Wrap::columns = 80;
    $Text::Wrap::huge = 'wrap';

    # Append a string to the comment to let users know that the comment came from
    # the "edit attachment" screen.
    my $comment = qq|(From update of attachment $::FORM{'id'})\n| . $::FORM{'comment'};

    my $wrappedcomment = "";
    foreach my $line (split(/\r\n|\r|\n/, $comment))
    {
      if ( $line =~ /^>/ )
      {
        $wrappedcomment .= $line . "\n";
      }
      else
      {
        $wrappedcomment .= wrap('', '', $line) . "\n";
      }
    }

    # Get the user's login name since the AppendComment function needs it.
    my $who = DBID_to_name($::userid);
    # Mention $::userid again so Perl doesn't give me a warning about it.
    my $neverused = $::userid;

    # Append the comment to the list of comments in the database.
    AppendComment($bugid, $who, $wrappedcomment);

  }

  # Send mail to let people know the bug has changed.  Uses a special syntax
  # of the "open" and "exec" commands to capture the output of "processmail",
  # which "system" doesn't allow, without running the command through a shell,
  # which backticks (``) do.
  #system ("./processmail", $bugid , $::userid);
  #my $mailresults = `./processmail $bugid $::userid`;
  my $mailresults = '';
  open(PMAIL, "-|") or exec('./processmail', $bugid, DBID_to_name($::userid));
  $mailresults .= $_ while <PMAIL>;
  close(PMAIL);
 
  # Define the variables and functions that will be passed to the UI template.
  $vars->{'attachid'} = $::FORM{'id'}; 
  $vars->{'bugid'} = $bugid; 
  $vars->{'mailresults'} = $mailresults; 

  # Return the appropriate HTTP response headers.
  print "Content-Type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/updated.atml", $vars)
    || DisplayError("Template process failed: " . $template->error())
    && exit;

}
