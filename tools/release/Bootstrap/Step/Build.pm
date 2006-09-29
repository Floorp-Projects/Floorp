package Bootstrap::Step::Build;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo build');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify build');
}

1;
