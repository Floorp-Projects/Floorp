#
# Sign step. Wait for signed builds to appear.
# 
package Bootstrap::Step::Sign;
use Bootstrap::Step;
use Bootstrap::Config;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $rc = $config->Get(var => 'rc');

    my $signedDir = $config->GetFtpCandidateDir(bitsUnsigned => 0);

    while (! -f catfile($signedDir . 'win32_signing_rc' . $rc . '.log')) {
        sleep(10);
    }
}

sub Verify {}

sub Announce {
    my $this = shift;

    my $config = new Bootstrap::Config();
    my $product = $config->Get(var => 'product');
    my $version = $config->Get(var => 'version');

    $this->SendAnnouncement(
      subject => "$product $version sign step finished",
      message => "$product $version win32 builds have been signed and copied to the candidates dir.",
    );
}

1;
