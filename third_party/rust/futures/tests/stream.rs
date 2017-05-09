#[macro_use]
extern crate futures;

use futures::{Poll, Future, Stream, Sink};
use futures::executor;
use futures::future::{ok, err};
use futures::stream::{iter, Peekable, BoxStream};
use futures::sync::oneshot;
use futures::sync::mpsc;

mod support;
use support::*;


fn list() -> BoxStream<i32, u32> {
    let (tx, rx) = mpsc::channel(1);
    tx.send(Ok(1))
      .and_then(|tx| tx.send(Ok(2)))
      .and_then(|tx| tx.send(Ok(3)))
      .forget();
    rx.then(|r| r.unwrap()).boxed()
}

fn err_list() -> BoxStream<i32, u32> {
    let (tx, rx) = mpsc::channel(1);
    tx.send(Ok(1))
      .and_then(|tx| tx.send(Ok(2)))
      .and_then(|tx| tx.send(Err(3)))
      .forget();
    rx.then(|r| r.unwrap()).boxed()
}

#[test]
fn map() {
    assert_done(|| list().map(|a| a + 1).collect(), Ok(vec![2, 3, 4]));
}

#[test]
fn map_err() {
    assert_done(|| err_list().map_err(|a| a + 1).collect(), Err(4));
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
struct FromErrTest(u32);

impl From<u32> for FromErrTest {
    fn from(i: u32) -> FromErrTest {
        FromErrTest(i)
    }
}

#[test]
fn from_err() {
    assert_done(|| err_list().from_err().collect(), Err(FromErrTest(3)));
}

#[test]
fn fold() {
    assert_done(|| list().fold(0, |a, b| ok::<i32, u32>(a + b)), Ok(6));
    assert_done(|| err_list().fold(0, |a, b| ok::<i32, u32>(a + b)), Err(3));
}

#[test]
fn filter() {
    assert_done(|| list().filter(|a| *a % 2 == 0).collect(), Ok(vec![2]));
}

#[test]
fn filter_map() {
    assert_done(|| list().filter_map(|x| {
        if x % 2 == 0 {
            Some(x + 10)
        } else {
            None
        }
    }).collect(), Ok(vec![12]));
}

#[test]
fn and_then() {
    assert_done(|| list().and_then(|a| Ok(a + 1)).collect(), Ok(vec![2, 3, 4]));
    assert_done(|| list().and_then(|a| err::<i32, u32>(a as u32)).collect(),
                Err(1));
}

#[test]
fn then() {
    assert_done(|| list().then(|a| a.map(|e| e + 1)).collect(), Ok(vec![2, 3, 4]));

}

#[test]
fn or_else() {
    assert_done(|| err_list().or_else(|a| {
        ok::<i32, u32>(a as i32)
    }).collect(), Ok(vec![1, 2, 3]));
}

#[test]
fn flatten() {
    assert_done(|| list().map(|_| list()).flatten().collect(),
                Ok(vec![1, 2, 3, 1, 2, 3, 1, 2, 3]));

}

#[test]
fn skip() {
    assert_done(|| list().skip(2).collect(), Ok(vec![3]));
}

#[test]
fn skip_passes_errors_through() {
    let mut s = iter(vec![Err(1), Err(2), Ok(3), Ok(4), Ok(5)])
        .skip(1)
        .wait();
    assert_eq!(s.next(), Some(Err(1)));
    assert_eq!(s.next(), Some(Err(2)));
    assert_eq!(s.next(), Some(Ok(4)));
    assert_eq!(s.next(), Some(Ok(5)));
    assert_eq!(s.next(), None);
}

#[test]
fn skip_while() {
    assert_done(|| list().skip_while(|e| Ok(*e % 2 == 1)).collect(),
                Ok(vec![2, 3]));
}
#[test]
fn take() {
    assert_done(|| list().take(2).collect(), Ok(vec![1, 2]));
}

#[test]
fn take_while() {
    assert_done(|| list().take_while(|e| Ok(*e < 3)).collect(),
                Ok(vec![1, 2]));
}

#[test]
fn take_passes_errors_through() {
    let mut s = iter(vec![Err(1), Err(2), Ok(3), Ok(4), Err(4)])
        .take(1)
        .wait();
    assert_eq!(s.next(), Some(Err(1)));
    assert_eq!(s.next(), Some(Err(2)));
    assert_eq!(s.next(), Some(Ok(3)));
    assert_eq!(s.next(), None);

    let mut s = iter(vec![Ok(1), Err(2)]).take(1).wait();
    assert_eq!(s.next(), Some(Ok(1)));
    assert_eq!(s.next(), None);
}

#[test]
fn peekable() {
    assert_done(|| list().peekable().collect(), Ok(vec![1, 2, 3]));
}

#[test]
fn fuse() {
    let mut stream = list().fuse().wait();
    assert_eq!(stream.next(), Some(Ok(1)));
    assert_eq!(stream.next(), Some(Ok(2)));
    assert_eq!(stream.next(), Some(Ok(3)));
    assert_eq!(stream.next(), None);
    assert_eq!(stream.next(), None);
    assert_eq!(stream.next(), None);
}

#[test]
fn buffered() {
    let (tx, rx) = mpsc::channel(1);
    let (a, b) = oneshot::channel::<u32>();
    let (c, d) = oneshot::channel::<u32>();

    tx.send(b.map_err(|_| ()).boxed())
      .and_then(|tx| tx.send(d.map_err(|_| ()).boxed()))
      .forget();

    let mut rx = rx.buffered(2);
    sassert_empty(&mut rx);
    c.send(3).unwrap();
    sassert_empty(&mut rx);
    a.send(5).unwrap();
    let mut rx = rx.wait();
    assert_eq!(rx.next(), Some(Ok(5)));
    assert_eq!(rx.next(), Some(Ok(3)));
    assert_eq!(rx.next(), None);

    let (tx, rx) = mpsc::channel(1);
    let (a, b) = oneshot::channel::<u32>();
    let (c, d) = oneshot::channel::<u32>();

    tx.send(b.map_err(|_| ()).boxed())
      .and_then(|tx| tx.send(d.map_err(|_| ()).boxed()))
      .forget();

    let mut rx = rx.buffered(1);
    sassert_empty(&mut rx);
    c.send(3).unwrap();
    sassert_empty(&mut rx);
    a.send(5).unwrap();
    let mut rx = rx.wait();
    assert_eq!(rx.next(), Some(Ok(5)));
    assert_eq!(rx.next(), Some(Ok(3)));
    assert_eq!(rx.next(), None);
}

#[test]
fn unordered() {
    let (tx, rx) = mpsc::channel(1);
    let (a, b) = oneshot::channel::<u32>();
    let (c, d) = oneshot::channel::<u32>();

    tx.send(b.map_err(|_| ()).boxed())
      .and_then(|tx| tx.send(d.map_err(|_| ()).boxed()))
      .forget();

    let mut rx = rx.buffer_unordered(2);
    sassert_empty(&mut rx);
    let mut rx = rx.wait();
    c.send(3).unwrap();
    assert_eq!(rx.next(), Some(Ok(3)));
    a.send(5).unwrap();
    assert_eq!(rx.next(), Some(Ok(5)));
    assert_eq!(rx.next(), None);

    let (tx, rx) = mpsc::channel(1);
    let (a, b) = oneshot::channel::<u32>();
    let (c, d) = oneshot::channel::<u32>();

    tx.send(b.map_err(|_| ()).boxed())
      .and_then(|tx| tx.send(d.map_err(|_| ()).boxed()))
      .forget();

    // We don't even get to see `c` until `a` completes.
    let mut rx = rx.buffer_unordered(1);
    sassert_empty(&mut rx);
    c.send(3).unwrap();
    sassert_empty(&mut rx);
    a.send(5).unwrap();
    let mut rx = rx.wait();
    assert_eq!(rx.next(), Some(Ok(5)));
    assert_eq!(rx.next(), Some(Ok(3)));
    assert_eq!(rx.next(), None);
}

#[test]
fn zip() {
    assert_done(|| list().zip(list()).collect(),
                Ok(vec![(1, 1), (2, 2), (3, 3)]));
    assert_done(|| list().zip(list().take(2)).collect(),
                Ok(vec![(1, 1), (2, 2)]));
    assert_done(|| list().take(2).zip(list()).collect(),
                Ok(vec![(1, 1), (2, 2)]));
    assert_done(|| err_list().zip(list()).collect(), Err(3));
    assert_done(|| list().zip(list().map(|x| x + 1)).collect(),
                Ok(vec![(1, 2), (2, 3), (3, 4)]));
}

#[test]
fn peek() {
    struct Peek {
        inner: Peekable<BoxStream<i32, u32>>
    }

    impl Future for Peek {
        type Item = ();
        type Error = u32;

        fn poll(&mut self) -> Poll<(), u32> {
            {
                let res = try_ready!(self.inner.peek());
                assert_eq!(res, Some(&1));
            }
            assert_eq!(self.inner.peek().unwrap(), Some(&1).into());
            assert_eq!(self.inner.poll().unwrap(), Some(1).into());
            Ok(().into())
        }
    }

    Peek {
        inner: list().peekable(),
    }.wait().unwrap()
}

#[test]
fn wait() {
    assert_eq!(list().wait().collect::<Result<Vec<_>, _>>(),
               Ok(vec![1, 2, 3]));
}

#[test]
fn chunks() {
    assert_done(|| list().chunks(3).collect(), Ok(vec![vec![1, 2, 3]]));
    assert_done(|| list().chunks(1).collect(), Ok(vec![vec![1], vec![2], vec![3]]));
    assert_done(|| list().chunks(2).collect(), Ok(vec![vec![1, 2], vec![3]]));
    let mut list = executor::spawn(err_list().chunks(3));
    let i = list.wait_stream().unwrap().unwrap();
    assert_eq!(i, vec![1, 2]);
    let i = list.wait_stream().unwrap().unwrap_err();
    assert_eq!(i, 3);
}

#[test]
#[should_panic]
fn chunks_panic_on_cap_zero() {
    let _ = list().chunks(0);
}

#[test]
fn select() {
    let a = iter(vec![Ok::<_, u32>(1), Ok(2), Ok(3)]);
    let b = iter(vec![Ok(4), Ok(5), Ok(6)]);
    assert_done(|| a.select(b).collect(), Ok(vec![1, 4, 2, 5, 3, 6]));

    let a = iter(vec![Ok::<_, u32>(1), Ok(2), Ok(3)]);
    let b = iter(vec![Ok(1), Ok(2)]);
    assert_done(|| a.select(b).collect(), Ok(vec![1, 1, 2, 2, 3]));

    let a = iter(vec![Ok(1), Ok(2)]);
    let b = iter(vec![Ok::<_, u32>(1), Ok(2), Ok(3)]);
    assert_done(|| a.select(b).collect(), Ok(vec![1, 1, 2, 2, 3]));
}

#[test]
fn forward() {
    let v = Vec::new();
    let v = iter(vec![Ok::<_, ()>(0), Ok(1)]).forward(v).wait().unwrap().1;
    assert_eq!(v, vec![0, 1]);

    let v = iter(vec![Ok::<_, ()>(2), Ok(3)]).forward(v).wait().unwrap().1;
    assert_eq!(v, vec![0, 1, 2, 3]);

    assert_done(move || iter(vec![Ok(4), Ok(5)]).forward(v).map(|(_, s)| s),
                Ok::<_, ()>(vec![0, 1, 2, 3, 4, 5]));
}

#[test]
fn concat() {
    let a = iter(vec![Ok::<_, ()>(vec![1, 2, 3]), Ok(vec![4, 5, 6]), Ok(vec![7, 8, 9])]);
    assert_done(move || a.concat(), Ok(vec![1, 2, 3, 4, 5, 6, 7, 8, 9]));

    let b = iter(vec![Ok::<_, ()>(vec![1, 2, 3]), Err(()), Ok(vec![7, 8, 9])]);
    assert_done(move || b.concat(), Err(()));
}
