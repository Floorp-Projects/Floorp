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
    PRE_CHOMP => 1,
    POST_CHOMP => 1,

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
    CASE    => 1,
    CREATE  => 1 , 
    GLOBAL  => {
      ARGCOUNT => ARGCOUNT_ONE , 
    }
  }
);
$config->file("doctor.conf");
my %CONFIG = $config->varlist(".*");
$vars->{'config'} = \%CONFIG;

# Store the home directory so we can get back to it after changing directories
# in certain places in the code.
use Cwd;
my $HOME = cwd;

################################################################################
# Main Body Execution
################################################################################

# All calls to this script should contain an "action" variable whose value
# determines what the user wants to do.  The code below checks the value 
# of that variable and runs the appropriate code.

my $action = lc($request->param('action'));
if (!$action) {
  $action = $request->param('file') ? "edit" : "choose";
}

# Displays a form for choosing the page you want to edit.
if    ($action eq "choose")       { choose()      }

# Displays a page with UI for editing the page online, downloading it
# for local editing, viewing the original and the user's modified versions,
# getting a diff of the user's changes, and committing the user's changes
# or submitting them to the editors for review.  This is the principle
# interface to Doctor functionality, and many other actions are called
# from links, forms, and JavaScript on this page.
elsif ($action eq "edit")         { edit()        }

# Retrieves a page from CVS and returns it.  Called by the "download the page"
# link on the Edit page for users who want to download and edit pages locally
# instead of via the embedded textarea.  Also called by the View Original panel
# of the Edit page to show the original version of the page being edited.
elsif ($action eq "download" || $action eq "display")
                                  { retrieve()    }

# Generates and returns a diff of changes between the submitted version
# of a page and the version in CVS.  Called by the Show Diff panel of the Edit page.
elsif ($action eq "diff")         { diff()        }

# Returns the content that was submitted to it.  Useful for displaying
# the modified version of a page the user downloaded and edited locally,
# since for security reasons there's no way to get access to a file
# in a file upload control on the client side.  Called by JavaScript
# when the user focuses the View Edited panel on the Edit page after entering
# a filename into the file upload control.
elsif ($action eq "regurgitate")  { regurgitate() }

# Submits a change to the editors for review.  Requires the EDITOR_EMAIL config
# parameter to be set to the editors' email addresses.
elsif ($action eq "queue")        { queue()       }

# Adds and commits a new page to the repository.
elsif ($action eq "create")       { create()      }

# Commits changes to an existing page to the repository.
elsif ($action eq "commit")       { commit()      }

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
  ValidateFile();
 
  my $file = $request->param('file');
  
  # Remove the base path from the beginning of the file to save space
  # when displaying it.
  $file =~ /^\Q$CONFIG{WEB_BASE_PATH}\E(.*)$/;
  $vars->{'short_file'} = "/$1";
  $vars->{'file'} = $file;

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

  print $request->header;
  $template->process("edit.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub retrieve
{
  ValidateFile();
 
  my $file = $request->param('file');
  
  # Separate the name of the file from its path.
  $file =~ /^(.*)\/([^\/]+)$/;
  my $path = $1;
  my $filename = $2;

  my ($content) = RetrieveFile($file);
  my $filesize = length($content);

  my $disposition = $action eq "download" ? "attachment" : "inline";
    
  print $request->header(-type=>"text/html; name=\"$filename\"",
                         -content_disposition=> "$disposition; filename=\"$filename\"",
                         -content_length => $filesize);
  print $content;
}

sub diff
{
  # Displays a diff between the version of a file in the repository
  # and the version submitted by the user so the user can review the
  # changes they are making to the file.  Offers the user the option
  # to either commit their changes or edit the file some more.
  
  ValidateFile();
  
  ChangeToTempDir();
  
  # Check out the file from the repository.
  my $file = $request->param('file');
  my $oldversion = $request->param('version');
  my $newversion = CheckOutFile($file);
  
  ValidateVersions($oldversion, $newversion);
  
  # Replace the checked out file with the edited version, generate
  # a diff between the edited file, and display it to the user.
  ReplaceFile($file);
  $vars->{'diff'} = DiffFile($file);
  
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

sub regurgitate
{
  # Returns the content that was submitted to it.  Useful for displaying
  # the modified version of a document the user downloaded and edited locally,
  # since for security reasons there's no way to get access to the file
  # until the user uploads it to the server.  When the user clicks
  # on the "modified" tab, client-side JS checks to see if there's a value
  # in the file upload control, and if so it adds an iframe to the "modified"
  # panel and posts the file to it with action=regurgitate, which then returns
  # the content of the file to the user, who can then see his changes.

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

sub queue
{
  # Sends the diff or new file to an editors mailing list for review.
  
  ThrowCodeError("The administrator has not enabled submission of patches for review.",
                 "Review Not Enabled")
    unless $CONFIG{EDITOR_EMAIL};

  ValidateFile();
  ValidateContent();
  # XXX validate the version as well?
  # XXX validate the user's email address
  my $email = $request->param('email');
  
  my $file = $request->param('file');
  my $comment = $request->param('comment');

  my ($patch, $oldversion, $newversion);
  if ($request->param('is_new')) {
    $patch = $request->param('content');
    $oldversion = "-new";
  }
  else {
    ChangeToTempDir();
    
    # Check out the file from the repository.
    $oldversion = $request->param('version');
    $newversion = CheckOutFile($file);
    
    ValidateVersions();
    
    # Replace the checked out file with the edited version, and generate a patch
    # that patches the checked out file to make it look like the edited version.
    ReplaceFile($file);
    $patch = DiffFile($file);
  }

  eval {
    use MIME::Entity;
    my $mail = MIME::Entity->build(Type     =>"multipart/mixed",
                                   From     => $email,
                                   To       => $CONFIG{EDITOR_EMAIL},
                                   Subject  => "patch: $file v$oldversion");
    $mail->attach(Data        =>$comment,
                  Encoding    => "quoted-printable");
    $mail->attach(Data        => $patch,
                  Encoding    => "quoted-printable",
                  Disposition => "inline",
                  Filename    => "patch.txt");
    $mail->send();
  };
  if ($@) {
    ThrowCodeError($@, "Mail Failure");
  }

  print $request->header;
  $template->process("queued.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
}

sub create
{
  # Creates a new file in the repository.
  
  ValidateFile();
  ValidateContent();
  ValidateUsername();
  ValidatePassword();
  ValidateComment();
  
  ChangeToTempDir();
  
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
  
  $vars->{'file'} = $request->param('file');
  
  print $request->header;
  $template->process("created.tmpl", $vars)
    || ThrowCodeError($template->error(), "Template Processing Failed");
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
  
  ChangeToTempDir();
  
  # Check out the file from the repository.
  my $file = $request->param('file');
  my $oldversion = $request->param('version');
  my $newversion = CheckOutFile($file);
  
  ValidateVersions();
  
  # Replace the checked out file with the edited version, generate
  # a diff between the edited file and the version in the repository,
  # and check in the edited file.
  ReplaceFile($file);
  $vars->{'diff'} = DiffFile($file);
  $vars->{'checkin_results'} = CheckInFile($file);
  
  $vars->{'file'} = $file;
  
  print $request->header;
  $template->process("committed.tmpl", $vars)
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
  if ($CONFIG{WEB_BASE_URI_PATTERN}) { $file =~ s/^$CONFIG{WEB_BASE_URI_PATTERN}//i }
  else                       { $file =~ s/^\Q$CONFIG{WEB_BASE_URI}\E//i     }


  # Entire Path Issues

  # Collapse multiple consecutive slashes (i.e. dir//file.txt) into a single slash.
  $file =~ s:/{2,}:/:;


  # Beginning of Path Issues

  # Remove a preceding slash.
  $file =~ s:^/::;

  # Add the base path of the file in the cvs repository if necessary.
  # (i.e. if the user entered a URL or a path based on the URL).
  if ($file !~ /^\Q$CONFIG{WEB_BASE_PATH}\E/) { $file = $CONFIG{WEB_BASE_PATH} . $file }


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
  if ($url =~ s/^\Q$CONFIG{WEB_BASE_PATH}\E(.*)$/$1/i) { $vars->{'file_url'} = $CONFIG{WEB_BASE_URI} . $url }

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
  # Throws an error if the version of the file that was edited
  # does not match the version in the repository.  In the future
  # we should try to merge the user's changes if possible.

  my ($oldversion, $newversion) = @_;

  if ($oldversion && $newversion && $oldversion != $newversion) {
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
              ":pserver:$CONFIG{READ_CVS_USERNAME}:$CONFIG{READ_CVS_PASSWORD}\@$CONFIG{READ_CVS_SERVER}", 
              "checkout",
              "-p", # write to STDOUT
              $name);
  
  # Check out the file from the CVS repository, capturing the file content
  # and any errors/notices.
  my ($error_code, $output, $errors) = system_capture("cvs", @args);
  
  # If the CVS server returned an error, stop further processing and notify
  # the user.  The only error we don't stop for is a "not found" error, 
  # since in that case we want to give the user the option to create a new 
  # file at this location.
  if ($error_code != 0 && $errors !~ /cannot find/) 
  {
    # Include the command in the error message (but hide the username/password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$CONFIG{READ_CVS_PASSWORD}\E/[password]/g;
    $errors =~ s/\Q$CONFIG{READ_CVS_PASSWORD}\E/[password]/g;
    
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
              ":pserver:$CONFIG{READ_CVS_USERNAME}:$CONFIG{READ_CVS_PASSWORD}\@$CONFIG{READ_CVS_SERVER}", 
              "checkout", 
              $file);
  
  # Check out the file from the repository, capturing the output of the
  # command and any error messages.
  my ($error_code, $output, $errors) = system_capture("cvs", @args);

  if ($error_code != 0) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$CONFIG{READ_CVS_PASSWORD}\E/[password]/g;
    $errors =~ s/\Q$CONFIG{READ_CVS_PASSWORD}\E/[password]/g;
    
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
              ":pserver:$CONFIG{READ_CVS_USERNAME}:$CONFIG{READ_CVS_PASSWORD}\@$CONFIG{READ_CVS_SERVER}", 
              "diff", 
              "-u", 
              $file);
  
  # Diff the file against the version in the repository, capturing the diff
  # and any error messages/notices.
  my ($error_code, $output, $errors) = system_capture("cvs", @args);
  
  # Check the error messages/notices in addition to the error code 
  # returned by the system call, because for some reason the call 
  # sometimes returns a non-zero error code (256) upon success.
  if ($error_code && $errors) 
  {
    # Include the command in the error message (but hide the password).
    my $command = join(" ", "cvs", @args);
    $command =~ s/\Q$CONFIG{READ_CVS_PASSWORD}\E/[password]/g;
    $errors =~ s/\Q$CONFIG{READ_CVS_PASSWORD}\E/[password]/g;
    
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
              ":pserver:$username:$password\@$CONFIG{WRITE_CVS_SERVER}" , 
              "commit" , 
              "-m" , 
              $comment , 
              $file);
  
  # Check the file into the repository and capture the results.
  my ($error_code, $output, $errors) = system_capture("cvs", @args);
  
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
  
  chdir($HOME);
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
  
  chdir($HOME);
  print $request->header;
  $template->process("code-error.tmpl", $vars)
    || print("
         <p>
         Unfortunately Doctor has experienced an internal error from which
         it was unable to recover.  More information about the error is
         provided below. Please forward this information along with any
         other information that would help diagnose and fix this problem
         to the system administrator at
         <a href=\"mailto:$CONFIG{ADMIN_EMAIL}\">$CONFIG{ADMIN_EMAIL}</a>.
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

sub ChangeToTempDir
{
  my $dir = tempdir("doctor-XXXXXXXX", TMPDIR => 1, CLEANUP => 1);
  chdir $dir;
}

sub system_capture {
  # Runs a command and captures its output and errors.  This should be using
  # in-memory files, but they require that we close STDOUT and STDERR
  # before reopening them on the in-memory files, and closing and reopening
  # STDERR causes CVS to choke with return value 256.

  my ($command, @args) = @_;

  my ($rv, $output, $errors);

  # Back up the original STDOUT and STDERR so we can restore them later.
  open my $oldout, ">&STDOUT"     or die "Can't back up STDOUT to \$oldout: $!";
  open OLDERR,     ">&", \*STDERR or die "Can't back up STDERR to OLDERR: $!";
  use vars qw( $OLDERR ); # suppress "used only once" warnings

  # Close and reopen STDOUT and STDERR to in-memory files, which are just
  # scalars that take output and append it to their value.
  # XXX Disabled in-memory files in favor of temp files until in-memory issues
  # can be worked out.
  #close STDOUT;
  #close STDERR;
  #open STDOUT, ">", \$output or die "Can't open STDOUT to output var: $!";
  #open STDERR, ">", \$errors or die "Can't open STDERR to errors var: $!";
  my $outtmpfile = tempfile();
  my $errtmpfile = tempfile();
  open STDOUT, ">&", $outtmpfile or die "Can't dupe STDOUT to output cache: $!";
  open STDERR, ">&", $errtmpfile or die "Can't dupe STDERR to errors cache: $!";

  # Run the command.
  $rv = system($command, @args);

  # Restore original STDOUT and STDERR.
  close STDOUT;
  close STDERR;
  open STDOUT, ">&", $oldout or die "Can't restore STDOUT from \$oldout: $!";
  open STDERR, ">&OLDERR"    or die "Can't restore STDERR from OLDERR: $!";

  # Grab output and errors from the caches.
  # XXX None of this would be necessary if in-memory files was working.
  undef $/;
  seek($outtmpfile, 0, 0);
  seek($errtmpfile, 0, 0);
  $output = <$outtmpfile>;
  $errors = <$errtmpfile>;

  return ($rv, $output, $errors);
}
