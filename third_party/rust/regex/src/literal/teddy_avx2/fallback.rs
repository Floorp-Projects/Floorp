use syntax::hir::literal::Literals;

#[derive(Debug, Clone)]
pub struct Teddy(());

#[derive(Debug, Clone)]
pub struct Match {
    pub pat: usize,
    pub start: usize,
    pub end: usize,
}

impl Teddy {
    pub fn available() -> bool { false }
    pub fn new(_pats: &Literals) -> Option<Teddy> { None }
    pub fn patterns(&self) -> &[Vec<u8>] { &[] }
    pub fn len(&self) -> usize { 0 }
    pub fn approximate_size(&self) -> usize { 0 }
    pub fn find(&self, _haystack: &[u8]) -> Option<Match> { None }
}
