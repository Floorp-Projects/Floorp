#!/usr/bin/perl -w

package Doctor;
require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw($template $vars %CONFIG);  # symbols to export on request

use strict;

use File::Temp qw(tempfile tempdir);
use Template;
use AppConfig qw(:expand :argcount);

# Create the global template object that processes templates and specify
# configuration parameters that apply to templates processed in this script.
our $template = Template->new({
    # Colon-separated list of directories containing templates.
    INCLUDE_PATH => "templates",
    PRE_CHOMP    => 1,
    POST_CHOMP   => 1
});

# Define the global variables and functions that will be passed to the UI
# template.  Individual functions add their own values to this hash before
# sending them to the templates they process.
our $vars = {};

# Create an AppConfig object and populate it with parameters defined
# in the configuration file.
# Note: Look in the configuration file for descriptions of each parameter.
our $config = AppConfig->new({ 
    CASE    => 1,
    CREATE  => 1 , 
    GLOBAL  => { ARGCOUNT => ARGCOUNT_ONE }
});
$config->file("doctor.conf");
our %CONFIG = $config->varlist(".*");

$vars->{'config'} = \%CONFIG;

sub system_capture {
    # Runs a command and captures its output and errors.  This should be using
    # in-memory files, but they require that we close STDOUT and STDERR
    # before reopening them on the in-memory files, and closing and reopening
    # STDERR causes CVS to choke with return value 256.
  
    my ($command, @args) = @_;
  
    my ($rv, $output, $errors);
  
    # Back up the original STDOUT and STDERR so we can restore them later.
    open(OLDOUT, ">&STDOUT") or die "Can't back up STDOUT to OLDOUT: $!";
    open(OLDERR, ">&STDERR") or die "Can't back up STDERR to OLDERR: $!";
    use vars qw( *OLDOUT *OLDERR ); # suppress "used only once" warnings
  
    # Close and reopen STDOUT and STDERR to in-memory files, which are just
    # scalars that take output and append it to their value.
    # XXX Disabled in-memory files in favor of temp files until in-memory issues
    # can be worked out.
    #close(STDOUT);
    #close(STDERR);
    #open(STDOUT, ">", \$output) or die "Can't open STDOUT to output var: $!";
    #open(STDERR, ">", \$errors) or die "Can't open STDERR to errors var: $!";
    my $outfile = tempfile();
    my $errfile = tempfile();
    # Perl 5.6.1 filehandle duplication doesn't support the three-argument form
    # of open, so we can't just open(STDOUT, ">&", $outfile); instead we have to
    # create an alias OUTFILE and then do open(STDOUT, ">&OUTFILE").
    local *OUTFILE = *$outfile;
    local *ERRFILE = *$errfile;
    use vars qw( *OUTFILE *ERRFILE ); # suppress "used only once" warnings
    open(STDOUT, ">&OUTFILE") or open(STDOUT, ">&OLDOUT")
                                 and die "Can't dupe STDOUT to output file: $!";
    open(STDERR, ">&ERRFILE") or open(STDOUT, ">&OLDOUT")
                                 and open(STDERR, ">&OLDERR")
                                 and die "Can't dupe STDERR to errors file: $!";
  
    # Run the command.
    $rv = system($command, @args);
  
    # Grab output and errors from the temp files.  In a block to localize $/.
    # XXX None of this would be necessary if in-memory files was working.
    {
        local $/ = undef;
        seek($outfile, 0, 0);
        seek($errfile, 0, 0);
        $output = <$outfile>;
        $errors = <$errfile>;
    }

    # Restore original STDOUT and STDERR.
    close(STDOUT);
    close(STDERR);
    open(STDOUT, ">&OLDOUT") or die "Can't restore STDOUT from OLDOUT: $!";
    open(STDERR, ">&OLDERR") or die "Can't restore STDERR from OLDERR: $!";

    return ($rv, $output, $errors);
}

1;  # so the require or use succeeds
