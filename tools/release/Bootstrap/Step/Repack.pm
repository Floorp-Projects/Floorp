package Bootstrap::Step::Repack;
use Bootstrap::Step;
@ISA = ("Bootstrap::Step");

sub Execute {
    my $this = shift;
    $this->Shell('cmd' => 'echo repack');
}

sub Verify {
    my $this = shift;
    $this->Shell('cmd' => 'echo Verify repack');
}

1;
