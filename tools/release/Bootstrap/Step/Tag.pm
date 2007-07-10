#
# Tag step. Sets up the tagging directory, and checks out the mozilla source.
# 
package Bootstrap::Step::Tag;

use Cwd;
use File::Copy qw(move);
use POSIX qw(strftime);

use MozBuild::Util qw(MkdirWithPath RunShellCommand);
use Bootstrap::Util qw(CvsCatfile);

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
    my $logDir = $config->Get(var => 'logDir');

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
                     dir => catfile($cvsrootTagDir, 'mozilla'));
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
    my $logDir = $config->Get(var => 'logDir');

    # This step doesn't really do anything now, because the verification it used
    # to do (which wasn't much) is now done in the Execute() method, since the
    # biz logic for rc 1 vs. rc > 1 is different.
}

sub CvsTag {
    my $this = shift;
    my %args = @_;

    # All the required args first, followed by the optional ones...
    die "ASSERT: Bootstrap::Step::Tag::CvsTag(): null tagName" if 
     (!exists($args{'tagName'})); 
    my $tagName = $args{'tagName'};

    die "ASSERT: Bootstrap::Step::Tag::CvsTag(): null coDir" if 
     (!exists($args{'coDir'})); 
    my $coDir = $args{'coDir'};

    die "ASSERT: Bootstrap::Step::Tag::CvsTag(): invalid files data" if 
     (exists($args{'files'}) && ref($args{'files'}) ne 'ARRAY');

    die "ASSERT: Bootstrap::Step::Tag::CvsTag(): null logFile"
     if (!exists($args{'logFile'}));
    my $logFile = $args{'logFile'};
   
    my $branch = exists($args{'branch'}) ? $args{'branch'} : 0;
    my $files = exists($args{'files'}) ? $args{'files'} : [];
    my $force = exists($args{'force'}) ? $args{'force'} : 0;

    my $config = new Bootstrap::Config();
    my $logDir = $config->Get(var => 'logDir');

    # only force or branch specific files, not the whole tree
    if ($force && scalar(@{$files}) <= 0) {
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): Cannot specify force without files");
    } elsif ($branch && scalar(@{$files}) <= 0) {
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): Cannot specify branch without files");
    } elsif ($branch && $force) {
        die("ASSERT: Bootstrap::Step::Tag::CvsTag(): Cannot specify both branch and force");
    }

    my @cmdArgs;
    push(@cmdArgs, 'tag');
    push(@cmdArgs, '-F') if ($force);
    push(@cmdArgs, '-b') if ($branch);
    push(@cmdArgs, $tagName);
    push(@cmdArgs, @{$files}) if (scalar(@{$files}) > 0);

    $this->Shell(
      cmd => 'cvs',
      cmdArgs => \@cmdArgs,
      dir => $coDir,
      logFile => $logFile,
    );
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

sub GetDiffFileList {
    my $this = shift;
    my %args = @_;

    foreach my $requiredArg (qw(cvsDir prevTag newTag)) {
        if (!exists($args{$requiredArg})) {
            die "ASSERT: MozBuild::Util::GetDiffFileList(): null arg: " .
             $requiredArg;
        }
    }

    my $cvsDir = $args{'cvsDir'};
    my $firstTag = $args{'prevTag'};
    my $newTag = $args{'newTag'};

    my $rv = RunShellCommand(command => 'cvs',
                             args => ['diff', '-uN',
                                      '-r', $firstTag,
                                      '-r', $newTag],
                             dir => $cvsDir,
                             timeout => 3600);

    # Gah. So, the shell return value of "cvs diff" is dependent on whether or
    # not there were diffs, NOT whether or not the command succeeded. (Thanks,
    # CVS!) So, we can't really check exitValue here, since it could be 1 or
    # 0, depending on whether or not there were diffs (and both cases are valid
    # for this function). Maybe if there's an error it returns a -1? Or 2?
    # Who knows.
    #
    # So basically, we check that it's not 1 or 0, which... isn't a great test.
    #
    # TODO - check to see if timedOut, dumpedCore, or sigNum are set.
    if ($rv->{'exitValue'} != 1 && $rv->{'exitValue'} != 0) {
        die("ASSERT: MozBuild::Util::GetDiffFileList(): cvs diff returned " .
         $rv->{'exitValue'});
    }

    my @differentFiles = ();

    foreach my $line (split(/\n/, $rv->{'output'})) {
        if ($line =~ /^Index:\s(.+)$/) {
            push(@differentFiles, $1);
        }
    }


    return \@differentFiles;
}

1;
