#
# Tag step. Sets up the tagging directory, and checks out the mozilla source.
# 
package Bootstrap::Step::Tag;

use Cwd;
use File::Copy qw(move);
use POSIX qw(strftime);

use MozBuild::Util qw(MkdirWithPath RunShellCommand);
use Bootstrap::Util qw(CvsCatfile GetDiffFileList);

use Bootstrap::Step;
use Bootstrap::Step::Tag::Bump;
use Bootstrap::Step::Tag::Mozilla;
use Bootstrap::Step::Tag::l10n;
use Bootstrap::Step::Tag::Talkback;
use Bootstrap::Config;

use strict;

our @ISA = qw(Bootstrap::Step);

my @TAG_SUB_STEPS = qw( Bump
                        Mozilla
                        l10n
                        Talkback
                      );

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $productTag = $config->Get(var => 'productTag');
    my $rc = $config->Get(var => 'rc');
    my $milestone = $config->Get(var => 'milestone');
    my $tagDir = $config->Get(var => 'tagDir');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $branchTag = $config->Get(var => 'branchTag');
    my $pullDate = $config->Get(var => 'pullDate');
    my $logDir = $config->Get(sysvar => 'logDir');

    my $releaseTag = $productTag . '_RELEASE';
    my $rcTag = $productTag . '_RC' . $rc;
    my $releaseTagDir = catfile($tagDir, $releaseTag);
    my $rcTagDir = catfile($tagDir, $rcTag);

    # create the main tag directory
    if (not -d $rcTagDir) {
        MkdirWithPath(dir => $rcTagDir) 
          or die("Cannot mkdir $rcTagDir: $!");
    }

    # Tagging area for Mozilla
    my $cvsrootTagDir = catfile($rcTagDir, 'cvsroot');
    if (-e $cvsrootTagDir) {
        die "ASSERT: Tag::Execute(): $cvsrootTagDir already exists?";
    }

    MkdirWithPath(dir => $cvsrootTagDir) 
     or die("Cannot mkdir $cvsrootTagDir: $!");

    # Check out Mozilla from the branch you want to tag.
    # TODO this should support running without branch tag or pull date.

    my $geckoTag = undef;

    if (1 == $rc) {
        $this->Shell(cmd => 'cvs',
                     cmdArgs => ['-d', $mozillaCvsroot, 
                                 'co', 
                                 '-r', $branchTag, 
                                 '-D', $pullDate, 
                                 CvsCatfile('mozilla', 'client.mk'),
                                ],
                     dir => $cvsrootTagDir,
                     logFile => catfile($logDir, 'tag_checkout_client_mk.log'),
                   );

        $this->CheckLog(log => catfile($logDir, 'tag_checkout_client_mk.log'),
                       checkForOnly => '^U mozilla/client.mk');

        $this->Shell(cmd => 'gmake',
                     cmdArgs => ['-f', 'client.mk', 'checkout', 
                                 'MOZ_CO_PROJECT=all', 
                                 'MOZ_CO_DATE=' . $pullDate],
                     dir => catfile($cvsrootTagDir, 'mozilla'),
                     logFile => catfile($logDir, 'tag_mozilla-checkout.log'));

        $this->CheckLog(
          log => catfile($logDir, 'tag_mozilla-checkout.log'),
          checkFor => '^U',
        );

        $geckoTag = $this->GenerateRelbranchName(milestone => $milestone);

        # The glob seems weird/pointless, but it's because CvsTag requires a 
        # list of files to operate on in the branch => 1 case. This may (or
        # may not) be considered a bug, depending on how paranoid you're
        # feeling about automatically creating branches.

        my $cwd = getcwd();
        chdir(catfile($cvsrootTagDir, 'mozilla')) or
         die "Couldn't chdir() to $cvsrootTagDir/mozilla: $!\n";
        my @topLevelMozCVSFiles = grep(!/^CVS$/, glob('*'));
        chdir($cwd) or die "Couldn't chdir() home: $!\n";

        $this->CvsTag(tagName => $geckoTag,
                      branch => 1,
                      files => \@topLevelMozCVSFiles,
                      coDir => catfile($cvsrootTagDir, 'mozilla'),
                      logFile => catfile($logDir, 'tag-relbranch_tag-' . 
                                         $geckoTag));

        $this->Shell(cmd => 'cvs',
                     cmdArgs => ['up',
                                 '-r', $geckoTag],
                     dir => catfile($cvsrootTagDir, 'mozilla'),
                     logFile => catfile($logDir, 'tag-relbranch_update_' .
                                        $geckoTag));
    } else {
        # We go through some convoluted hoops here to get the _RELBRANCH
        # datespec without forcing it to be specified. Because of this,
        # there's lots of icky CVS parsing.

        my $rcOneTag = $productTag . '_RC1';
        my $checkoutLog = "tag_rc${rc}_checkout_client_ck.log";

        $this->Shell(cmd => 'cvs',
                     cmdArgs => ['-d', $mozillaCvsroot,
                                 'co', 
                                 '-r', $branchTag, 
                                 '-D', $pullDate, 
                                 CvsCatfile('mozilla', 'client.mk')],
                     dir => $cvsrootTagDir,
                     logFile => catfile($logDir, $checkoutLog),
                   );


        $this->CheckLog(log => catfile($logDir, $checkoutLog),
                        checkForOnly => '^U mozilla/client.mk');

        # Use RunShellCommand() here because we need to grab the output,
        # and Shell() sends the output to a log.
        my $clientMkInfo = RunShellCommand(command => 'cvs',
                                           args => ['log', 
                                                    'client.mk'],
                                           dir => catfile($cvsrootTagDir, 
                                                          'mozilla'));
       
        if ($clientMkInfo->{'exitValue'} != 0) {
            die("cvs log call on client.mk failed: " .
             $clientMkInfo->{'exitValue'} . "\n");
        }
   
        my $inSymbolic = 0;
        my $inDescription = 0;
        my $haveRev = 0;
        my $cvsRev = '';
        my $cvsDateSpec = '';
        foreach my $logLine (split(/\n/, $clientMkInfo->{'output'})) {
            if ($inSymbolic && $logLine =~ /^\s+$rcOneTag:\s([\d\.]+)$/) {
                $cvsRev = $1;            
                $inSymbolic = 0;
                next;
            } elsif ($inDescription && $logLine =~ /^revision $cvsRev$/) {
                $haveRev = 1;
                next;
            } elsif ($haveRev) {
                if ($logLine =~ /^date:\s([^;]+);/) {
                    # Gives us a line like: "2006/12/05 19:12:58" 
                    $cvsDateSpec = $1;
                    last;
                }

                die 'ASSERT: Step::Tag::Execute(): have rev, but did not ' .
                 'find a datespec?';
            } elsif ($logLine =~ /^symbolic names:/) {
                $inSymbolic = 1;
                next;
            } elsif ($logLine =~ /^description:/) {
                $inDescription = 1;
                next;
            }
        }

        # relBranchDateSpec now has something like: "2006/12/05 19:12:58" 
        my $relBranchDateSpec = $cvsDateSpec;
        # Strip off the time...
        $relBranchDateSpec =~ s/^\s*([\d\/]+).*/$1/;
        # Strip out the /'s; now we have our datespec: 20061205
        $relBranchDateSpec =~ s/\///g;

        $geckoTag = $this->GenerateRelbranchName(milestone => $milestone,
         datespec => $relBranchDateSpec);

        $this->Shell(cmd => 'cvs',
                     cmdArgs => ['-d', $mozillaCvsroot,
                                 'co',
                                 '-r', $geckoTag,
                                 'mozilla'],
                     dir => $cvsrootTagDir,
                     logFile => catfile($logDir, 'tag_checkout_client_mk.log'),
                   );

    }

    $config->Set(var => 'geckoBranchTag', value => $geckoTag);

    # Call substeps
    for (my $curStep = 0; $curStep < scalar(@TAG_SUB_STEPS); $curStep++) {
        my $stepName = $TAG_SUB_STEPS[$curStep];
        eval {
            $this->Log(msg => 'Tag running substep ' . $stepName);
            my $step = "Bootstrap::Step::Tag::$stepName"->new();
            $step->Execute();
            $step->Verify();
        };
        if ($@) {
            die("Tag substep $stepName died: $@");
        }
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(sysvar => 'logDir');

    # This step doesn't really do anything now, because the verification it used
    # to do (which wasn't much) is now done in the Execute() method, since the
    # biz logic for rc 1 vs. rc > 1 is different.
}

sub CvsTag {
    my $this = shift;
    my %args = @_;

    # All the required args first, followed by the optional ones...

    die "ASSERT: Bootstrap::Step::Tag::CvsTag(): null logFile"
     if (!exists($args{'logFile'}));
    die "ASSERT: Bootstrap::Step::Tag::CvsTag(): null coDir" if 
     (!exists($args{'coDir'}));

    # We renamed this argument when CvsTag() got moved...
    $args{'cvsDir'} = $args{'coDir'};

    # Check if we're supposed to dump the tagging output to stdout...
    my $config = new Bootstrap::Config();
    if ($config->Exists(var => 'dumpLogs') && 
     $config->Get(var => 'dumpLogs')) {
        $args{'output'} = 1;
    }

    # We call this by full scoping (and don't include it in the use() statement
    # for Bootstrap::Util above) to disambiguate between the Util version and
    # the Tag version, which is a shim now.
    my $rv = Bootstrap::Util::CvsTag(%args);

    if ($rv->{'timedOut'} || ($rv->{'exitValue'} != 0)) {
        $this->Log(msg => "Bootstrap::Step::Tag::CvsTag failed; rv: " .
         "$rv->{'exitValue'}, timeout: $rv->{'timedOut'}, output: " .
         "$rv->{'output'}");
        die("Bootstrap::Step::Tag::CvsTag: exited bogusly: $rv->{'exitValue'}");
    }

    return $rv;
}

#
# Give me some information, I'll give you the GECKO$version_$datespec_RELBRANCH
# tag back; utility function, so we can centralize creation of this string.
#
# It has two modes; if you give it a branch (always required), you'll get a new
# (current) _RELBRANCH tag; if you give it a datespec, you'll get a _RELBRANCH
# tag based on that datespec (which, of course, may or may not exist.
#
# You can override all of this logic by setting "RelbranchOverride" in the 
# bootstrap.cfg.
#
sub GenerateRelbranchName {
    my $this = shift;
    my %args = @_;
    
    die "ASSERT: GenerateRelbranchName(): null milestone" if 
     (!exists($args{'milestone'}));

    my $config = new Bootstrap::Config();

    if ($config->Exists(var => 'RelbranchOverride')) {
        return $config->Get(var => 'RelbranchOverride');
    }

    # Convert milestone (1.8.1.x) into "181"; we assume we should always have
    # three digits for now (180, 181, 190, etc.)

    my $geckoVersion = $args{'milestone'};
    $geckoVersion =~ s/\.//g;
    $geckoVersion =~ s/^(\d{3}).*$/$1/;

    die "ASSERT: GenerateRelbranchName(): Gecko version should be only " .
     "numbers by now" if ($geckoVersion !~ /^\d{3}$/);

    my $geckoDateSpec = exists($args{'datespec'}) ? $args{'datespec'} : 
     strftime('%Y%m%d', localtime());

    # This assert()ion has a Y21k (among other) problem(s)...
    die "ASSERT: GenerateRelbranchName(): invalid datespec" if 
     ($geckoDateSpec !~ /^20\d{6}$/);

    return 'GECKO' . $geckoVersion . '_' . $geckoDateSpec . '_RELBRANCH';
}

1;
