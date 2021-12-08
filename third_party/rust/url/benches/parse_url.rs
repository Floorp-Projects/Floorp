#[macro_use]
extern crate bencher;

extern crate url;

use bencher::{black_box, Bencher};

use url::Url;

fn short(bench: &mut Bencher) {
    let url = "https://example.com/bench";

    bench.bytes = url.len() as u64;
    bench.iter(|| black_box(url).parse::<Url>().unwrap());
}

benchmark_group!(benches, short);
benchmark_main!(benches);
