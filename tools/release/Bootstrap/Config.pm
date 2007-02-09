#
# Config object for release automation
#

package Bootstrap::Config;

use strict;

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

sub Get {
    my $this = shift;

    my %args = @_;
    my $var = $args{'var'};

    if ($config{$var}) {
        return $config{$var};
    } else {
        die("No such config variable: $var\n");
    }
}

sub Exists {
    my $this = shift;
    my %args = @_;

    my $var = $args{'var'};
    return exists($config{$var});
}

1;
