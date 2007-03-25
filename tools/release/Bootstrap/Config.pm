#
# Config object for release automation
#

package Bootstrap::Config;

use strict;
use POSIX "uname";

# shared static config
my %config;

# single shared instance
my $singleton = undef;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;

    return $singleton if defined $singleton;

    my $this = {};
    bless($this, $class);
    $this->Parse();
    $singleton = $this;
    
    return $this;
}

sub Parse {
    my $this = shift;
    
    open (CONFIG, "< bootstrap.cfg") 
      || die("Can't open config file bootstrap.cfg");

    while (<CONFIG>) {
        chomp; # no newline
        s/#.*//; # no comments
        s/^\s+//; # no leading white
        s/\s+$//; # no trailing white
        next unless length; # anything left?
        my ($var, $value) = split(/\s*=\s*/, $_, 2);
        $config{$var} = $value;
    }
    close CONFIG;
}

##
# Get checks to see if a variable exists and returns it.
# Returns scalar
#
# This method supports system-specific overrides, or "sysvar"s.
#  For example, if a caller is on win32 and does 
#  Get(sysvar => "buildDir") and win32_buildDir exists, the value of 
#  win32_buildDir will be returned. If not, the value of buildDir will
#  be returned. Otherwise, the die() assertion will be hit.
##

sub Get {
    my $this = shift;

    my %args = @_;
    my $var = $args{'var'};
    # sysvar will attempt to prepend the OS name to the requested var
    my $sysvar = $args{'sysvar'};

    if ((! defined($args{'var'})) && (! defined($args{'sysvar'}))) {
      die "ASSERT: Bootstep::Config::Get(): null var requested";
    } elsif ((defined($args{'var'})) && (defined($args{'sysvar'}))) {
      die "ASSERT: Bootstep::Config::Get(): both var and sysvar requested";
    }

    if (defined($args{'sysvar'})) {
        # look for a system specific var first
        my $osname = $this->SystemInfo(var => 'osname');
        my $sysvarOverride = $osname . '_' . $sysvar;

        if ($this->Exists(var => $sysvarOverride)) {
            return $config{$sysvarOverride};
        } elsif ($this->Exists(var => $sysvar)) {
            return $config{$sysvar};
        } else {
            die("No such system config variable: $sysvar");
        }
    } elsif ($this->Exists(var => $var)) {
        return $config{$var};
    } else {
        die("No such config variable: $var");
    }
}

##
# Exists checks to see if a config variable exists.
# Returns boolean (1 or 0)
#
# This method supports system-specific overrides, or "sysvar"s.
#  For example, if a caller is on win32 and does 
#  Exists(sysvar => "win32_buildDir") and only buildDir exists, a 0
#  will be returned. There is no "fallback" as in the case of Get.
##

sub Exists {
    my $this = shift;
    my %args = @_;
    my $var = $args{'var'};
    # sysvar will attempt to prepend the OS name to the requested var
    my $sysvar = $args{'sysvar'};

    if ((! defined($args{'var'})) && (! defined($args{'sysvar'}))) {
      die "ASSERT: Bootstep::Config::Get(): null var requested";
    } elsif ((defined($args{'var'})) && (defined($args{'sysvar'}))) {
      die "ASSERT: Bootstep::Config::Get(): both var and sysvar requested";
    }

    if (defined($args{'sysvar'})) {
        # look for a system specific var first
        my $osname = $this->SystemInfo(var => 'osname');
        my $sysvarOverride = $osname . '_' . $sysvar;

        if (exists($config{$sysvarOverride})) {
            return 1;
        } elsif (exists($config{$sysvar})) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return exists($config{$var});
    }
}

sub SystemInfo {
    my $this = shift;
    my %args = @_;

    my $var = $args{'var'};

    my ($sysname, $hostname, $release, $version, $machine ) = uname;

    if ($var eq 'sysname') {
        return $sysname;
    } elsif ($var eq 'hostname') {
        return $hostname;
    } elsif ($var eq 'release') {
        return $release;
    } elsif ($var eq 'version') {
        return $version;
    } elsif ($var eq 'machine') {
        return $machine;
    } elsif ($var eq 'osname') {
        if ($sysname =~ /cygwin/i) {
            return 'win32';
        } elsif ($sysname =~ /darwin/i) {
            return 'macosx';
        } elsif ($sysname =~ /linux/i) {
            return 'linux';
        } else {
            die("Unrecognized OS: $sysname");
        }
    } else {
        die("No system info named $var");
    }
}

1;
