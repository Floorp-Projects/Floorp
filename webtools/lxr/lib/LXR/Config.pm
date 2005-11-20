# $Id: Config.pm,v 1.6 2005/11/20 22:10:40 timeless%mozdev.org Exp $

package LXR::Config;

use LXR::Common;

require Exporter;
@ISA = qw(Exporter);
# @EXPORT = '';

$confname = 'lxr.conf';


sub new {
    my ($class, @parms) = @_;
    my $self = {};
    bless($self);
    $self->_initialize(@parms);
    return(treeify($self));
}

sub treeify {
    my ($self) = @_;

    #If there are multiple definitions of sourceroot in lxr.conf then
    #this installation is configured for multiple trees. For a single
    #tree "sourceroot" is a single directory where the source can be
    #found. If the file contains multiple definitions of sourceroot then
    #each definition is a tree,directory pair.

    #remove the extra space that i stupidly added when parsing lxr.conf
    $self->{'sourceroot'} =~ s/^\s+//;;

    if ($self->{'sourceroot'} =~ /\S\s+\S/) {
        $self->{'oldroot'} = $self->{'sourceroot'};

        #since there's whitespace within the root directory definition
        #there is one or more tree defined. (Using directory names with
        #embedded spaces here would be a bad thing.)
        my %treehash = split(/\s+/, $self->{'sourceroot'});

        #To compute which tree we're looking at, grab the second to last
        #component from the script name which will be of the form: 
        # /seamonkey/source
        $self->{'treename'} = $ENV{'SCRIPT_NAME'};
        $self->{'treename'} =~ s/.*\/([^\/]+)\/[\w]*/$1/;

        #Match the tree name against our list of trees and extract the proper
        #directory. Set "sourceroot" to this directory.
        $self->{'sourceroot'} = $treehash{$self->{'treename'}};

        #set srcrootname to tree name
        $self->{'sourcerootname'} = $self->{'treename'};

        #append tree name to virtroot
        $self->{'virtroot'} =  $self->{'virtroot'} . "/" . $self->{'treename'} ;

        #append tree name to baserul
        $self->{'baseurl'} =  $self->{'baseurl'} . $self->{'treename'};

        #append tree name to dbdir
        $self->{'dbdir'} =  $self->{'dbdir'} . "/" . $self->{'treename'} ;

        #find the cvsroot to sed in proper bonsai url
        my $path = $self->{'sourceroot'};
        my @pathdirs = split(/\//, $path);
        my $pathnum = @pathdirs;
        $self->{'bonsaicvsroot'} = $pathdirs[$pathnum - 1]; 

    }
    return($self);
}

sub makevalueset {
    my $val = shift;
    my @valset;

    if ($val =~ /^\s*\(([^\)]*)\)/) {
	@valset = split(/\s*,\s*/,$1);
    } elsif ($val =~ /^\s*\[\s*(\S*)\s*\]/) {
	if (open(VALUESET, "$1")) {
	    $val = join('',<VALUESET>);
	    close(VALUESET);
	    @valset = split("\n",$val);
	} else {
	    @valset = ();
	}
    } else {
	@valset = ();
    }
    return(@valset);
}


sub parseconf {
    my $line = shift;
    my @items = ();
    my $item;

    foreach $item ($line =~ /\s*(\[.*?\]|\(.*?\)|\".*?\"|\S+)\s*(?:$|,)/g) {
	if ($item =~ /^\[\s*(.*?)\s*\]/) {
	    if (open(LISTF, "$1")) {
		$item = '('.join(',',<LISTF>).')';
		close(LISTF);
	    } else {
		$item = '';
	    }
	}
	if ($item =~ s/^\((.*)\)/$1/s) {
	    $item = join("\0",($item =~ /\s*(\S+)\s*(?:$|,)/gs));
	}
	$item =~ s/^\"(.*)\"/$1/;

	push(@items, $item);
    }
    return(@items);
}


sub _initialize {
    my ($self, $conf) = @_;
    my ($dir, $arg);

    unless ($conf) {
	($conf = $0) =~ s#/[^/]+$#/#;
	$conf .= $confname;
    }
    
    unless (open(CONFIG, $conf)) {
	&fatal("Couldn't open configuration file \"$conf\".");
    }
    while (<CONFIG>) {
	s/\#.*//;
	next if /^\s*$/;
	if (($dir, $arg) = /^\s*(\S+):\s*(.*)/) {
	    if ($dir eq 'variable') {
		@args = &parseconf($arg);
		if (@args[0]) {
		    $self->{vardescr}->{$args[0]} = $args[1];
		    push(@{$self->{variables}},$args[0]);
		    $self->{varrange}->{$args[0]} = [split(/\0/,$args[2])];
		    $self->{vdefault}->{$args[0]} = $args[3];
		    $self->{vdefault}->{$args[0]} ||= 
			$self->{varrange}->{$args[0]}->[0];
		    $self->{variable}->{$args[0]} =
			$self->{vdefault}->{$args[0]};
		}
	    } elsif ($dir eq 'sourceroot' ||
		     $dir eq 'srcrootname' ||
                     $dir eq 'virtroot' ||
		     $dir eq 'baseurl' ||
		     $dir eq 'incprefix' ||
		     $dir eq 'dbdir' ||
		     $dir eq 'bonsaihome' ||
		     $dir eq 'glimpsebin' ||
		     $dir eq 'htmlhead' ||
		     $dir eq 'htmltail' ||
		     $dir eq 'sourcehead' ||
		     $dir eq 'sourcetail' ||
		     $dir eq 'sourcedirhead' ||
		     $dir eq 'sourcedirtail' ||
		     $dir eq 'findhead' ||
		     $dir eq 'findtail' ||
		     $dir eq 'identhead' ||
		     $dir eq 'identtail' ||
		     $dir eq 'searchhead' ||
		     $dir eq 'searchtail' ||
		     $dir eq 'htmldir') {
		if ($arg =~ /([^\n]+)/) {
	            if ($dir eq 'sourceroot') {
		        $self->{$dir} = $self->{$dir} . " " . $1;
                    }else{
		    $self->{$dir} = $1;
                    }
		}
	    } elsif ($dir eq 'map') {
		if ($arg =~ /(\S+)\s+(\S+)/) {
		    push(@{$self->{maplist}}, [$1,$2]);
		}
	    } else {
		&warning("Unknown config directive (\"$dir\")");
	    }			
	    next;
	}
	&warning("Noise in config file (\"$_\")");
    }
}


sub allvariables {
    my $self = shift;
    return(@{$self->{variables}});
}


sub variable {
    my ($self, $var, $val) = @_;
    $self->{variable}->{$var} = $val if defined($val);
    return($self->{variable}->{$var});
}


sub vardefault {
    my ($self, $var) = @_;
    return($self->{vdefault}->{$var});
}


sub vardescription {
    my ($self, $var, $val) = @_;
    $self->{vardescr}->{$var} = $val if defined($val);
    return($self->{vardescr}->{$var});
}


sub varrange {
    my ($self, $var) = @_;
    return(@{$self->{varrange}->{$var}});
}


sub varexpand {
    my ($self, $exp) = @_;
    $exp =~ s/\$\{?(\w+)\}?/$self->{variable}->{$1}/g;
    return($exp);
}


sub baseurl {
    my $self = shift;
    return($self->varexpand($self->{'baseurl'}));
}


sub sourceroot {
    my $self = shift;
    return($self->varexpand($self->{'sourceroot'}));
}


sub sourcerootname {
    my $self = shift;
    return($self->varexpand($self->{'srcrootname'}));
}

sub virtroot{
    my $self = shift;
    return($self->varexpand($self->{'virtroot'}));
}


sub incprefix {
    my $self = shift;
    return($self->varexpand($self->{'incprefix'}));
}


sub bonsaihome {
    my $self = shift;
    return($self->varexpand($self->{'bonsaihome'}));
}


sub dbdir {
    my $self = shift;
    return($self->varexpand($self->{'dbdir'}));
}


sub glimpsebin {
    my $self = shift;
    return($self->varexpand($self->{'glimpsebin'}));
}


sub htmlhead {
    my $self = shift;
    return($self->varexpand($self->{'htmlhead'}));
}


sub htmltail {
    my $self = shift;
    return($self->varexpand($self->{'htmltail'}));
}

sub sourcehead {
    my $self = shift;
    return($self->varexpand($self->{'sourcehead'}));
}

sub sourcetail {
    my $self = shift;
    return($self->varexpand($self->{'sourcetail'}));
}

sub sourcedirhead {
    my $self = shift;
    return($self->varexpand($self->{'sourcedirhead'}));
}

sub sourcedirtail {
    my $self = shift;
    return($self->varexpand($self->{'sourcedirtail'}));
}

sub findhead {
    my $self = shift;
    return($self->varexpand($self->{'findhead'}));
}

sub findtail {
    my $self = shift;
    return($self->varexpand($self->{'findtail'}));
}

sub identhead {
    my $self = shift;
    return($self->varexpand($self->{'identhead'}));
}

sub identtail {
    my $self = shift;
    return($self->varexpand($self->{'identtail'}));
}

sub searchhead {
    my $self = shift;
    return($self->varexpand($self->{'searchhead'}));
}

sub searchtail {
    my $self = shift;
    return($self->varexpand($self->{'searchtail'}));
}


sub htmldir {
    my $self = shift;
    return($self->varexpand($self->{'htmldir'}));
}


sub mappath {
    my ($self, $path, @args) = @_;
    my (%oldvars) = %{$self->{variable}};
    my ($m);
    
    foreach $m (@args) {
	$self->{variable}->{$1} = $2 if $m =~ /(.*?)=(.*)/;
    }

    foreach $m (@{$self->{maplist}}) {
	$path =~ s/$m->[0]/$self->varexpand($m->[1])/e;
    }

    $self->{variable} = {%oldvars};
    return($path);
}

#sub mappath {
#    my ($self, $path) = @_;
#    my ($m);
#    
#    foreach $m (@{$self->{maplist}}) {
#	$path =~ s/$m->[0]/$self->varexpand($m->[1])/e;
#    }
#    return($path);
#}

1;
