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
# The Original Code is Doctor.
#
# The Initial Developer of the Original Code is Netscape 
# Communications Corporation. Portions created by Netscape 
# are Copyright (C) 2002 Netscape Communications Corporation. 
# All Rights Reserved.
#
# Contributor(s): Myk Melez <myk@mozilla.org>

################################################################################
# Script Initialization
################################################################################

# Make it harder to do dangerous things in Perl.
use diagnostics;
use strict;

# Make it easier to access Perl's built-in variables by turning on the option 
# of referencing them by sensible name instead of punctuation mark.
use English;

# Include the standard Perl CGI library and create a new request object
# to handle this CGI request.
use CGI;
my $request = new CGI;

# Use a useful library for capturing filehandle output into a variable.
# http://groups.google.com/groups?selm=yr0e8.12924%24ZC3.1033373%40newsread2.prod.itd.earthlink.net
use IO::Capture;

# Include the Perl library for creating and deleting directory hierarchies
# and the one for creating temporary files and directories.
use File::Path;
use File::Temp qw/ tempfile tempdir /;

# Use the Template Toolkit (http://www.template-toolkit.org/) to generate
# the user interface using templates in the "template/" subdirectory.
use Template;

# Create the global template object that processes templates and specify
# configuration parameters that apply to templates processed in this script.
my $template = Template->new(
  {
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => "templates" ,

    FILTERS => 
    {
        linebreak => sub 
        {
          my ($var) = @_; 
          $var =~ s/\\/\\\\/g;
          $var =~ s/\n/\\n/g;
          $var =~ s/\r/\\r/g;
          return $var;
        }
    }
  }
);

# Define the global variables and functions that will be passed to the UI
# template.  Individual functions add their own values to this hash before
# sending them to the templates they process.
my $vars = {};

################################################################################
# Script Configuration
################################################################################

# Note: Look in the configuration file for descriptions of each parameter.

# Use the AppConfig module for handling script configuration.
use AppConfig qw(:expand :argcount);

# Create an AppConfig object and populate it with parameters defined
# in the configuration file.
my $config = AppConfig->new(
  { 
    CREATE => 1 , 
    GLOBAL => {
      ARGCOUNT => ARGCOUNT_ONE , 
    }
  }
);
$config->file("doctor.conf");

# Write parameters into global variables.
my $TEMP_DIR = $config->get('TEMP_DIR');
my $READ_CVS_SERVER = $config->get('READ_CVS_SERVER');
my $READ_CVS_USERNAME = $config->get('READ_CVS_USERNAME');
my $READ_CVS_PASSWORD = $config->get('READ_CVS_PASSWORD');
my $WRITE_CVS_SERVER = $config->get('WRITE_CVS_SERVER');
my $WEB_BASE_URI = $config->get('WEB_BASE_URI');
my $WEB_BASE_URI_PATTERN = $config->get('WEB_BASE_URI_PATTERN');
my $WEB_BASE_PATH = $config->get('WEB_BASE_PATH');
my $ADMIN_EMAIL = $config->get('ADMIN_EMAIL');

# !!! Switch use of the global variables above to this hash!
my $CONFIG = {
  TEMP_DIR              => $TEMP_DIR              , 
  READ_CVS_SERVER       => $READ_CVS_SERVER       ,
  READ_CVS_USERNAME     => $READ_CVS_USERNAME     ,
  READ_CVS_PASSWORD     => $READ_CVS_PASSWORD     ,
  WRITE_CVS_SERVER      => $WRITE_CVS_SERVER      ,
  WEB_BASE_URI          => $WEB_BASE_URI          ,
  WEB_BASE_URI_PATTERN  => $WEB_BASE_URI_PATTERN  ,
  WEB_BASE_PATH         => $WEB_BASE_PATH         ,
  ADMIN_EMAIL           => $ADMIN_EMAIL           ,
  DB_HOST               => $config->get('DB_HOST') ,
  DB_PORT               => $config->get('DB_PORT') ,
  DB_NAME               => $config->get('DB_NAME') ,
  DB_USERNAME           => $config->get('DB_USERNAME') ,
  DB_PASSWORD           => $config->get('DB_PASSWORD') ,
};

$vars->{'CONFIG'} = $CONFIG;

# Store the home directory so we can get back to it after changing directories
# in certain places in the code.
use Cwd;
my $HOME = cwd;

################################################################################
# Main Body Execution
################################################################################

# Note: All calls to this script should contain an "action" variable whose 
# value determines what the user wants to do.  The code below checks the value 
# of that variable and runs the appropriate code.

# Determine whether to use the action specified by the user or the default,
# and if the user wants to edit a file, but they haven't specified the name
# of the file, prompt them for it.
my $action = 
  lc($request->param('action')) || ($request->param('file') ? "edit" : "choose");

if    ($action eq "choose")       { choose()      }
elsif ($action eq "edit")         { edit()        }
elsif ($action eq "review")       { review()      }
elsif ($action eq "commit")       { commit()      }
elsif ($action eq "create")       { create()      }
elsif ($action eq "download")     { download()    }
elsif ($action eq "display")      { display()     }
elsif ($action eq "diff")         { diff()        }
elsif ($action eq "queue")        { queue()       }
elsif ($action eq "list")         { list()        }
elsif ($action eq "regurgitate")  { regurgitate() }
else
{
  ThrowCodeError("couldn't recognize the value of the action parameter", "Unknown Action");
}

exit;

################################################################################
# Main Execution Functions
################################################################################

sub choose
{
  print $request->header;
  $template->process("select.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub edit
{
  if ($request->param('patch_id')) {
    # Get the patch and metadata from the database.
    my $id = ValidateID($request->param('patch_id'));
    my $dbh = ConnectToDatabase();
    my $get_patch_sth =
      $dbh->prepare("SELECT file, version, patch FROM patch WHERE id = ?");
    $get_patch_sth->execute($id);
    my ($file, $oldversion, $patch) = $get_patch_sth->fetchrow_array();
    $get_patch_sth->finish();
    $dbh->disconnect();

    # Check out the file from the repository.
    CreateTempDir();
    my $newversion = CheckOutFile($file);
    
    ValidateVersions($oldversion, $newversion);

    # Apply the patch.
    open(PATCH, "|/usr/bin/patch $file 2>&1 > /dev/null");
    print PATCH $patch;
    close(PATCH);

    # Get the file with the patch applied.
    open(NEWFILE, "< $file");
    undef $/;
    my $content = <NEWFILE>;
    close(NEWFILE);
  
    # Delete the temporary directory into which we checked out the file.
    DeleteTempDir();
  
    my $line_endings = "unix";
    if ($content =~ /\r\n/s) { $line_endings = "windows" }
    elsif ($content =~ /\r[^\n]/s) { $line_endings = "mac" }

    $vars->{'content'} = $content;
    $vars->{'version'} = $newversion;
    $vars->{'line_endings'} = $line_endings;
    $vars->{'patch_id'} = $id;
    $vars->{'file'} = $file;
  }
  else {
    ValidateFile();
   
    $vars->{'file'} = $request->param('file');
    
    if ($request->param('content'))
    {
      ValidateContent();
      $vars->{'content'} = $request->param('content');
      $vars->{'version'} = $request->param('version');
      $vars->{'line_endings'} = $request->param('line_endings');
    }
    else
    {
      ($vars->{'content'}, $vars->{'version'}, $vars->{'line_endings'}) 
        = RetrieveFile($request->param('file'));
    }
  }

  print $request->header;
  $template->process("edit.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub download
{
  ValidateFile();
 
  my $file = $request->param('file');
  
  # Separate the name of the file from its path.
  $file =~ /^(.*)\/([^\/]+)$/;
  my $path = $1;
  my $filename = $2;

  my ($content) = RetrieveFile($file);
  my $filesize = length($content);
    
  print $request->header(-type=>"text/plain; name=\"$filename\"",
                         -content_disposition=> "attachment; filename=\"$filename\"",
                         -content_length => $filesize);
  print $content;
}

sub display
{
  ValidateFile();
 
  my $file = $request->param('file');
  
  # Separate the name of the file from its path.
  $file =~ /^(.*)\/([^\/]+)$/;
  my $path = $1;
  my $filename = $2;

  my ($content) = RetrieveFile($file);
  my $filesize = length($content);
    
  print $request->header(-type=>"text/html; name=\"$filename\"",
                         -content_disposition=> "inline; filename=\"$filename\"",
                         -content_length => $filesize);
  print $content;
}

sub regurgitate
{
  ValidateFile();
 
  my $content = GetUploadedContent() || $request->param('content');

  my $file = $request->param('file');
  
  # Separate the name of the file from its path.
  $file =~ /^(.*)\/([^\/]+)$/;
  my $path = $1;
  my $filename = $2;

  my $filesize = length($content);
    
  print $request->header(-type=>"text/html; name=\"$filename\"",
                         -content_disposition=> "inline; filename=\"$filename\"",
                         -content_length => $filesize);
  print $content;
}

sub review
{
  # Displays a diff between the version of a file in the repository
  # and the version submitted by the user so the user can review the
  # changes they are making to the file.  Offers the user the option
  # to either commit their changes or edit the file some more.
  
  ValidateFile();
  
  # Create and change to a temporary sub-directory into which 
  # we will check out the file being reviewed.
  CreateTempDir();
  
  # Check out the file from the repository.
  my $file = $request->param('file');
  my $oldversion = $request->param('version');
  my $newversion = CheckOutFile($file);
  
  # Throw an error if the version of the file that was edited
  # does not match the version in the repository.  In the future
  # we should try to merge the user's changes if possible.
  if ($oldversion && $newversion && $oldversion != $newversion) 
  {
    ThrowCodeError("You edited version <em>$oldversion</em> of the file,
      but version <em>$newversion</em> is in the repository.  Reload the edit 
      page and make your changes again (and bother the authors of this script 
      to implement change merging
      (<a href=\"http://bugzilla.mozilla.org/show_bug.cgi?id=164342\">bug 164342</a>).");
  }
  
  # Replace the checked out file with the edited version, generate
  # a diff between the edited file, and display it to the user.
  ReplaceFile($file);
  $vars->{'diff'} = DiffFile($file);
  
  # Delete the temporary directory into which we checked out the file.
  DeleteTempDir();
  
  $vars->{'file'} = $file;
  $vars->{'content'} = $request->param('content');
  $vars->{'version'} = $request->param('version');
  $vars->{'line_endings'} = $request->param('line_endings');

  my $raw = $request->param('raw');
  my $ctype = $raw ? "text/plain" : "text/html";
  print $request->header(-type=>$ctype);
  $raw ? print $vars->{diff}
       : $template->process("review.tmpl", $vars)
           || ThrowCodeError("Template Process Failed", $template->error());
}

sub commit 
{
  # Commit a file to the repository.  Checks out the file via cvs, 
  # applies the changes, generates a diff, and then checks the file 
  # back in.  It would be easier if we could commit a file by piping
  # it into standard input, but cvs does not provide for this.
  
  ValidateFile();
  ValidateContent();
  ValidateUsername();
  ValidatePassword();
  ValidateComment();
  my $patch_id;
  if ($request->param('patch_id')) {
    $patch_id = ValidateID($request->param('patch_id'));
  }
  
  # Create and change to a temporary sub-directory into which 
  # we will check out the file being committed.
  CreateTempDir();
  
  # Check out the file from the repository.
  my $file = $request->param('file');
  my $oldversion = $request->param('version');
  my $newversion = CheckOutFile($file);
  
  # Throw an error if the version of the file that was edited
  # does not match the version in the repository.  In the future
  # we should try to merge the user's changes if possible.
  if ($oldversion && $newversion && $oldversion != $newversion) 
  {
    ThrowCodeError("You edited version <em>$oldversion</em> of the file,
      but version <em>$newversion</em> is in the repository.  Reload the edit 
      page and make your changes again (and bother the authors of this script 
      to implement change merging
      (<a href=\"http://bugzilla.mozilla.org/show_bug.cgi?id=164342\">bug 164342</a>).");
  }
  
  # Replace the checked out file with the edited version, generate
  # a diff between the edited file and the version in the repository,
  # and check in the edited file.
  ReplaceFile($file);
  $vars->{'diff'} = DiffFile($file);
  $vars->{'checkin_results'} = CheckInFile($file);
  
  # Delete the temporary directory into which we checked out the file.
  DeleteTempDir();
  
  # Delete the patch from the patches queue.
  if ($patch_id) {
    my $dbh = ConnectToDatabase();
    $dbh->do("DELETE FROM patch WHERE id = $patch_id");
    $dbh->disconnect;
  }
  
  $vars->{'file'} = $file;
  
  print $request->header;
  $template->process("committed.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub queue
{
  # Queue the file for review.
  # Checks out the file via cvs, applies the changes, generates a diff,
  # and then saves the diff with meta-data to a file for retrieval
  # by people with CVS access.
  
  ValidateFile();
  ValidateContent();
  # XXX validate the version as well?
  # XXX validate the user's email address
  
  # Create and change to a temporary sub-directory into which we will check out
  # the file being committed.
  CreateTempDir();
  
  # Check out the file from the repository.
  my $file = $request->param('file');
  my $comment = $request->param('queue_comment');
  my $oldversion = $request->param('version');
  my $newversion = CheckOutFile($file);
  
  # Throw an error if the version of the file that was edited
  # does not match the version in the repository.  In the future
  # we should try to merge the user's changes if possible.
  if ($oldversion && $newversion && $oldversion != $newversion) 
  {
    ThrowCodeError("You edited version <em>$oldversion</em> of the file,
      but version <em>$newversion</em> is in the repository.  Reload the edit 
      page and make your changes again (and bother the authors of this script 
      to implement change merging
      (<a href=\"http://bugzilla.mozilla.org/show_bug.cgi?id=164342\">bug 164342</a>).");
  }
  
  # Replace the checked out file with the edited version, and generate a patch
  # that patches the checked out file to make it look like the edited version.
  ReplaceFile($file);
  my $patch = DiffFile($file);
  
  # Delete the temporary directory into which we checked out the file.
  DeleteTempDir();

  my $dbh = ConnectToDatabase();
  my $insert_patch_sth =
    $dbh->prepare("INSERT INTO patch (id, submitter, submitted, file, version,
                                      patch, comment)
                   VALUES (?, ?, NOW(), ?, ?, ?, ?)");

  $dbh->do("START TRANSACTION");
  my ($patch_id) = $dbh->selectrow_array("SELECT MAX(id) FROM patch");
  $patch_id = ($patch_id || 0) + 1;
  $insert_patch_sth->execute($patch_id, $request->param('email'), $file,
                             $oldversion, $patch, $comment);
  $dbh->commit;
  $dbh->disconnect;

  print $request->header;
  $template->process("queued.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub list
{
  my $dbh = ConnectToDatabase();
  my $get_patches_sth =
    $dbh->prepare("SELECT id, submitter, submitted, file, version, comment FROM patch");
  $get_patches_sth->execute();
  my ($patch, @patches);
  while ( $patch = $get_patches_sth->fetchrow_hashref ) {
    push(@patches, $patch);
  }
  $vars->{patches} = \@patches;

  $dbh->disconnect;

  print $request->header;
  $template->process("list.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub ConnectToDatabase
{
  use DBI;
  
  # Establish a database connection.
  my $dsn = "DBI:mysql:host=$CONFIG->{DB_HOST};database=$CONFIG->{DB_NAME};port=$CONFIG->{DB_PORT}";
  my $dbh = DBI->connect($dsn,
                         $CONFIG->{DB_USERNAME},
                         $CONFIG->{DB_PASSWORD},
                         { RaiseError => 1,
                           PrintError => 0,
                           ShowErrorStatement => 1,
                           AutoCommit => 0 } );
  return $dbh;
}

sub create
{
  # Creates a new file in the repository.
  
  ValidateFile();
  ValidateContent();
  ValidateUsername();
  ValidatePassword();
  ValidateComment();
  
  # Create and change to a temporary directory where we'll do all our file
  # and CVS manipulation.
  CreateTempDir();
  
  # Separate the name of the file from its path.
  $request->param('file') =~ /^(.*)\/([^\/]+)$/;
  my $filename = $2;
  my $path = $1;
    
  # Write the file to the temporary directory.  "ReplaceFile" is an unfortunate
  # name.  !!! Replace it with a new fortunate name!
  ReplaceFile($filename);
    
  # Create a fake CVS directory that makes the temporary directory look like
  # a local working copy of the directory in the repository where this file
  # will be created.  This way we just have to commit the file to create it;
  # we avoid having to first check out the directory and then add the file.  
  
  # Make the CVS directory and change to it.
  mkdir("CVS") || ThrowCodeError("couldn't create directory 'CVS': $!");
  chdir("CVS");

  # Make the Entries file and add an entry for the new file.
  open(FILE, ">Entries") || ThrowCodeError("couldn't create file 'Entries': $!");
  print FILE "/$filename/0/Initial $filename//\nD\n";
  close(FILE);
  
  # Make an empty file named after the new file but with a ",t" appended
  # to the name.  I don't know what this is for, but the CVS client creates it,
  # so I figure I should too.
  #
  # UPDATE: Commenting out this code didn't make it fail, so I'm leaving it
  # commented out--less code means less things to break.
  #
  #open(FILE, ">${filename},t")
  #  || ThrowCodeError("couldn't create file '${filename},t': $!");
  #close(FILE);
  
  # Make the Repository file, which contains the path to the directory
  # in the CVS repository.
  open(FILE, ">Repository") 
    || ThrowCodeError("couldn't create file 'Repository': $!");
  print FILE "$path\n";
  close(FILE);
  
  # Note that we don't have to create a Root file with information about
  # the repository (authentication type, server address, root path, etc.)
  # since we add that information to the command line.
  
  # Change back to the directory containing the file and check it in.
  chdir("..");
  $vars->{'checkin_results'} = CheckInFile($filename);
  
  # Delete the temporary directory where we did all our file and CVS manipulation.
  DeleteTempDir();
  
  $vars->{'file'} = $request->param('file');
  
  print $request->header;
  $template->process("created.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

################################################################################
# Input Validation
################################################################################

sub ValidateFile
{
  # Make sure a path was entered.
  my $file = $request->param("file");
  $file
    || ThrowUserError("You must include the name/path or the URL of the file.");
  

  # URL -> Path Conversion

  # Remove the absolute URI for files on the web site (if any)
  # from the beginning of the path.
  if ($WEB_BASE_URI_PATTERN) { $file =~ s/^$WEB_BASE_URI_PATTERN//i }
  else                       { $file =~ s/^\Q$WEB_BASE_URI\E//i     }


  # Entire Path Issues

  # Collapse multiple consecutive slashes (i.e. dir//file.txt) into a single slash.
  $file =~ s:/{2,}:/:;


  # Beginning of Path Issues

  # Remove a preceding slash.
  $file =~ s:^/::;

  # Add the base path of the file in the cvs repository if necessary.
  # (i.e. if the user entered a URL or a path based on the URL).
  if ($file !~ /^\Q$WEB_BASE_PATH\E/) { $file = $WEB_BASE_PATH . $file }


  # End of Path Issues

  # If the filename (the last name in the path) contains no period,
  # it is probably a directory, so add a slash.
  if ($file =~ m:^[^\./]+$: || $file =~ m:/[^\./]+$:) { $file .= "/" }

  # If the file ends with a forward slash, it is a directory,
  # so add the name of the default file.
  if ($file =~ m:/$:) { $file .= "index.html" }


  # Set the file path.
  $request->param("file", $file);
  
  # Note: we don't need to make sure the file exists at this point
  # because CVS will tell us that.

  # Construct a URL to the file if possible.
  my $url = $file;
  if ($url =~ s/^\Q$WEB_BASE_PATH\E(.*)$/$1/i) { $vars->{'file_url'} = $WEB_BASE_URI . $url }

}

sub ValidateUsername
{
  $request->param('username')
    || ThrowUserError("You must enter your username.");

  my $username = $request->param('username');

  # If the username has an at sign in it, convert it to a percentage sign,
  # since that is probably what the user meant (CVS usernames are often
  # email addresses in which the at sign has been converted to a percentage sign.
  $username =~ s/@/%/;

  $request->param('username', $username);
}

sub ValidatePassword
{
  $request->param('password')
    || ThrowUserError("You must enter your password.");
}

sub ValidateComment
{
  $request->param('comment')
    || ThrowUserError("You must enter a check-in comment describing your changes.");
}

sub ValidateContent
{
  my $content = $request->param('content');
  # The HTML spec tells browsers to remove line breaks (carriage return
  # and newline characters) from form values in some cases.  This happens
  # even if those characters are encoded into entities (&#10; and &#13;).
  # To prevent corruption of file content in this case, we have to escape
  # those characters as \r and \n, so here we convert them back.
  # 
  # An escaped line break is the literal string "\r" or "\n" with zero
  # or an even number of slashes before it, because if there are an odd
  # number of slashes before it, then the total number of slashes is even,
  # so the slashes are actually escaped slashes followed by a literal
  # "r" or "n" character, rather than an escaped line break character.
  # In other words, "\n" and "\\\n" are a line break and a slash followed
  # by a line break, respectively, but "\\n" is a slash followed by
  # a literal "n".
  #
  $content =~ s/(^|[^\\])((\\\\)*)\\r/$1$2\r/g;
  $content =~ s/(^|[^\\])((\\\\)*)\\n/$1$2\n/g;
  $content =~ s/\\\\/\\/g;

  $request->param('content', $content);
}

sub ValidateID()
{
  my ($id) = @_;
  $id =~ m/(\d+)/;
  $id = $1;
  $id || ThrowCodeError("$id is not a valid patch ID");
  return $id;
}

sub ValidateVersions()
{
  my ($oldversion, $newversion) = @_;

  # Throw an error if the version of the file that was edited does not match
  # the version in the repository.  In the future we should try to merge
  # the user's changes if possible.
  if ($oldversion && $newversion && $oldversion != $newversion) 
  {
    ThrowCodeError("You edited version <em>$oldversion</em> of the file,
      but version <em>$newversion</em> is in the repository.  Reload the edit 
      page and make your changes again (and bother the authors of this script 
      to implement change merging
      (<a href=\"http://bugzilla.mozilla.org/show_bug.cgi?id=164342\">bug 164342</a>).");
  }
}


################################################################################
# CVS Glue
################################################################################

sub RetrieveFile
{
  my ($name) = @_;
  
  my @args = ("-d",
              ":pserver:$READ_CVS_USERNAME:$READ_CVS_PASSWORD\@$READ_CVS_SERVER", 
              "checkout",
              "-p", # write to STDOUT
              $name);
  
  # Check out the file from the CVS repository, capturing the file content
  # and any errors/notices.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;
  
  # If the CVS server returned an error, stop further processing and notify
  # the user.  The only error we don't stop for is a "not found" error, 
  # since in that case we want to give the user the option to create a new 
  # file at this location.
  if ($error_code != 0 && $errors !~ /cannot find/) 
  {
    # Include the command in the error message (but hide the username/password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    $errors =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    
    ThrowUserError("Doctor could not check out the file from the repository.",
                   undef,
                   $command,
                   $error_code,
                   $errors);
  }
  
  my $version = "";
  my $line_endings = "unix";
  
  # If the file doesn't exist, hack the version string to reflect that.
  if ($error_code != 0 && $errors =~ /cannot find/) { $version = "new" }
  
  # Otherwise, try to determine the file's version and line-ending style 
  # (unix, mac, or windows) from the "error" messages and content.
  else {
    if ($errors =~ /VERS:\s([0-9.]+)\s/) { $version = $1 }
    
    if ($output =~ /\r\n/s) { $line_endings = "windows" }
    elsif ($output =~ /\r[^\n]/s) { $line_endings = "mac" }
  }
  
  return ($output, $version, $line_endings);
}

sub CheckOutFile
{
  # Checks out a file from the repository.
  
  my ($file) = @_;
  
  my @args = ("-t", # debugging messages from which to extract the version
              "-d", 
              ":pserver:$READ_CVS_USERNAME:$READ_CVS_PASSWORD\@$READ_CVS_SERVER", 
              "checkout", 
              $file);
  
  # Check out the file from the repository, capturing the output of the
  # command and any error messages/notices.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;
  
  if ($error_code != 0) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    $errors =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    
    ThrowUserError("Doctor could not check out the file from the repository.",
                   undef,
                   $command,
                   undef,
                   $errors);
  }

  # Extract the file version from the errors/notices.
  my $version = "";
  if ($errors =~ /\Q$file\E,v,\s([0-9.]+),/) { $version = $1 }
  
  return $version;
}

sub DiffFile
{
  # Returns the results of diffing a file against the version in the repository.
  
  my ($file) = @_;
  
  my @args = ("-d", 
              ":pserver:$READ_CVS_USERNAME:$READ_CVS_PASSWORD\@$READ_CVS_SERVER", 
              "diff", 
              "-u", 
              $file);
  
  # Diff the file against the version in the repository, capturing the diff
  # and any error messages/notices.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;
  
  # Check the error messages/notices in addition to the error code 
  # returned by the system call, because for some reason the call 
  # sometimes returns a non-zero error code (256) upon success.
  if ($error_code && $errors) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    $errors =~ s/\Q$READ_CVS_PASSWORD\E/[password]/g;
    
    ThrowUserError("Doctor could not diff your version of the file 
                    against the version in the repository.",
                   undef,
                   $command,
                   undef,
                   $errors);
  }
  
  $output || ThrowUserError("You didn't change anything!");
  
  return $output;
}

sub CheckInFile
{
  # Checks a file into the repository.
  
  my ($file) = @_;
  
  my $username = $request->param('username');
  my $password = $request->param('password');
  
  my $comment = $request->param('comment');
  my @args = ("-d" , 
              ":pserver:$username:$password\@$WRITE_CVS_SERVER" , 
              "commit" , 
              "-m" , 
              $comment , 
              $file);
  
  # Check the file into the repository and capture the results.
  my $CAPTURE_ERR = IO::Capture->new(\*STDERR);
  my $CAPTURE_OUT = IO::Capture->new(\*STDOUT);
  my $error_code = system("cvs", @args);
  my $output = $CAPTURE_OUT->capture;
  my $errors = $CAPTURE_ERR->capture;
  
  if ($error_code != 0) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$password\E/[password]/g;
    $errors =~ s/\Q$password\E/[password]/g;
    
    ThrowUserError("Doctor could not check the file into the repository.",
                   undef,
                   $command,
                   $error_code,
                   $errors);
  }
  
  return $output;
}

################################################################################
# Error Handling
################################################################################

sub ThrowUserError
{
  # Throw an error about a problem with the user's request.  This function
  # should avoid mentioning system problems displaying the error message, since
  # the user isn't going to care about them and probably doesn't need to deal
  # with them after fixing their own mistake.  Errors should be gentle on 
  # the user, since many "user" errors are caused by bad UI that trip them up.
  
  # !!! Mail code errors to the system administrator!
  
  ($vars->{'message'}, 
   $vars->{'title'}, 
   $vars->{'cvs_command'}, 
   $vars->{'cvs_error_code'}, 
   $vars->{'cvs_error_message'}) = @_;
  
  # Return to the original working directory and delete the temporary
  # working directory (if any; generally all user errors should be caught
  # before we do anything requiring the creation of a temporary directory).
  chdir($HOME);
  rmtree("$TEMP_DIR/$PID", 0, 1) if -e "$TEMP_DIR/$PID";
  
  print $request->header;
  $template->process("user-error.tmpl", $vars)
    || print( ($vars->{'title'} ? "<h1>$vars->{'title'}</h1>" : "") . 
              "<p>$vars->{'message'}</p><p>Please go back and try again.</p>" );
  exit;
}

sub ThrowCodeError
{
  # Throw error about a problem with the code.  This function should be
  # apologetic and deferent to the user, since it isn't the user's fault
  # the code didn't work.
  
  # !!! Mail code errors to the system administrator!
  
  ($vars->{'message'}, $vars->{'title'}) = @_;
  
  # Return to the original working directory and delete the temporary
  # working directory.
  (chdir($HOME) && (-e "$TEMP_DIR/$PID" ? rmtree("$TEMP_DIR/$PID", 0, 1) : 1))
    || ($vars->{'message'} = 
         ("couldn't return to original working directory '$HOME' " . 
          "and delete temporary working directory '$TEMP_DIR/$PID': $!" . 
          "; error occurred while trying to display error message: " . 
          ($vars->{'title'} ? "$vars->{'title'}: ": "") . $vars->{'message'}));
  
  print $request->header;
  $template->process("code-error.tmpl", $vars)
    || print("
         <p>
         Unfortunately Doctor has experienced an internal error from which
         it was unable to recover.  More information about the error is
         provided below. Please forward this information along with any
         other information that would help diagnose and fix this problem
         to the system administrator at
         <a href=\"mailto:$CONFIG->{'ADMIN_EMAIL'}\">$CONFIG->{'ADMIN_EMAIL'}</a>.
         </p>
         <p>
         couldn't process error.tmpl template: " . $template->error() . 
         "; error occurred while trying to display error message: " . 
         ($vars->{'title'} ? "$vars->{'title'}: ": "") . $vars->{'message'} . 
         "</p>");
  exit;
}

################################################################################
# File and Directory Manipulation
################################################################################

sub ReplaceFile
{
  # Replaces the file checked out from the repository with the edited version.
  
  my ($file) = @_;
  
  my $content = GetUploadedContent() || $request->param('content');
  my $line_endings = $request->param('line_endings');

  # Replace the Windows-style line endings in which browsers send content
  # with line endings appropriate to the file being replaced if the file
  # has Unix- or Mac-style line endings.
  if ($line_endings eq "unix") { $content =~ s/\r\n/\n/g }
  if ($line_endings eq "mac") { $content =~ s/\r\n/\r/g }
  
  open(DOC, ">$file")
    || ThrowCodeError("I could not open the temporary file <em>$file</em> 
                       for writing: $!");
  print DOC $content;
  close(DOC);
}

sub GetUploadedContent
{
  my $fh = $request->upload('content_file');

  # enable 'slurp' mode
  local $/;

  if ($fh) {
    my $content = <$fh>;
    return $content;
  }
}

sub CreateTempDir
{
  # Creates and changes to a temporary sub-directory unique to this process
  # that can be used for CVS activities requiring the manipulation of files.
  
  if (!-e $TEMP_DIR and !mkdir($TEMP_DIR))
  {
    ThrowCodeError("couldn't create the temporary directory 
                    <em>$TEMP_DIR</em>: $!");
  }
  elsif (!-d $TEMP_DIR)
  {
    ThrowCodeError("couldn't use the temporary directory <em>$TEMP_DIR</em> 
                    because it isn't a directory.");
  }
  
  # Create and change to a unique sub-directory in the temporary directory.
  mkdir("$TEMP_DIR/$PID") 
    || ThrowCodeError("couldn't create a temporary sub-directory 
                       <em>$TEMP_DIR/$PID</em>: $!");
  
  chdir("$TEMP_DIR/$PID") 
    || ThrowCodeError("couldn't change to the temporary sub-directory 
                       <em>$TEMP_DIR/$PID</em>: $!");
}

sub DeleteTempDir
{
  # Changes back to the home directory and deletes the temporary directory.
  
  chdir($HOME);
  rmtree("$TEMP_DIR/$PID", 0, 1)
    || ThrowCodeError("couldn't delete the temporary sub-directory 
                       <em>$TEMP_DIR/$PID</em>: $!");
}
