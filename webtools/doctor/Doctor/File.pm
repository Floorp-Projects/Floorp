#!/usr/bin/perl -I.. -w

package Doctor::File;

use strict;

use Doctor qw(:DEFAULT %CONFIG $template);

use File::Temp qw(tempfile tempdir);

# doesn't import "diff" to prevent conflicts with our own
# ??? perhaps that means we should inherit?
use Text::Diff ();

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;

    my $spec = shift;

    my $self  = {};
    $self->{_content} = undef;
    $self->{_version} = undef;
    $self->{_spec} = undef;

    bless($self, $class);

    if (defined $spec) { $self->spec($spec) }

    return $self;
}

sub content {
    my $self = shift;
    if (!$self->{_content}) {
        $self->retrieve();
        if (!$self->{_content}) {
            return undef;
        }
    }
    return $self->{_content};
}

sub version {
    my $self = shift;
    if (!$self->{_version}) {
        $self->retrieve();
        if (!$self->{_version}) {
            return undef;
        }
    }
    return $self->{_version};
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

sub spec {
    my $self = shift;
    # The spec can only be set once.
    if (@_ && !$self->{_spec}) {
        $self->{_spec} = shift;
        $self->_normalize();
    }
    return $self->{_spec};
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

# XXX This should just be part of the spec getter/setter.
sub _normalize {
    my $self = shift;

    # Make sure a path was entered.
    $self->{_spec}
      || die("empty file spec");

    # URL -> Path Conversion
  
    # Remove the absolute URI for files on the web site (if any)
    # from the beginning of the path.
    if ($CONFIG{WEB_BASE_URI_PATTERN}) {
        $self->{_spec} =~ s/^$CONFIG{WEB_BASE_URI_PATTERN}//i;
    }
    else {
        $self->{_spec} =~ s/^\Q$CONFIG{WEB_BASE_URI}\E//i;
    }
  
  
    # Entire Path Issues
  
    # Collapse multiple consecutive slashes (i.e. dir//file.txt) into one.
    $self->{_spec} =~ s:/{2,}:/:;
  
  
    # Beginning of Path Issues
  
    # Remove a preceding slash.
    $self->{_spec} =~ s:^/::;
  
    # Add the base path of the file in the CVS repository if necessary.
    # (i.e. if the user entered a URL or a path based on the URL).
    if ($self->{_spec} !~ /^\Q$CONFIG{WEB_BASE_PATH}\E/) {
        $self->{_spec} = $CONFIG{WEB_BASE_PATH} . $self->{_spec};
    }
  
  
    # End of Path Issues
  
    # If the filename (the last name in the path) contains no period,
    # it is probably a directory, so add a slash.
    if ($self->{_spec} =~ m:^[^\./]+$: || $self->{_spec} =~ m:/[^\./]+$:) { $self->{_spec} .= "/" }
  
    # If the file ends with a forward slash, it is a directory,
    # so add the name of the default file.
    if ($self->{_spec} =~ m:/$:) { $self->{_spec} .= "index.html" }
}

sub retrieve {
    my $self = shift;

    return if $self->{_content} && $self->{_version};

    my @args = ("-d",
                ":pserver:$CONFIG{READ_CVS_USERNAME}:$CONFIG{READ_CVS_PASSWORD}\@$CONFIG{READ_CVS_SERVER}", 
                "checkout",
                "-p", # write to STDOUT
                $self->spec);
    
    # Check out the file from the CVS repository, capturing the file content
    # and any errors/notices.
    my ($error_code, $output, $errors) = Doctor::system_capture("cvs", @args);
    
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
    
    # If the file doesn't exist, hack the version string to reflect that.
    if ($error_code != 0 && $errors =~ /cannot find/) {
        $self->{_version} = "new";
    }
    # Otherwise, try to determine the file's version from the errors.
    else {
        if ($errors =~ /VERS:\s([0-9.]+)\s/) { $self->{_version} = $1 }
    }
    $self->{_content} = $output;
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
