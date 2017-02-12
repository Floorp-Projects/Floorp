#![cfg_attr(windows, allow(dead_code, unused_imports))]

use std::env;
use std::fs::{self, File};
use std::io;
use std::path::{Path, PathBuf};

use quickcheck::{Arbitrary, Gen, QuickCheck, StdGen};
use rand::{self, Rng};

use super::{DirEntry, WalkDir, WalkDirIterator, Iter, Error, ErrorInner};

#[derive(Clone, Debug, Eq, Ord, PartialEq, PartialOrd)]
enum Tree {
    Dir(PathBuf, Vec<Tree>),
    File(PathBuf),
    Symlink {
        src: PathBuf,
        dst: PathBuf,
        dir: bool,
    }
}

impl Tree {
    fn from_walk_with<P, F>(
        p: P,
        f: F,
    ) -> io::Result<Tree>
    where P: AsRef<Path>, F: FnOnce(WalkDir) -> WalkDir {
        let mut stack = vec![Tree::Dir(p.as_ref().to_path_buf(), vec![])];
        let it: WalkEventIter = f(WalkDir::new(p)).into();
        for ev in it {
            match try!(ev) {
                WalkEvent::Exit => {
                    let tree = stack.pop().unwrap();
                    if stack.is_empty() {
                        return Ok(tree);
                    }
                    stack.last_mut().unwrap().children_mut().push(tree);
                }
                WalkEvent::Dir(dent) => {
                    stack.push(Tree::Dir(pb(dent.file_name()), vec![]));
                }
                WalkEvent::File(dent) => {
                    let node = if dent.file_type().is_symlink() {
                        let src = try!(dent.path().read_link());
                        let dst = pb(dent.file_name());
                        let dir = dent.path().is_dir();
                        Tree::Symlink { src: src, dst: dst, dir: dir }
                    } else {
                        Tree::File(pb(dent.file_name()))
                    };
                    stack.last_mut().unwrap().children_mut().push(node);
                }
            }
        }
        assert_eq!(stack.len(), 1);
        Ok(stack.pop().unwrap())
    }

    fn name(&self) -> &Path {
        match *self {
            Tree::Dir(ref pb, _) => pb,
            Tree::File(ref pb) => pb,
            Tree::Symlink { ref dst, .. } => dst,
        }
    }

    fn unwrap_singleton(self) -> Tree {
        match self {
            Tree::File(_) | Tree::Symlink { .. } => {
                panic!("cannot unwrap file or link as dir");
            }
            Tree::Dir(_, mut childs) => {
                assert_eq!(childs.len(), 1);
                childs.pop().unwrap()
            }
        }
    }

    fn unwrap_dir(self) -> Vec<Tree> {
        match self {
            Tree::File(_) | Tree::Symlink { .. } => {
                panic!("cannot unwrap file as dir");
            }
            Tree::Dir(_, childs) => childs,
        }
    }

    fn children_mut(&mut self) -> &mut Vec<Tree> {
        match *self {
            Tree::File(_) | Tree::Symlink { .. } => {
                panic!("files do not have children");
            }
            Tree::Dir(_, ref mut children) => children,
        }
    }

    fn create_in<P: AsRef<Path>>(&self, parent: P) -> io::Result<()> {
        let parent = parent.as_ref();
        match *self {
            Tree::Symlink { ref src, ref dst, dir } => {
                if dir {
                    try!(soft_link_dir(src, parent.join(dst)));
                } else {
                    try!(soft_link_file(src, parent.join(dst)));
                }
            }
            Tree::File(ref p) => { try!(File::create(parent.join(p))); }
            Tree::Dir(ref dir, ref children) => {
                try!(fs::create_dir(parent.join(dir)));
                for child in children {
                    try!(child.create_in(parent.join(dir)));
                }
            }
        }
        Ok(())
    }

    fn canonical(&self) -> Tree {
        match *self {
            Tree::Symlink { ref src, ref dst, dir } => {
                Tree::Symlink { src: src.clone(), dst: dst.clone(), dir: dir }
            }
            Tree::File(ref p) => {
                Tree::File(p.clone())
            }
            Tree::Dir(ref p, ref cs) => {
                let mut cs: Vec<Tree> =
                    cs.iter().map(|c| c.canonical()).collect();
                cs.sort();
                Tree::Dir(p.clone(), cs)
            }
        }
    }

    fn dedup(&self) -> Tree {
        match *self {
            Tree::Symlink { ref src, ref dst, dir } => {
                Tree::Symlink { src: src.clone(), dst: dst.clone(), dir: dir }
            }
            Tree::File(ref p) => {
                Tree::File(p.clone())
            }
            Tree::Dir(ref p, ref cs) => {
                let mut nodupes: Vec<Tree> = vec![];
                for (i, c1) in cs.iter().enumerate() {
                    if !cs[i+1..].iter().any(|c2| c1.name() == c2.name())
                        && !nodupes.iter().any(|c2| c1.name() == c2.name()) {
                        nodupes.push(c1.dedup());
                    }
                }
                Tree::Dir(p.clone(), nodupes)
            }
        }
    }

    fn gen<G: Gen>(g: &mut G, depth: usize) -> Tree {
        #[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
        struct NonEmptyAscii(String);

        impl Arbitrary for NonEmptyAscii {
            fn arbitrary<G: Gen>(g: &mut G) -> NonEmptyAscii {
                use std::char::from_u32;
                let upper_bound = g.size();
                // We start with a lower bound of `4` to avoid
                // generating the special file name `con` on Windows,
                // because such files cannot exist...
                let size = g.gen_range(4, upper_bound);
                NonEmptyAscii((0..size)
                .map(|_| from_u32(g.gen_range(97, 123)).unwrap())
                .collect())
            }

            fn shrink(&self) -> Box<Iterator<Item=NonEmptyAscii>> {
                let mut smaller = vec![];
                for i in 1..self.0.len() {
                    let s: String = self.0.chars().skip(i).collect();
                    smaller.push(NonEmptyAscii(s));
                }
                Box::new(smaller.into_iter())
            }
        }

        let name = pb(NonEmptyAscii::arbitrary(g).0);
        if depth == 0 {
            Tree::File(name)
        } else {
            let children: Vec<Tree> =
                (0..g.gen_range(0, 5))
                .map(|_| Tree::gen(g, depth-1))
                .collect();
            Tree::Dir(name, children)
        }
    }
}

impl Arbitrary for Tree {
    fn arbitrary<G: Gen>(g: &mut G) -> Tree {
        let depth = g.gen_range(0, 5);
        Tree::gen(g, depth).dedup()
    }

    fn shrink(&self) -> Box<Iterator<Item=Tree>> {
        let trees: Box<Iterator<Item=Tree>> = match *self {
            Tree::Symlink { .. } => unimplemented!(),
            Tree::File(ref path) => {
                let s = path.to_string_lossy().into_owned();
                Box::new(s.shrink().map(|s| Tree::File(pb(s))))
            }
            Tree::Dir(ref path, ref children) => {
                let s = path.to_string_lossy().into_owned();
                if children.is_empty() {
                    Box::new(s.shrink().map(|s| Tree::Dir(pb(s), vec![])))
                } else if children.len() == 1 {
                    let c = &children[0];
                    Box::new(Some(c.clone()).into_iter().chain(c.shrink()))
                } else {
                    Box::new(children
                             .shrink()
                             .map(move |cs| Tree::Dir(pb(s.clone()), cs)))
                }
            }
        };
        Box::new(trees.map(|t| t.dedup()))
    }
}

#[derive(Debug)]
enum WalkEvent {
    Dir(DirEntry),
    File(DirEntry),
    Exit,
}

struct WalkEventIter {
    depth: usize,
    it: Iter,
    next: Option<Result<DirEntry, Error>>,
}

impl From<WalkDir> for WalkEventIter {
    fn from(it: WalkDir) -> WalkEventIter {
        WalkEventIter { depth: 0, it: it.into_iter(), next: None }
    }
}

impl Iterator for WalkEventIter {
    type Item = io::Result<WalkEvent>;

    fn next(&mut self) -> Option<io::Result<WalkEvent>> {
        let dent = self.next.take().or_else(|| self.it.next());
        let depth = match dent {
            None => 0,
            Some(Ok(ref dent)) => dent.depth(),
            Some(Err(ref err)) => err.depth(),
        };
        if depth < self.depth {
            self.depth -= 1;
            self.next = dent;
            return Some(Ok(WalkEvent::Exit));
        }
        self.depth = depth;
        match dent {
            None => None,
            Some(Err(err)) => Some(Err(From::from(err))),
            Some(Ok(dent)) => {
                if dent.file_type().is_dir() {
                    self.depth += 1;
                    Some(Ok(WalkEvent::Dir(dent)))
                } else {
                    Some(Ok(WalkEvent::File(dent)))
                }
            }
        }
    }
}

struct TempDir(PathBuf);

impl TempDir {
    fn path<'a>(&'a self) -> &'a Path {
        &self.0
    }
}

impl Drop for TempDir {
    fn drop(&mut self) {
        fs::remove_dir_all(&self.0).unwrap();
    }
}

fn tmpdir() -> TempDir {
    let p = env::temp_dir();
    let mut r = rand::thread_rng();
    let ret = p.join(&format!("rust-{}", r.next_u32()));
    fs::create_dir(&ret).unwrap();
    TempDir(ret)
}

fn dir_setup_with<F>(t: &Tree, f: F) -> (TempDir, Tree)
        where F: FnOnce(WalkDir) -> WalkDir {
    let tmp = tmpdir();
    t.create_in(tmp.path()).unwrap();
    let got = Tree::from_walk_with(tmp.path(), f).unwrap();
    (tmp, got.unwrap_singleton().unwrap_singleton())
}

fn dir_setup(t: &Tree) -> (TempDir, Tree) {
    dir_setup_with(t, |wd| wd)
}

fn canon(unix: &str) -> String {
    if cfg!(windows) {
        unix.replace("/", "\\")
    } else {
        unix.to_string()
    }
}

fn pb<P: AsRef<Path>>(p: P) -> PathBuf { p.as_ref().to_path_buf() }
fn td<P: AsRef<Path>>(p: P, cs: Vec<Tree>) -> Tree {
    Tree::Dir(pb(p), cs)
}
fn tf<P: AsRef<Path>>(p: P) -> Tree {
    Tree::File(pb(p))
}
fn tld<P: AsRef<Path>, Q: AsRef<Path>>(src: P, dst: Q) -> Tree {
    Tree::Symlink { src: pb(src), dst: pb(dst), dir: true }
}
fn tlf<P: AsRef<Path>, Q: AsRef<Path>>(src: P, dst: Q) -> Tree {
    Tree::Symlink { src: pb(src), dst: pb(dst), dir: false }
}

#[cfg(unix)]
fn soft_link_dir<P: AsRef<Path>, Q: AsRef<Path>>(
    src: P,
    dst: Q,
) -> io::Result<()> {
    use std::os::unix::fs::symlink;
    symlink(src, dst)
}

#[cfg(unix)]
fn soft_link_file<P: AsRef<Path>, Q: AsRef<Path>>(
    src: P,
    dst: Q,
) -> io::Result<()> {
    soft_link_dir(src, dst)
}

#[cfg(windows)]
fn soft_link_dir<P: AsRef<Path>, Q: AsRef<Path>>(
    src: P,
    dst: Q,
) -> io::Result<()> {
    use std::os::windows::fs::symlink_dir;
    symlink_dir(src, dst)
}

#[cfg(windows)]
fn soft_link_file<P: AsRef<Path>, Q: AsRef<Path>>(
    src: P,
    dst: Q,
) -> io::Result<()> {
    use std::os::windows::fs::symlink_file;
    symlink_file(src, dst)
}

macro_rules! assert_tree_eq {
    ($e1:expr, $e2:expr) => {
        assert_eq!($e1.canonical(), $e2.canonical());
    }
}

#[test]
fn walk_dir_1() {
    let exp = td("foo", vec![]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_2() {
    let exp = tf("foo");
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_3() {
    let exp = td("foo", vec![tf("bar")]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_4() {
    let exp = td("foo", vec![tf("foo"), tf("bar"), tf("baz")]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_5() {
    let exp = td("foo", vec![td("bar", vec![])]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_6() {
    let exp = td("foo", vec![
        td("bar", vec![
           tf("baz"), td("bat", vec![]),
        ]),
    ]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_7() {
    let exp = td("foo", vec![
        td("bar", vec![
           tf("baz"), td("bat", vec![]),
        ]),
        td("a", vec![tf("b"), tf("c"), tf("d")]),
    ]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_sym_1() {
    let exp = td("foo", vec![tf("bar"), tlf("bar", "baz")]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_sym_2() {
    let exp = td("foo", vec![
        td("a", vec![tf("a1"), tf("a2")]),
        tld("a", "alink"),
    ]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
fn walk_dir_sym_root() {
    let exp = td("foo", vec![
        td("bar", vec![tf("a"), tf("b")]),
        tld("bar", "alink"),
    ]);
    let tmp = tmpdir();
    let tmp_path = tmp.path();
    let tmp_len = tmp_path.to_str().unwrap().len();
    exp.create_in(tmp_path).unwrap();

    let it = WalkDir::new(tmp_path.join("foo").join("alink")).into_iter();
    let mut got = it
        .map(|d| d.unwrap().path().to_str().unwrap()[tmp_len+1..].into())
        .collect::<Vec<String>>();
    got.sort();
    assert_eq!(got, vec![
        canon("foo/alink"), canon("foo/alink/a"), canon("foo/alink/b"),
    ]);

    let it = WalkDir::new(tmp_path.join("foo/alink/")).into_iter();
    let mut got = it
        .map(|d| d.unwrap().path().to_str().unwrap()[tmp_len+1..].into())
        .collect::<Vec<String>>();
    got.sort();
    assert_eq!(got, vec!["foo/alink/", "foo/alink/a", "foo/alink/b"]);
}

#[test]
#[cfg(unix)]
fn walk_dir_sym_detect_no_follow_no_loop() {
    let exp = td("foo", vec![
        td("a", vec![tf("a1"), tf("a2")]),
        td("b", vec![tld("../a", "alink")]),
    ]);
    let (_tmp, got) = dir_setup(&exp);
    assert_tree_eq!(exp, got);
}

#[test]
#[cfg(unix)]
fn walk_dir_sym_follow_dir() {
    let actual = td("foo", vec![
        td("a", vec![tf("a1"), tf("a2")]),
        td("b", vec![tld("../a", "alink")]),
    ]);
    let followed = td("foo", vec![
        td("a", vec![tf("a1"), tf("a2")]),
        td("b", vec![td("alink", vec![tf("a1"), tf("a2")])]),
    ]);
    let (_tmp, got) = dir_setup_with(&actual, |wd| wd.follow_links(true));
    assert_tree_eq!(followed, got);
}

#[test]
#[cfg(unix)]
fn walk_dir_sym_detect_loop() {
    let actual = td("foo", vec![
        td("a", vec![tlf("../b", "blink"), tf("a1"), tf("a2")]),
        td("b", vec![tlf("../a", "alink")]),
    ]);
    let tmp = tmpdir();
    actual.create_in(tmp.path()).unwrap();
    let got = WalkDir::new(tmp.path())
                      .follow_links(true)
                      .into_iter()
                      .collect::<Result<Vec<_>, _>>();
    match got {
        Ok(x) => panic!("expected loop error, got no error: {:?}", x),
        Err(err @ Error { inner: ErrorInner::Io { .. }, .. }) => {
            panic!("expected loop error, got generic IO error: {:?}", err);
        }
        Err(Error { inner: ErrorInner::Loop { .. }, .. }) => {}
    }
}

#[test]
fn walk_dir_sym_infinite() {
    let actual = tlf("a", "a");
    let tmp = tmpdir();
    actual.create_in(tmp.path()).unwrap();
    let got = WalkDir::new(tmp.path())
                      .follow_links(true)
                      .into_iter()
                      .collect::<Result<Vec<_>, _>>();
    match got {
        Ok(x) => panic!("expected IO error, got no error: {:?}", x),
        Err(Error { inner: ErrorInner::Loop { .. }, .. }) => {
            panic!("expected IO error, but got loop error");
        }
        Err(Error { inner: ErrorInner::Io { .. }, .. }) => {}
    }
}

#[test]
fn walk_dir_min_depth_1() {
    let exp = td("foo", vec![tf("bar")]);
    let (_tmp, got) = dir_setup_with(&exp, |wd| wd.min_depth(1));
    assert_tree_eq!(tf("bar"), got);
}

#[test]
fn walk_dir_min_depth_2() {
    let exp = td("foo", vec![tf("bar"), tf("baz")]);
    let tmp = tmpdir();
    exp.create_in(tmp.path()).unwrap();
    let got = Tree::from_walk_with(tmp.path(), |wd| wd.min_depth(2))
                   .unwrap().unwrap_dir();
    assert_tree_eq!(exp, td("foo", got));
}

#[test]
fn walk_dir_min_depth_3() {
    let exp = td("foo", vec![
        tf("bar"),
        td("abc", vec![tf("xyz")]),
        tf("baz"),
    ]);
    let tmp = tmpdir();
    exp.create_in(tmp.path()).unwrap();
    let got = Tree::from_walk_with(tmp.path(), |wd| wd.min_depth(3))
                   .unwrap().unwrap_dir();
    assert_eq!(vec![tf("xyz")], got);
}

#[test]
fn walk_dir_max_depth_1() {
    let exp = td("foo", vec![tf("bar")]);
    let (_tmp, got) = dir_setup_with(&exp, |wd| wd.max_depth(1));
    assert_tree_eq!(td("foo", vec![]), got);
}

#[test]
fn walk_dir_max_depth_2() {
    let exp = td("foo", vec![tf("bar"), tf("baz")]);
    let (_tmp, got) = dir_setup_with(&exp, |wd| wd.max_depth(1));
    assert_tree_eq!(td("foo", vec![]), got);
}

#[test]
fn walk_dir_max_depth_3() {
    let exp = td("foo", vec![
        tf("bar"),
        td("abc", vec![tf("xyz")]),
        tf("baz"),
    ]);
    let exp_trimmed = td("foo", vec![
        tf("bar"),
        td("abc", vec![]),
        tf("baz"),
    ]);
    let (_tmp, got) = dir_setup_with(&exp, |wd| wd.max_depth(2));
    assert_tree_eq!(exp_trimmed, got);
}

#[test]
fn walk_dir_min_max_depth() {
    let exp = td("foo", vec![
        tf("bar"),
        td("abc", vec![tf("xyz")]),
        tf("baz"),
    ]);
    let tmp = tmpdir();
    exp.create_in(tmp.path()).unwrap();
    let got = Tree::from_walk_with(tmp.path(),
                                   |wd| wd.min_depth(2).max_depth(2))
                   .unwrap().unwrap_dir();
    assert_tree_eq!(
        td("foo", vec![tf("bar"), td("abc", vec![]), tf("baz")]),
        td("foo", got));
}

#[test]
fn walk_dir_skip() {
    let exp = td("foo", vec![
        tf("bar"),
        td("abc", vec![tf("xyz")]),
        tf("baz"),
    ]);
    let tmp = tmpdir();
    exp.create_in(tmp.path()).unwrap();
    let mut got = vec![];
    let mut it = WalkDir::new(tmp.path()).min_depth(1).into_iter();
    loop {
        let dent = match it.next().map(|x| x.unwrap()) {
            None => break,
            Some(dent) => dent,
        };
        let name = dent.file_name().to_str().unwrap().to_owned();
        if name == "abc" {
            it.skip_current_dir();
        }
        got.push(name);
    }
    got.sort();
    assert_eq!(got, vec!["abc", "bar", "baz", "foo"]); // missing xyz!
}

#[test]
fn walk_dir_filter() {
    let exp = td("foo", vec![
        tf("bar"),
        td("abc", vec![tf("fit")]),
        tf("faz"),
    ]);
    let tmp = tmpdir();
    let tmp_path = tmp.path().to_path_buf();
    exp.create_in(tmp.path()).unwrap();
    let it = WalkDir::new(tmp.path()).min_depth(1)
                     .into_iter()
                     .filter_entry(move |d| {
                         let n = d.file_name().to_string_lossy().into_owned();
                         !d.file_type().is_dir()
                         || n.starts_with("f")
                         || d.path() == &*tmp_path
                     });
    let mut got = it.map(|d| d.unwrap().file_name().to_str().unwrap().into())
                    .collect::<Vec<String>>();
    got.sort();
    assert_eq!(got, vec!["bar", "faz", "foo"]);
}

#[test]
fn qc_roundtrip() {
    fn p(exp: Tree) -> bool {
        let (_tmp, got) = dir_setup(&exp);
        exp.canonical() == got.canonical()
    }
    QuickCheck::new()
               .gen(StdGen::new(rand::thread_rng(), 15))
               .tests(1_000)
               .max_tests(10_000)
               .quickcheck(p as fn(Tree) -> bool);
}

// Same as `qc_roundtrip`, but makes sure `follow_links` doesn't change
// the behavior of walking a directory *without* symlinks.
#[test]
fn qc_roundtrip_no_symlinks_with_follow() {
    fn p(exp: Tree) -> bool {
        let (_tmp, got) = dir_setup_with(&exp, |wd| wd.follow_links(true));
        exp.canonical() == got.canonical()
    }
    QuickCheck::new()
               .gen(StdGen::new(rand::thread_rng(), 15))
               .tests(1_000)
               .max_tests(10_000)
               .quickcheck(p as fn(Tree) -> bool);
}

#[test]
fn walk_dir_sort() {
    let exp = td("foo", vec![
        tf("bar"),
        td("abc", vec![tf("fit")]),
        tf("faz"),
    ]);
    let tmp = tmpdir();
    let tmp_path = tmp.path();
    let tmp_len = tmp_path.to_str().unwrap().len();
    exp.create_in(tmp_path).unwrap();
    let it = WalkDir::new(tmp_path).sort_by(|a,b| a.cmp(b)).into_iter();
    let got = it.map(|d| {
        let path = d.unwrap();
        let path = &path.path().to_str().unwrap()[tmp_len..];
        path.replace("\\", "/")
    }).collect::<Vec<String>>();
    assert_eq!(got,
        ["", "/foo", "/foo/abc", "/foo/abc/fit", "/foo/bar", "/foo/faz"]);
}

#[test]
fn walk_dir_sort_small_fd_max() {
    let exp = td("foo", vec![
        tf("bar"),
        td("abc", vec![tf("fit")]),
        tf("faz"),
    ]);
    let tmp = tmpdir();
    let tmp_path = tmp.path();
    let tmp_len = tmp_path.to_str().unwrap().len();
    exp.create_in(tmp_path).unwrap();
    let it =
        WalkDir::new(tmp_path).max_open(1).sort_by(|a,b| a.cmp(b)).into_iter();
    let got = it.map(|d| {
        let path = d.unwrap();
        let path = &path.path().to_str().unwrap()[tmp_len..];
        path.replace("\\", "/")
    }).collect::<Vec<String>>();
    assert_eq!(got,
        ["", "/foo", "/foo/abc", "/foo/abc/fit", "/foo/bar", "/foo/faz"]);
}
