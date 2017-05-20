
#![feature(test)]
extern crate test;

extern crate odds;
extern crate memchr;
extern crate itertools;

#[macro_use] extern crate lazy_static;

use std::mem::{size_of, size_of_val};
use test::Bencher;
use test::black_box;

use itertools::enumerate;

use odds::slice::shared_prefix;
use odds::stride::Stride;
use odds::slice::unalign::UnalignedIter;

#[bench]
fn find_word_memcmp_ascii(b: &mut Bencher) {
    let words = &*WORDS_ASCII;
    let word = b"the";
    b.iter(|| {
        words.iter().map(|w|
            (w.as_bytes() == &word[..]) as usize
        ).sum::<usize>()
    });
    b.bytes = words.iter().map(|w| w.len() as u64).sum::<u64>()
}

#[bench]
fn find_word_shpfx_ascii(b: &mut Bencher) {
    let words = &*WORDS_ASCII;
    let word = b"the";
    b.iter(|| {
        words.iter().map(|w|
            (shared_prefix(w.as_bytes(), &word[..]) == word.len()) as usize
        ).sum::<usize>()
    });
    b.bytes = words.iter().map(|w| w.len() as u64).sum::<u64>()
}


#[bench]
fn shpfx(bench: &mut Bencher) {
    let a = vec![0u8; 64 * 1024 * 1024];
    let mut b = vec![0u8; 64 * 1024 * 1024];
    const OFF: usize = 47 * 1024 * 1024;
    b[OFF] = 1;

    bench.iter(|| {
        shared_prefix(&a, &b);
    });
    bench.bytes = size_of_val(&b[..OFF]) as u64;
}

#[bench]
fn shpfx_memcmp(bench: &mut Bencher) {
    let a = vec![0u8; 64 * 1024 * 1024];
    let mut b = vec![0u8; 64 * 1024 * 1024];
    const OFF: usize = 47 * 1024 * 1024;
    b[OFF] = 1;

    bench.iter(|| {
        &a[..] == &b[..]
    });
    bench.bytes = size_of_val(&b[..OFF]) as u64;
}

#[bench]
fn shpfx_short(bench: &mut Bencher) {
    let a = vec![0; 64 * 1024];
    let mut b = vec![0; 64 * 1024];
    const OFF: usize = 47 * 1024;
    b[OFF] = 1;

    bench.iter(|| {
        shared_prefix(&a, &b)
    });
    bench.bytes = size_of_val(&b[..OFF]) as u64;
}

#[bench]
fn shpfx_memcmp_short(bench: &mut Bencher) {
    let a = vec![0u8; 64 * 1024];
    let mut b = vec![0u8; 64 * 1024];
    const OFF: usize = 47 * 1024;
    b[OFF] = 1;

    bench.iter(|| {
        &a[..] == &b[..]
    });
    bench.bytes = size_of_val(&b[..OFF]) as u64;
}

fn bench_data() -> Vec<u8> { vec![b'z'; 10_000] }

#[bench]
fn optimized_memchr(b: &mut test::Bencher) {
    let haystack = bench_data();
    let needle = b'a';
    b.iter(|| {
        memchr::memchr(needle, &haystack)
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn odds_memchr(b: &mut Bencher) {
    let haystack = bench_data();
    let needle = b'a';
    b.iter(|| {
        memchr_mockup(needle, &haystack[1..])
    });
    b.bytes = haystack.len() as u64;
}

#[bench]
fn odds_unalign_memchr(b: &mut Bencher) {
    let haystack = bench_data();
    let needle = b'a';
    b.iter(|| {
        memchr_unalign(needle, &haystack[1..])
    });
    b.bytes = haystack.len() as u64;
}

// use truncation
const LO_USIZE: usize = !0 / 0xff;
const HI_USIZE: usize = 0x8080808080808080 as usize;

/// Return `true` if `x` contains any zero byte.
///
/// From *Matters Computational*, J. Arndt
///
/// "The idea is to subtract one from each of the bytes and then look for
/// bytes where the borrow propagated all the way to the most significant
/// bit."
#[inline]
fn contains_zero_byte(x: usize) -> bool {
    x.wrapping_sub(LO_USIZE) & !x & HI_USIZE != 0
}

fn find<T>(pat: T, text: &[T]) -> Option<usize>
    where T: PartialEq
{
    text.iter().position(|x| *x == pat)
}

fn find_shorter_than<Shorter>(pat: u8, text: &[u8]) -> Option<usize> {
    use std::cmp::min;
    let len = min(text.len(), size_of::<Shorter>() - 1);
    let text = &text[..len];
    for i in 0..len {
        if text[i] == pat {
            return Some(i);
        }
    }
    None
}

// quick and dumb memchr copy
fn memchr_mockup(pat: u8, text: &[u8]) -> Option<usize> {
    type T = [usize; 2];
    let block_size = size_of::<T>();
    let (a, b, c) = odds::slice::split_aligned_for::<T>(text);
    if let r @ Some(_) = find_shorter_than::<T>(pat, a) {
        return r;
    }

    let rep = LO_USIZE * (pat as usize);
    let mut reach = None;
    for (i, block) in enumerate(b) {
        let f1 = contains_zero_byte(rep ^ block[0]);
        let f2 = contains_zero_byte(rep ^ block[1]);
        if f1 || f2 {
            reach = Some(i);
            break;
        }
    }
    if let Some(i) = reach {
        find(pat, &text[i * block_size..]).map(|pos| pos + a.len())
    } else {
        find_shorter_than::<T>(pat, c).map(|pos| pos + text.len() - c.len())
    }
}

// quick and dumb memchr copy
#[inline(never)]
fn memchr_unalign(pat: u8, text: &[u8]) -> Option<usize> {
    type T = [usize; 2];
    let mut iter = UnalignedIter::<T>::from_slice(text);
    let rep = LO_USIZE * (pat as usize);
    while let Some(block) = iter.peek_next() {
        let f1 = contains_zero_byte(rep ^ block[0]);
        let f2 = contains_zero_byte(rep ^ block[1]);
        if f1 || f2 {
            break;
        }
        iter.next();
    }
    {
        let tail = iter.tail();
        let block_len = text.len() - tail.len();
        for (j, byte) in tail.enumerate() {
            if byte == pat {
                return Some(block_len + j);
            }
        }
        None
    }
}


#[bench]
fn slice_iter_pos1(b: &mut Bencher)
{
    let xs = black_box(vec![1; 128]);
    b.iter(|| {
        let mut s = 0;
        for elt in &xs {
            s += *elt;
        }
        s
    });
}

#[bench]
fn stride_iter_pos1(b: &mut Bencher)
{
    let xs = black_box(vec![1; 128]);
    b.iter(|| {
        let mut s = 0;
        for elt in Stride::from_slice(&xs, 1) {
            s += *elt;
        }
        s
    });
}

#[bench]
fn stride_iter_rev(b: &mut Bencher)
{
    let xs = black_box(vec![1; 128]);
    b.iter(|| {
        let mut s = 0;
        for elt in Stride::from_slice(&xs, 1).rev() {
            s += *elt;
        }
        s
    });
}

#[bench]
fn stride_iter_neg1(b: &mut Bencher)
{
    let xs = black_box(vec![1; 128]);
    b.iter(|| {
        let mut s = 0;
        for elt in Stride::from_slice(&xs, -1) {
            s += *elt;
        }
        s
    });
}



//


lazy_static! {
    static ref WORDS_ASCII: Vec<String> = {
        LONG.split_whitespace().map(String::from).collect()
    };
    static ref WORDS_CY: Vec<String> = {
        LONG_CY.split_whitespace().map(String::from).collect()
    };
}

#[bench]
fn memchr_words_ascii(b: &mut Bencher) {
    let words = &*WORDS_ASCII;
    let pat = b'a';
    b.iter(|| {
        words.iter().map(|w|
            memchr::memchr(pat, w.as_bytes()).unwrap_or(0)
        ).sum::<usize>()
    });
    b.bytes = words.iter().map(|w| w.len() as u64).sum::<u64>()
}

#[bench]
fn memchr_odds_words_ascii(b: &mut Bencher) {
    let words = &*WORDS_ASCII;
    let pat = b'a';
    b.iter(|| {
        words.iter().map(|w|
            memchr_mockup(pat, w.as_bytes()).unwrap_or(0)
        ).sum::<usize>()
    });
    b.bytes = words.iter().map(|w| w.len() as u64).sum::<u64>()
}

#[bench]
fn memchr_odds_words_cy(b: &mut Bencher) {
    let words = &*WORDS_CY;
    let pat = b'a';
    b.iter(|| {
        words.iter().map(|w|
            memchr_mockup(pat, w.as_bytes()).unwrap_or(0)
        ).sum::<usize>()
    });
    b.bytes = words.iter().map(|w| w.len() as u64).sum::<u64>()
}

#[bench]
fn memchr_unalign_words_ascii(b: &mut Bencher) {
    let words = &*WORDS_ASCII;
    let pat = b'a';
    b.iter(|| {
        words.iter().map(|w|
            memchr_unalign(pat, w.as_bytes()).unwrap_or(0)
        ).sum::<usize>()
    });
    b.bytes = words.iter().map(|w| w.len() as u64).sum::<u64>()
}

#[bench]
fn memchr_unalign_words_cy(b: &mut Bencher) {
    let words = &*WORDS_CY;
    let pat = b'a';
    b.iter(|| {
        words.iter().map(|w|
            memchr_unalign(pat, w.as_bytes()).unwrap_or(0)
        ).sum::<usize>()
    });
    b.bytes = words.iter().map(|w| w.len() as u64).sum::<u64>()
}

static LONG: &'static str = "\
Lorem ipsum dolor sit amet, consectetur adipiscing elit. Suspendisse quis lorem sit amet dolor \
ultricies condimentum. Praesent iaculis purus elit, ac malesuada quam malesuada in. Duis sed orci \
eros. Suspendisse sit amet magna mollis, mollis nunc luctus, imperdiet mi. Integer fringilla non \
sem ut lacinia. Fusce varius tortor a risus porttitor hendrerit. Morbi mauris dui, ultricies nec \
tempus vel, gravida nec quam.

In est dui, tincidunt sed tempus interdum, adipiscing laoreet ante. Etiam tempor, tellus quis \
sagittis interdum, nulla purus mattis sem, quis auctor erat odio ac tellus. In nec nunc sit amet \
diam volutpat molestie at sed ipsum. Vestibulum laoreet consequat vulputate. Integer accumsan \
lorem ac dignissim placerat. Suspendisse convallis faucibus lorem. Aliquam erat volutpat. In vel \
eleifend felis. Sed suscipit nulla lorem, sed mollis est sollicitudin et. Nam fermentum egestas \
interdum. Curabitur ut nisi justo.

Sed sollicitudin ipsum tellus, ut condimentum leo eleifend nec. Cras ut velit ante. Phasellus nec \
mollis odio. Mauris molestie erat in arcu mattis, at aliquet dolor vehicula. Quisque malesuada \
lectus sit amet nisi pretium, a condimentum ipsum porta. Morbi at dapibus diam. Praesent egestas \
est sed risus elementum, eu rutrum metus ultrices. Etiam fermentum consectetur magna, id rutrum \
felis accumsan a. Aliquam ut pellentesque libero. Sed mi nulla, lobortis eu tortor id, suscipit \
ultricies neque. Morbi iaculis sit amet risus at iaculis. Praesent eget ligula quis turpis \
feugiat suscipit vel non arcu. Interdum et malesuada fames ac ante ipsum primis in faucibus. \
Aliquam sit amet placerat lorem.

Cras a lacus vel ante posuere elementum. Nunc est leo, bibendum ut facilisis vel, bibendum at \
mauris. Nullam adipiscing diam vel odio ornare, luctus adipiscing mi luctus. Nulla facilisi. \
Mauris adipiscing bibendum neque, quis adipiscing lectus tempus et. Sed feugiat erat et nisl \
lobortis pharetra. Donec vitae erat enim. Nullam sit amet felis et quam lacinia tincidunt. Aliquam \
suscipit dapibus urna. Sed volutpat urna in magna pulvinar volutpat. Phasellus nec tellus ac diam \
cursus accumsan.

Nam lectus enim, dapibus non nisi tempor, consectetur convallis massa. Maecenas eleifend dictum \
feugiat. Etiam quis mauris vel risus luctus mattis a a nunc. Nullam orci quam, imperdiet id \
vehicula in, porttitor ut nibh. Duis sagittis adipiscing nisl vitae congue. Donec mollis risus eu \
leo suscipit, varius porttitor nulla porta. Pellentesque ut sem nec nisi euismod vehicula. Nulla \
malesuada sollicitudin quam eu fermentum.";

static LONG_CY: &'static str = "\
Брутэ дольорэ компрэхэнжам йн эжт, ючю коммюны дылыктуч эа, квюо льаорыыт вёвындо мэнандря экз. Ед ыюм емпыдит аккюсам, нык дйкит ютенам ад. Хаж аппэтырэ хонэзтатёз нэ. Ад мовэт путант юрбанйтаж вяш.

Коммодо квюальизквюэ абхоррэант нэ ыюм, праэчынт еракюндйа ылаборарэт эю мыа. Нэ квуым жюмо вольуптатибюж вяш, про ыт бонорюм вёвындо, мэя юллюм новум ку. Пропрёаы такематыш атоморюм зыд ан. Эи омнэжквюы оффекйяж компрэхэнжам жят, апыирёан конкыптам ёнкорруптэ ючю ыт.

Жят алёа лэгыры ед, эи мацим оффэндйт вим. Нык хёнк льаборэж йн, зыд прима тимэам ан. Векж нужквюам инимёкюж ты, ыам эа омнеж ырант рэформйданч. Эрож оффекйяж эю вэл.

Ад нам ножтрюд долорюм, еюж ут вэрыар эюрйпйдяч. Квюач аффэрт тинкидюнт про экз, дёкант вольуптатибюж ат зыд. Ыт зыд экшырки констятюам. Квюо квюиж юрбанйтаж ометтантур экз, хёз экз мютат граэкы рыкючабо, нэ прё пюрто элитр пэрпэтюа. Но квюандо минемум ыам.

Амэт лыгимуз ометтантур кюм ан. Витюпырата котёдиэквюэ нам эю, эю вокынт алёквюам льебэравичсы жят. Экз пыртенакж янтэрэсщэт инзтруктеор нам, еюж ад дйкит каючаэ, шэа витаэ конжтетуто ут. Квюач мандамюч кюм ат, но ёнкорруптэ рэформйданч ючю, незл либриз аюдирэ зыд эи. Ты эож аугюэ иреуры льюкяльиюч, мэль алььтыра докэндё омнэжквюы ат. Анёмал жямиляквюы аккоммодары ыам нэ, экз пэрчёус дэфянятйоныс квюо. Эи дуо фюгит маиорюм.

Эвэртё партйэндо пытынтёюм ыюм ан, шэа ку промпта квюаырэндум. Агам дикунт вим ку. Мюкиуж аюдиам тамквюам про ут, ку мыа квюод квюот эррэм, вяш ад номинави зючкёпит янжольэнж. Нык эи пожжёт путант эффякиантур. Ку еюж нощтыр контынтёонэж. Кюм йужто харюм ёужто ад, ыюм оратио квюоджё экз.

Чонэт факэтэ кюм ан, вэре факэр зальютатуж мэя но. Ыюм ут зальы эффикеэнди, экз про алиё конжыквуюнтюр. Квуй ыльит хабымуч ты, алёа омнэжквюы мандамюч шэа ыт, пльакырат аккюжамюз нэ мэль. Хаж нэ партым нюмквуам прёнкипыз, ат импэрдеэт форынчйбюж кончэктэтюыр шэа. Пльакырат рэформйданч эи векж, ючю дюиж фюйзчыт эи.

Экз квюо ажжюм аугюэ, ат нык мёнём анёмал кытэрож. Кюм выльёт эрюдитя эа. Йн порро малйж кончэктэтюыр хёз, жят кашы эрюдитя ат. Эа вяш мацим пыртенакж, но порро утамюр дяшзынтиыт кюм. Ыт мютат зючкёпит эож, нэ про еракюндйа котёдиэквюэ. Квуй лаудым плььатонэм ед, ку вим ножтрюм лаборамюз.

Вёжи янвыняры хаж ед, ты нолюёжжэ индоктум квуй. Квюач тебиквюэ ут жят, тальэ адхюк убяквюэ йн эож. Ыррор бландит вяш ан, ютроквюы нолюёжжэ констятюам йн ыюм, жят эи прима нобёз тхэопхражтуз. Ты дёкант дэльэнйт нолюёжжэ пэр, молыжтйаы модыратиюз интыллыгам ку мэль.

Ад ылаборарэт конжыквуюнтюр ентырпрытаряш прё, факэтэ лыгэндоч окюррырэт вим ад, элитр рэформйданч квуй ед. Жюмо зальы либриз мэя ты. Незл зюаз видишчы ан ыюм, но пожжэ молыжтйаы мэль. Фиэрэнт адипижкй ометтантур квюо экз. Ут мольлиз пырикюлёз квуй. Ыт квюиж граэко рыпудяары жят, вим магна обльйквюэ контынтёонэж эю, ты шэа эним компльыктётюр.
";

