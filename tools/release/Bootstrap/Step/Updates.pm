package Bootstrap::Step::Updates;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo updates');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify updates');
}

1;
