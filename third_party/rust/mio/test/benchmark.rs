use std::mem;
use mio::net::{AddressFamily, Inet, Inet6, SockAddr, InetAddr, IPv4Addr, SocketType, Dgram, Stream};
use std::io::net::ip::IpAddr;
use native::NativeTaskBuilder;
use std::task::TaskBuilder;
use mio::os::{from_sockaddr};
use time::Instant;
use std::vec::*;
use std::io::timer;

mod nix {
    pub use nix::c_int;
    pub use nix::fcntl::{Fd, O_NONBLOCK, O_CLOEXEC};
    pub use nix::errno::{EWOULDBLOCK, EINPROGRESS};
    pub use nix::sys::socket::*;
    pub use nix::unistd::*;
    pub use nix::sys::epoll::*;
}

fn timed(label: &str, f: ||) {
    let start = Instant::now();
    f();
    let elapsed = start.elapsed();
    println!("  {}: {}", label, elapsed.as_secs() as f64 + elapsed.subsec_nanos() as f64 / 1_000_000_000.0);
}

fn init(saddr: &str) -> (nix::Fd, nix::Fd) {
    let optval = 1i;
    let addr = SockAddr::parse(saddr.as_slice()).expect("could not parse InetAddr");
    let srvfd = nix::socket(nix::AF_INET, nix::SOCK_STREAM, nix::SOCK_CLOEXEC).unwrap();
    nix::setsockopt(srvfd, nix::SOL_SOCKET, nix::SO_REUSEADDR, &optval).unwrap();
    nix::bind(srvfd, &from_sockaddr(&addr)).unwrap();
    nix::listen(srvfd, 256u).unwrap();

    let fd = nix::socket(nix::AF_INET, nix::SOCK_STREAM, nix::SOCK_CLOEXEC | nix::SOCK_NONBLOCK).unwrap();
    let res = nix::connect(fd, &from_sockaddr(&addr));
    let start = Instant::now();
    println!("connecting : {}", res);

    let clifd = nix::accept4(srvfd, nix::SOCK_CLOEXEC | nix::SOCK_NONBLOCK).unwrap();
    let elapsed = start.elapsed();
    println!("accepted : {} - {}", clifd, elapsed.as_secs() as f64 + elapsed.subsec_nanos() as f64 / 1_000_000_000.0);

    (clifd, srvfd)
}

#[test]
fn read_bench() {
    let (clifd, srvfd) = init("10.10.1.5:11111");
    let mut buf = Vec::with_capacity(1600);
    unsafe { buf.set_len(1600); }
    timed("read", || {
        let mut i = 0u;
        while i < 10000000 {
            let res = nix::read(clifd, buf.as_mut_slice());
            assert_eq!(res.unwrap_err().kind, nix::EWOULDBLOCK);
            i = i + 1;
        }
    });
}

#[test]
fn epollctl_bench() {
    let (clifd, srvfd) = init("10.10.1.5:22222");

    let epfd = nix::epoll_create().unwrap();
    let info = nix::EpollEvent { events: nix::EPOLLIN | nix::EPOLLONESHOT | nix::EPOLLET,
                                 data: 0u64 };

    nix::epoll_ctl(epfd, nix::EpollCtlAdd, clifd, &info);

    timed("epoll_ctl", || {
        let mut i = 0u;
        while i < 10000000 {
            nix::epoll_ctl(epfd, nix::EpollCtlMod, clifd, &info);
            i = i + 1;
        }
    });

}
