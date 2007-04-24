##
# TinderConfig - creates config file for Tinderbox
##

package Bootstrap::Step::TinderConfig;

use Bootstrap::Step;
use Bootstrap::Config;
use MozBuild::TinderLogParse;
use MozBuild::Util qw(MkdirWithPath);

@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $configBumpDir = $config->Get(var => 'configBumpDir');
    my $productTag = $config->Get(var => 'productTag');
    my $version = $config->Get(var => 'version');
    my $mozillaCvsroot = $config->Get(var => 'mozillaCvsroot');
    my $product = $config->Get(var => 'product');
    my $logDir = $config->Get(var => 'logDir');
    my $branchTag = $config->Get(var => 'branchTag');
    my $osname = $config->SystemInfo(var => 'osname');

    my $releaseTag = $productTag . '_RELEASE';

    MkdirWithPath(dir => $configBumpDir)
      or die("Cannot mkdir $configBumpDir: $!");

    foreach my $branch ($branchTag . '_release', $branchTag . '_l10n_release') {
        $this->Shell(
          cmd => 'cvs',
          cmdArgs => ['-d', $mozillaCvsroot, 
                      'co', '-d', 'tinderbox-configs',
                      '-r', $branch,
                      'mozilla/tools/tinderbox-configs/' . 
                      $product . '/' . $osname],
          logFile => catfile($logDir, 
           'build_config-checkout-' . $branch . '.log'),
          dir => $configBumpDir,
        );
    
        foreach my $configFile ('tinder-config.pl', 'mozconfig') {
            $config->Bump( configFile => 
             catfile($configBumpDir, 'tinderbox-configs', $configFile));
            $this->Shell(
              cmd => 'cvs',
              cmdArgs => ['-d', $mozillaCvsroot, 
                          'ci', '-m', 
                          '"Automated configuration bump, release for ' 
                           . $product  . ' ' . $version . '"', 
                          'tinderbox-configs/' . $configFile],
              logFile => catfile($logDir, 
               'build_config-checkin-' . $configFile . '-' . 
                $branch . '.log'),
              dir => catfile($configBumpDir),
            );
        }
    }
}

sub Verify {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $branchTag = $config->Get(var => 'branchTag');
    my $logDir = $config->Get(var => 'logDir');

    foreach my $branch ($branchTag . '_release', $branchTag . '_l10n_release') {
        $this->CheckLog(
            log => catfile($logDir, 
             'build_config-checkout-' . $branch . '.log'),
            notAllowed => 'fail',
        );
        $this->CheckLog(
            log => catfile($logDir, 
             'build_config-checkout-' . $branch . '.log'),
            notAllowed => 'aborted',
        );
        foreach my $configFile ('mozconfig', 'tinder-config.pl') {
            $this->CheckLog(
             log => catfile($logDir,
               'build_config-checkin-' . $configFile . '-' .  $branch . '.log'),
             notAllowed => 'fail',
            );
            $this->CheckLog(
             log => catfile($logDir,
               'build_config-checkin-' . $configFile . '-' .  $branch . '.log'),
             notAllowed => 'aborted',
            );
        }
    }
}
