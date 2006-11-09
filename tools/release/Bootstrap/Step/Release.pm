#
# Release step. Pushes bits to production.
#
package Bootstrap::Step::Release;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo release');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify release');
}

1;
