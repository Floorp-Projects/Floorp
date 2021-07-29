mod scenarios;

pub use scenarios::get_scenarios;

pub struct Scenario {
    pub name: String,
    pub width: usize,
    pub depth: usize,
    pub values: Vec<Vec<bool>>,
    pub solutions: Vec<Vec<usize>>,
}

impl Scenario {
    pub fn new<S: ToString>(
        name: S,
        width: usize,
        depth: usize,
        values: Vec<Vec<bool>>,
        solutions: Vec<Vec<usize>>,
    ) -> Self {
        Self {
            name: name.to_string(),
            width,
            depth,
            values,
            solutions,
        }
    }
}
