#!/usr/bonsaitools/bin/perl -wT
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
#                 Daniel Raichle <draichle@gmx.net>
#                 Dave Miller <justdave@syndicomm.com>

################################################################################
# Script Initialization
################################################################################

# Make it harder for us to do dangerous things in Perl.
use strict;

use lib qw(.);

use vars qw(
  $cgi
  $template
  $vars
);

# Include the Bugzilla CGI and general utility library.
require "CGI.pl";

# Use these modules to handle flags.
use Bugzilla::Flag; 
use Bugzilla::FlagType; 
use Bugzilla::User;

# Establish a connection to the database backend.
ConnectToDatabase();

# Check whether or not the user is logged in and, if so, set the $::userid 
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
  ValidateComment($::FORM{'comment'});
  validateFilename();
  validateIsPatch();
  my $data = validateData();
  validateDescription();
  validateContentType() unless $::FORM{'ispatch'};
  validateObsolete() if $::FORM{'obsolete'};
  insert($data);
}
elsif ($action eq "edit") 
{ 
  quietly_check_login();
  validateID();
  validateCanEdit($::FORM{'id'});
  edit(); 
}
elsif ($action eq "update") 
{ 
  confirm_login();
  ValidateComment($::FORM{'comment'});
  validateID();
  validateCanEdit($::FORM{'id'});
  validateDescription();
  validateIsPatch();
  validateContentType() unless $::FORM{'ispatch'};
  validateIsObsolete();
  validatePrivate();
  Bugzilla::User::match_field({ '^requestee(_type)?-(\d+)$' => 
                                    { 'type' => 'single' } });
  Bugzilla::Flag::validate(\%::FORM);
  Bugzilla::FlagType::validate(\%::FORM);
  update();
}
else 
{ 
  ThrowCodeError("unknown_action");
}

exit;

################################################################################
# Data Validation / Security Authorization
################################################################################

sub validateID
{
    # Validate the value of the "id" form field, which must contain an
    # integer that is the ID of an existing attachment.

    $vars->{'attach_id'} = $::FORM{'id'};
    
    detaint_natural($::FORM{'id'}) 
     || ThrowUserError("invalid_attach_id");
  
    # Make sure the attachment exists in the database.
    SendSQL("SELECT bug_id, isprivate FROM attachments WHERE attach_id = $::FORM{'id'}");
    MoreSQLData()
      || ThrowUserError("invalid_attach_id");

    # Make sure the user is authorized to access this attachment's bug.
    my ($bugid, $isprivate) = FetchSQLData();
    ValidateBugID($bugid);
    if (($isprivate > 0 ) && Param("insidergroup") && !(UserInGroup(Param("insidergroup")))) {
        ThrowUserError("attachment_access_denied");
    }
}

sub validateCanEdit
{
    my ($attach_id) = (@_);

    # If the user is not logged in, claim that they can edit. This allows
    # the edit scrren to be displayed to people who aren't logged in.
    # People not logged in can't actually commit changes, because that code
    # calls confirm_login, not quietly_check_login, before calling this sub
    return if $::userid == 0;

    # People in editbugs can edit all attachments
    return if UserInGroup("editbugs");

    # Bug 97729 - the submitter can edit their attachments
    SendSQL("SELECT attach_id FROM attachments WHERE " .
            "attach_id = $attach_id AND submitter_id = $::userid");

    FetchSQLData()
      || ThrowUserError("illegal_attachment_edit");
}

sub validateDescription
{
  $::FORM{'description'}
    || ThrowUserError("missing_attachment_description");
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
    ThrowUserError("missing_content_type_method");
  }
  elsif ($::FORM{'contenttypemethod'} eq 'autodetect')
  {
    my $contenttype = $cgi->uploadInfo($cgi->param('data'))->{'Content-Type'};
    # The user asked us to auto-detect the content type, so use the type
    # specified in the HTTP request headers.
    if ( !$contenttype )
    {
      ThrowUserError("missing_content_type");
    }
    $::FORM{'contenttype'} = $contenttype;
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
    $vars->{'contenttypemethod'} = $::FORM{'contenttypemethod'};
    ThrowCodeError("illegal_content_type_method");
  }

  if ( $::FORM{'contenttype'} !~ /^(application|audio|image|message|model|multipart|text|video)\/.+$/ )
  {
    $vars->{'contenttype'} = $::FORM{'contenttype'};
    ThrowUserError("invalid_content_type");
  }
}

sub validateIsObsolete
{
  # Set the isobsolete flag to zero if it is undefined, since the UI uses
  # an HTML checkbox to represent this flag, and unchecked HTML checkboxes
  # do not get sent in HTML requests.
  $::FORM{'isobsolete'} = $::FORM{'isobsolete'} ? 1 : 0;
}

sub validatePrivate
{
    # Set the isprivate flag to zero if it is undefined, since the UI uses
    # an HTML checkbox to represent this flag, and unchecked HTML checkboxes
    # do not get sent in HTML requests.
    $::FORM{'isprivate'} = $::FORM{'isprivate'} ? 1 : 0;
}

sub validateData
{
  my $maxsize = $::FORM{'ispatch'} ? Param('maxpatchsize') : Param('maxattachmentsize');
  $maxsize *= 1024; # Convert from K

  my $fh = $cgi->upload('data');
  my $data;

  # We could get away with reading only as much as required, except that then
  # we wouldn't have a size to print to the error handler below.
  {
      # enable 'slurp' mode
      local $/;
      $data = <$fh>;
  }

  $data
    || ThrowUserError("zero_length_file");

  # Make sure the attachment does not exceed the maximum permitted size
  my $len = length($data);
  if ($maxsize && $len > $maxsize) {
      $vars->{'filesize'} = sprintf("%.0f", $len/1024);
      if ( $::FORM{'ispatch'} ) {
          ThrowUserError("patch_too_large");
      } else {
          ThrowUserError("file_too_large");
      }
  }

  return $data;
}

my $filename;
sub validateFilename
{
  defined $cgi->upload('data')
    || ThrowUserError("file_not_specified");

  $filename = $cgi->upload('data');
  
  # Remove path info (if any) from the file name.  The browser should do this
  # for us, but some are buggy.  This may not work on Mac file names and could
  # mess up file names with slashes in them, but them's the breaks.  We only
  # use this as a hint to users downloading attachments anyway, so it's not 
  # a big deal if it munges incorrectly occasionally.
  $filename =~ s/^.*[\/\\]//;

  # Truncate the filename to 100 characters, counting from the end of the string
  # to make sure we keep the filename extension.
  $filename = substr($filename, -100, 100);
}

sub validateObsolete
{
  # Make sure the attachment id is valid and the user has permissions to view
  # the bug to which it is attached.
  foreach my $attachid (@{$::MFORM{'obsolete'}}) {
    $vars->{'attach_id'} = $attachid;
    
    detaint_natural($attachid)
      || ThrowCodeError("invalid_attach_id_to_obsolete");
  
    SendSQL("SELECT bug_id, isobsolete, description 
             FROM attachments WHERE attach_id = $attachid");

    # Make sure the attachment exists in the database.
    MoreSQLData()
      || ThrowUserError("invalid_attach_id");

    my ($bugid, $isobsolete, $description) = FetchSQLData();

    $vars->{'description'} = $description;
    
    if ($bugid != $::FORM{'bugid'})
    {
      $vars->{'my_bug_id'} = $::FORM{'bugid'};
      $vars->{'attach_bug_id'} = $bugid;
      ThrowCodeError("mismatched_bug_ids_on_obsolete");
    }

    if ( $isobsolete )
    {
      ThrowCodeError("attachment_already_obsolete");
    }

    # Check that the user can modify this attachment
    validateCanEdit($attachid);
  }

}

################################################################################
# Functions
################################################################################

sub view
{
    # Display an attachment.

    # Retrieve the attachment content and its content type from the database.
    SendSQL("SELECT mimetype, filename, thedata FROM attachments WHERE attach_id = $::FORM{'id'}");
    my ($contenttype, $filename, $thedata) = FetchSQLData();

    # Return the appropriate HTTP response headers.
    $filename =~ s/^.*[\/\\]//;
    my $filesize = length($thedata);
    print qq{Content-Type: $contenttype; name="$filename"\n};
    print qq{Content-Disposition: inline; filename=$filename\n};
    print qq{Content-Length: $filesize\n};
    print qq{\n$thedata};

}


sub viewall
{
  # Display all attachments for a given bug in a series of IFRAMEs within one HTML page.

  # Retrieve the attachments from the database and write them into an array
  # of hashes where each hash represents one attachment.
    my $privacy = "";
    if (Param("insidergroup") && !(UserInGroup(Param("insidergroup")))) {
        $privacy = "AND isprivate < 1 ";
    }
    SendSQL("SELECT attach_id, DATE_FORMAT(creation_ts, '%Y.%m.%d %H:%i'),
            mimetype, description, ispatch, isobsolete, isprivate 
            FROM attachments WHERE bug_id = $::FORM{'bugid'} $privacy 
            ORDER BY attach_id");
  my @attachments; # the attachments array
  while (MoreSQLData())
  {
    my %a; # the attachment hash
    ($a{'attachid'}, $a{'date'}, $a{'contenttype'}, 
     $a{'description'}, $a{'ispatch'}, $a{'isobsolete'}, $a{'isprivate'}) = FetchSQLData();

    # Flag attachments as to whether or not they can be viewed (as opposed to
    # being downloaded).  Currently I decide they are viewable if their MIME type 
    # is either text/*, image/*, or application/vnd.mozilla.*.
    # !!! Yuck, what an ugly hack.  Fix it!
    $a{'isviewable'} = ( $a{'contenttype'} =~ /^(text|image|application\/vnd\.mozilla\.)/ );

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
  $template->process("attachment/show-multiple.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}


sub enter
{
  # Display a form for entering a new attachment.

  # Retrieve the attachments the user can edit from the database and write
  # them into an array of hashes where each hash represents one attachment.
  my $canEdit = "";
  if (!UserInGroup("editbugs")) {
      $canEdit = "AND submitter_id = $::userid";
  }
  SendSQL("SELECT attach_id, description, isprivate
           FROM attachments
           WHERE bug_id = $::FORM{'bugid'}
           AND isobsolete = 0 $canEdit
           ORDER BY attach_id");
  my @attachments; # the attachments array
  while ( MoreSQLData() ) {
    my %a; # the attachment hash
    ($a{'id'}, $a{'description'}, $a{'isprivate'}) = FetchSQLData();

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
  $template->process("attachment/create.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}


sub insert
{
  my ($data) = @_;

  # Insert a new attachment into the database.

  # Escape characters in strings that will be used in SQL statements.
  $filename = SqlQuote($filename);
  my $description = SqlQuote($::FORM{'description'});
  my $contenttype = SqlQuote($::FORM{'contenttype'});
  my $thedata = SqlQuote($data);
  my $isprivate = $::FORM{'isprivate'} ? 1 : 0;

  # Insert the attachment into the database.
  SendSQL("INSERT INTO attachments (bug_id, creation_ts, filename, description, mimetype, ispatch, isprivate, submitter_id, thedata) 
           VALUES ($::FORM{'bugid'}, now(), $filename, $description, $contenttype, $::FORM{'ispatch'}, $isprivate, $::userid, $thedata)");

  # Retrieve the ID of the newly created attachment record.
  SendSQL("SELECT LAST_INSERT_ID()");
  my $attachid = FetchOneColumn();

  # Insert a comment about the new attachment into the database.
  my $comment = "Created an attachment (id=$attachid)\n$::FORM{'description'}\n";
  $comment .= ("\n" . $::FORM{'comment'}) if $::FORM{'comment'};

  use Text::Wrap;
  $Text::Wrap::columns = 80;
  $Text::Wrap::huge = 'overflow';
  $comment = Text::Wrap::wrap('', '', $comment);

  AppendComment($::FORM{'bugid'}, 
                $::COOKIE{"Bugzilla_login"},
                $comment,
                $isprivate);

  # Make existing attachments obsolete.
  my $fieldid = GetFieldID('attachments.isobsolete');
  foreach my $obsolete_id (@{$::MFORM{'obsolete'}}) {
      SendSQL("UPDATE attachments SET isobsolete = 1 WHERE attach_id = $obsolete_id");
      SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
               VALUES ($::FORM{'bugid'}, $obsolete_id, $::userid, NOW(), $fieldid, '0', '1')");
      # If the obsolete attachment has pending flags, migrate them to the new attachment.
      if (Bugzilla::Flag::count({ 'attach_id' => $obsolete_id , 'status' => 'pending' })) {
        Bugzilla::Flag::migrate($obsolete_id, $attachid);
      }
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
  $template->process("attachment/created.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}


sub edit
{
  # Edit an attachment record.  Users with "editbugs" privileges, (or the 
  # original attachment's submitter) can edit the attachment's description,
  # content type, ispatch and isobsolete flags, and statuses, and they can
  # also submit a comment that appears in the bug.
  # Users cannot edit the content of the attachment itself.

  # Retrieve the attachment from the database.
  SendSQL("SELECT description, mimetype, filename, bug_id, ispatch, isobsolete, isprivate 
           FROM attachments WHERE attach_id = $::FORM{'id'}");
  my ($description, $contenttype, $filename, $bugid, $ispatch, $isobsolete, $isprivate) = FetchSQLData();

  # Flag attachment as to whether or not it can be viewed (as opposed to
  # being downloaded).  Currently I decide it is viewable if its content
  # type is either text/.* or application/vnd.mozilla.*.
  # !!! Yuck, what an ugly hack.  Fix it!
  my $isviewable = ( $contenttype =~ /^(text|image|application\/vnd\.mozilla\.)/ );

  # Retrieve a list of attachments for this bug as well as a summary of the bug
  # to use in a navigation bar across the top of the screen.
  SendSQL("SELECT attach_id FROM attachments WHERE bug_id = $bugid ORDER BY attach_id");
  my @bugattachments;
  push(@bugattachments, FetchSQLData()) while (MoreSQLData());
  SendSQL("SELECT short_desc FROM bugs WHERE bug_id = $bugid");
  my ($bugsummary) = FetchSQLData();
  
  # Get a list of flag types that can be set for this attachment.
  SendSQL("SELECT product_id, component_id FROM bugs WHERE bug_id = $bugid");
  my ($product_id, $component_id) = FetchSQLData();
  my $flag_types = Bugzilla::FlagType::match({ 'target_type'  => 'attachment' , 
                                               'product_id'   => $product_id , 
                                               'component_id' => $component_id });
  foreach my $flag_type (@$flag_types) {
    $flag_type->{'flags'} = Bugzilla::Flag::match({ 'type_id'   => $flag_type->{'id'}, 
                                                    'attach_id' => $::FORM{'id'} });
  }
  $vars->{'flag_types'} = $flag_types;
  $vars->{'any_flags_requesteeble'} = grep($_->{'is_requesteeble'}, @$flag_types);
  
  # Define the variables and functions that will be passed to the UI template.
  $vars->{'attachid'} = $::FORM{'id'}; 
  $vars->{'description'} = $description; 
  $vars->{'contenttype'} = $contenttype; 
  $vars->{'filename'} = $filename;
  $vars->{'bugid'} = $bugid; 
  $vars->{'bugsummary'} = $bugsummary; 
  $vars->{'ispatch'} = $ispatch; 
  $vars->{'isobsolete'} = $isobsolete; 
  $vars->{'isprivate'} = $isprivate; 
  $vars->{'isviewable'} = $isviewable; 
  $vars->{'attachments'} = \@bugattachments; 

  # Return the appropriate HTTP response headers.
  print "Content-Type: text/html\n\n";

  # Generate and return the UI (HTML page) from the appropriate template.
  $template->process("attachment/edit.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}


sub update
{
  # Updates an attachment record.

  # Get the bug ID for the bug to which this attachment is attached.
  SendSQL("SELECT bug_id FROM attachments WHERE attach_id = $::FORM{'id'}");
  my $bugid = FetchSQLData();
  unless ($bugid) 
  {
    $vars->{'bug_id'} = $bugid;
    ThrowUserError("invalid_bug_id");
  }
  
  # Lock database tables in preparation for updating the attachment.
  SendSQL("LOCK TABLES attachments WRITE , flags WRITE , " . 
          "flagtypes READ , fielddefs READ , bugs_activity WRITE, " . 
          "flaginclusions AS i READ, flagexclusions AS e READ, " . 
          "bugs READ, profiles READ");
  
  # Get a copy of the attachment record before we make changes
  # so we can record those changes in the activity table.
  SendSQL("SELECT description, mimetype, filename, ispatch, isobsolete, isprivate
           FROM attachments WHERE attach_id = $::FORM{'id'}");
  my ($olddescription, $oldcontenttype, $oldfilename, $oldispatch,
      $oldisobsolete, $oldisprivate) = FetchSQLData();

  # Quote the description and content type for use in the SQL UPDATE statement.
  my $quoteddescription = SqlQuote($::FORM{'description'});
  my $quotedcontenttype = SqlQuote($::FORM{'contenttype'});
  my $quotedfilename = SqlQuote($::FORM{'filename'});

  # Update the attachment record in the database.
  # Sets the creation timestamp to itself to avoid it being updated automatically.
  SendSQL("UPDATE  attachments 
           SET     description = $quoteddescription , 
                   mimetype = $quotedcontenttype , 
                   filename = $quotedfilename ,
                   ispatch = $::FORM{'ispatch'} , 
                   isobsolete = $::FORM{'isobsolete'} ,
                   isprivate = $::FORM{'isprivate'} 
           WHERE   attach_id = $::FORM{'id'}
         ");

  # Figure out when the changes were made.
  SendSQL("SELECT NOW()");
  my $timestamp = FetchOneColumn();
    
  # Record changes in the activity table.
  my $sql_timestamp = SqlQuote($timestamp);
  if ($olddescription ne $::FORM{'description'}) {
    my $quotedolddescription = SqlQuote($olddescription);
    my $fieldid = GetFieldID('attachments.description');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, $sql_timestamp, $fieldid, $quotedolddescription, $quoteddescription)");
  }
  if ($oldcontenttype ne $::FORM{'contenttype'}) {
    my $quotedoldcontenttype = SqlQuote($oldcontenttype);
    my $fieldid = GetFieldID('attachments.mimetype');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, $sql_timestamp, $fieldid, $quotedoldcontenttype, $quotedcontenttype)");
  }
  if ($oldfilename ne $::FORM{'filename'}) {
    my $quotedoldfilename = SqlQuote($oldfilename);
    my $fieldid = GetFieldID('attachments.filename');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, NOW(), $fieldid, $quotedoldfilename, $quotedfilename)");
  }
  if ($oldispatch ne $::FORM{'ispatch'}) {
    my $fieldid = GetFieldID('attachments.ispatch');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, $sql_timestamp, $fieldid, $oldispatch, $::FORM{'ispatch'})");
  }
  if ($oldisobsolete ne $::FORM{'isobsolete'}) {
    my $fieldid = GetFieldID('attachments.isobsolete');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, $sql_timestamp, $fieldid, $oldisobsolete, $::FORM{'isobsolete'})");
  }
  if ($oldisprivate ne $::FORM{'isprivate'}) {
    my $fieldid = GetFieldID('attachments.isprivate');
    SendSQL("INSERT INTO bugs_activity (bug_id, attach_id, who, bug_when, fieldid, removed, added) 
             VALUES ($bugid, $::FORM{'id'}, $::userid, NOW(), $fieldid, $oldisprivate, $::FORM{'isprivate'})");
  }
  
  # Update flags.
  my $target = Bugzilla::Flag::GetTarget(undef, $::FORM{'id'});
  Bugzilla::Flag::process($target, $timestamp, \%::FORM);
  
  # Unlock all database tables now that we are finished updating the database.
  SendSQL("UNLOCK TABLES");

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
    AppendComment($bugid, $who, $wrappedcomment, $::FORM{'isprivate'}, $timestamp);

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
  $template->process("attachment/updated.html.tmpl", $vars)
    || ThrowTemplateError($template->error());
}
