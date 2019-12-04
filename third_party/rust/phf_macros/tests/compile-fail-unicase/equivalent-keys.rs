use unicase::UniCase;
use phf::phf_map;

static MAP: phf::Map<UniCase<&'static str>, isize> = phf_map!( //~ ERROR duplicate key UniCase("FOO")
    UniCase("FOO") => 42, //~ NOTE one occurrence here
    UniCase("foo") => 42, //~ NOTE one occurrence here
);

fn main() {}
