#!/usr/bin/perl -I.. -w

package Doctor::File;

use strict;

use Cwd qw(getcwd chdir);

use Doctor qw(:DEFAULT %CONFIG $template);

use File::Temp qw(tempfile);

# doesn't import "diff" to prevent conflicts with our own
use Text::Diff ();

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;

    my $spec = shift;

    my $self  = {};
    $self->{_content} = undef;
    $self->{_version} = undef;
    $self->{_spec} = undef;
    $self->{_tempdir} = undef;

    bless($self, $class);

    if (defined $spec) { $self->spec($spec) }

    return $self;
}

sub content {
    my $self = shift;
    if (!$self->{_content}) {
        $self->checkout();
        if (!$self->{_content}) {
            return undef;
        }
    }
    return $self->{_content};
}

sub version {
    my $self = shift;
    if (!$self->{_version}) {
        $self->checkout();
        if (!$self->{_version}) {
            return undef;
        }
    }
    return $self->{_version};
}

sub spec {
    my $self = shift;
    # The spec can only be set once.
    if (@_ && !$self->{_spec}) {
        $self->{_spec} = shift;

        # Canonicalize the spec:

        # URL -> Spec Conversion

        # Remove the absolute URI for files on the web site (if any)
        # from the beginning of the path.
        if ($CONFIG{WEB_BASE_URI_PATTERN}) {
            $self->{_spec} =~ s/^$CONFIG{WEB_BASE_URI_PATTERN}//i;
        }
        else {
            $self->{_spec} =~ s/^\Q$CONFIG{WEB_BASE_URI}\E//i;
        }

        # Entire Spec Issues

        # Collapse multiple consecutive slashes (i.e. dir//file.txt) into one.
        $self->{_spec} =~ s:/{2,}:/:;

        # Beginning of Spec Issues

        # Remove a preceding slash.
        $self->{_spec} =~ s:^/::;

        # Add the base path of the file in the CVS repository if necessary.
        # (i.e. if the user entered a URL or a path based on the URL).
        if ($self->{_spec} !~ /^\Q$CONFIG{WEB_BASE_PATH}\E/) {
            $self->{_spec} = $CONFIG{WEB_BASE_PATH} . $self->{_spec};
        }

        # End of Spec Issues

        # If the filename (the last name in the path) contains no period,
        # it is probably a directory, so add a slash.
        if ($self->{_spec} =~ m:^[^\./]+$: || $self->{_spec} =~ m:/[^\./]+$:) { $self->{_spec} .= "/" }

        # If the file ends with a forward slash, it is a directory,
        # so add the name of the default file.
        if ($self->{_spec} =~ m:/$:) { $self->{_spec} .= "index.html" }
    }
    return $self->{_spec};
}

sub tempdir {
    my $self = shift;
    $self->{_tempdir} ||=
        File::Temp->tempdir("doctor-XXXXXXXX", TMPDIR => 1, CLEANUP => 1);
    return $self->{_tempdir};
}

sub line_endings {
    my $self = shift;
    if      (!$self->content)               { return undef }
    elsif   ($self->content =~ /\r\n/s)     { return "windows" }
    elsif   ($self->content =~ /\r[^\n]/s)  { return "mac" }
    else                                    { return "unix" }
}

sub linebreak {
    my $self = shift;
    if      (!$self->content)               { return undef; }
    elsif   ($self->content =~ /\r\n/s)     { return "\r\n" }
    elsif   ($self->content =~ /\r[^\n]/s)  { return "\r" }
    else                                    { return "\n" }
}

sub url {
    my $self = shift;
        # Construct a URL to the file if possible.
    my $spec = $self->spec;
    if ($spec =~ s/^\Q$CONFIG{WEB_BASE_PATH}\E(.*)$/$1/i) {
        return $CONFIG{WEB_BASE_URI} . $spec;
    }
    else {
        return undef;
    }
}

sub name {
    my $self = shift;
    return $self->_name_or_path("name");
}

sub path {
    my $self = shift;
    return $self->_name_or_path("path");
}

sub _name_or_path {
    my $self = shift;
    my $type = shift;
    $self->spec
      || return undef;
    $self->spec =~ /^(.*[\/\\])?([^\/\\]+)$/;
    return ($type eq "path" ? $1 : ($type eq "name" ? $2 : undef));
}

sub relative_spec {
    # Returns the part of the spec which appears in the URL after the server name.
    my $self = shift;
    $self->spec
      || return undef;
    $self->spec =~ /^\Q$CONFIG{WEB_BASE_PATH}\E(.*)$/;
    return $1;
}

sub add {
    my $self = shift;
    my $content = shift;
    
    my $olddir = getcwd();
    chdir $self->tempdir;

    # Write the content to a file.
    open(FILE, ">$self->name") or die "Couldn't create $self->name: $!";
    print FILE $content;
    close(FILE);

    # Create a fake CVS directory that makes the temporary directory look like
    # a local working copy of the directory in the repository where this file
    # will be created.  This way we just have to commit the file to create it;
    # we avoid having to first check out the directory and then add the file.  
    mkdir("CVS") or die("Couldn't create CVS directory: $!");
    chdir("CVS");
  
    # Make the Entries file and add an entry for the new file.
    open(FILE, ">Entries") or die "Couldn't create CVS/Entries file: $!";
    print FILE "/$self->name/0/Initial $self->name//\nD\n";
    close(FILE);
    
    # Make the Repository file, which contains the path to the directory.
    open(FILE, ">Repository") or die "couldn't create CVS/Repository file: $!";
    print FILE "$self->path\n";
    close(FILE);
    
    # Note that we don't have to create a Root file with information about
    # the repository (authentication type, server address, root path, etc.)
    # since we add that information to the command line.
    
    chdir $olddir;
}

sub patch {
    my $self = shift;
    my $newcontent = shift;

    my $olddir = getcwd();
    chdir $self->tempdir;
    -e $self->spec or $self->checkout();
    open(FILE, ">", $self->spec) or die "couldn't open $self->spec: $!";
    print FILE $newcontent;
    close(FILE);
    chdir $olddir;
}

sub checkout {
    my $self = shift;

    my @args = ("-d", 
                ":pserver:$CONFIG{READ_CVS_USERNAME}:$CONFIG{READ_CVS_PASSWORD}\@$CONFIG{READ_CVS_SERVER}", 
                "checkout", 
                $self->spec);
  
    my $olddir = getcwd();
    chdir $self->tempdir;

    # Check out the file from the repository, capturing the output
    # of the command and any error messages.
    my ($rv, $output, $errors) = Doctor::system_capture("cvs", @args);

    # Extract the content and version.
    if ($rv == 0) {
        # Extract the version from the CVS/Entries file.
        open(FILE, "<", $self->path . "CVS/Entries")
          or die "Can't open " . $self->spec . "/CVS/Entries: $!";
        my $entry = <FILE>; # just the first line, which should be all there is
        close(FILE);
        $entry =~ m:^/[^/]*/([^/]*)/:;
        $self->{_version} = $1;

        # Extract the content from the file.  In a block to localize $/
        {
            local $/ = undef;
            open(FILE, "<", $self->spec)
              or die "Can't open " . $self->spec . ": $!";
            $self->{_content} = <FILE>;
            close(FILE);
        }
    }
    elsif ($errors =~ /cannot find/) {
        $self->{_version} = "new";
    }

    chdir $olddir;

    return ($rv, $output, $errors);
}

sub commit {
    # Commits changes to the repository.  Requires the file to already be
    # checked out, patched, and sitting in the temporary directory waiting
    # to be committed.  Dies otherwise.

    my $self = shift;
    my ($username, $password, $comment) = @_;

    # New files get added to the top-level of the temporary directory
    # rather than the appropriate subdirectory (we just hack CVS/Repository
    # to make it look like the appropriate subdirectory; see the add() function
    # for the details), so if the file is new we set the spec to the name
    # of the file instead of its full spec.
    my $spec = $self->version eq "new" ? $self->name : $self->spec;

    my @args = ("-d", 
                ":pserver:$username:$password\@$CONFIG{WRITE_CVS_SERVER}", 
                "commit", 
                "-m", 
                $comment, 
                $spec);
  
    # Check the file into the repository and capture the results.
    my $olddir = getcwd();
    chdir $self->tempdir;
    -e $spec or die "couldn't find $spec: $!";
    my ($rv, $output, $errors) = Doctor::system_capture("cvs", @args);
    chdir $olddir;
  
    return ($rv, $output, $errors);
}

sub diff {
    my $self = shift;
    my $revision = shift;
    if (!$revision) { die("no revision to diff") }

    # Convert line breaks to whatever the file in CVS uses.
    my $linebreak = $self->linebreak;
    $revision =~ s/\r\n|\r|\n/$linebreak/g;

    return Text::Diff::diff \$self->content, \$revision;
}

1;  # so the require or use succeeds
