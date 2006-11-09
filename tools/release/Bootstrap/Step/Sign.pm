#
# Sign step. Applies digital signatures to builds.
# 
package Bootstrap::Step::Sign;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo sign');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify sign');
}

1;
