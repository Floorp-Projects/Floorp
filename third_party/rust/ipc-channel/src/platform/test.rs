// Copyright 2015 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use platform::{self, OsIpcChannel, OsIpcReceiverSet};
use platform::{OsIpcSharedMemory};
use std::sync::Arc;
use std::time::{Duration, Instant};
use std::thread;

#[cfg(not(any(feature = "force-inprocess", target_os = "windows", target_os = "android")))]
use libc;
use platform::{OsIpcSender, OsIpcOneShotServer};
#[cfg(not(any(feature = "force-inprocess", target_os = "windows", target_os = "android")))]
use test::{fork, Wait};

#[test]
fn simple() {
    let (tx, rx) = platform::channel().unwrap();
    let data: &[u8] = b"1234567";
    tx.send(data, Vec::new(), Vec::new()).unwrap();
    let (mut received_data, received_channels, received_shared_memory) = rx.recv().unwrap();
    received_data.truncate(7);
    assert_eq!((&received_data[..], received_channels, received_shared_memory),
               (data, Vec::new(), Vec::new()));
}

#[test]
fn sender_transfer() {
    let (super_tx, super_rx) = platform::channel().unwrap();
    let (sub_tx, sub_rx) = platform::channel().unwrap();
    let data: &[u8] = b"foo";
    super_tx.send(data, vec![OsIpcChannel::Sender(sub_tx)], vec![]).unwrap();
    let (_, mut received_channels, _) = super_rx.recv().unwrap();
    assert_eq!(received_channels.len(), 1);
    let sub_tx = received_channels.pop().unwrap().to_sender();
    sub_tx.send(data, vec![], vec![]).unwrap();
    let (mut received_data, received_channels, received_shared_memory_regions) =
        sub_rx.recv().unwrap();
    received_data.truncate(3);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[test]
fn receiver_transfer() {
    let (super_tx, super_rx) = platform::channel().unwrap();
    let (sub_tx, sub_rx) = platform::channel().unwrap();
    let data: &[u8] = b"foo";
    super_tx.send(data, vec![OsIpcChannel::Receiver(sub_rx)], vec![]).unwrap();
    let (_, mut received_channels, _) = super_rx.recv().unwrap();
    assert_eq!(received_channels.len(), 1);
    let sub_rx = received_channels.pop().unwrap().to_receiver();
    sub_tx.send(data, vec![], vec![]).unwrap();
    let (mut received_data, received_channels, received_shared_memory_regions) =
        sub_rx.recv().unwrap();
    received_data.truncate(3);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[test]
fn multisender_transfer() {
    let (super_tx, super_rx) = platform::channel().unwrap();
    let (sub0_tx, sub0_rx) = platform::channel().unwrap();
    let (sub1_tx, sub1_rx) = platform::channel().unwrap();
    let data: &[u8] = b"asdfasdf";
    super_tx.send(data,
                  vec![OsIpcChannel::Sender(sub0_tx), OsIpcChannel::Sender(sub1_tx)],
                  vec![]).unwrap();
    let (_, mut received_channels, _) = super_rx.recv().unwrap();
    assert_eq!(received_channels.len(), 2);

    let sub0_tx = received_channels.remove(0).to_sender();
    sub0_tx.send(data, vec![], vec![]).unwrap();
    let (mut received_data, received_subchannels, received_shared_memory_regions) =
        sub0_rx.recv().unwrap();
    received_data.truncate(8);
    assert_eq!((&received_data[..], received_subchannels, received_shared_memory_regions),
               (data, vec![], vec![]));

    let sub1_tx = received_channels.remove(0).to_sender();
    sub1_tx.send(data, vec![], vec![]).unwrap();
    let (mut received_data, received_subchannels, received_shared_memory_regions) =
        sub1_rx.recv().unwrap();
    received_data.truncate(8);
    assert_eq!((&received_data[..], received_subchannels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[test]
fn medium_data() {
    let data: Vec<u8> = (0..65536).map(|i| (i % 251) as u8).collect();
    let data: &[u8] = &data[..];
    let (tx, rx) = platform::channel().unwrap();
    tx.send(data, vec![], vec![]).unwrap();
    let (mut received_data, received_channels, received_shared_memory_regions) =
        rx.recv().unwrap();
    received_data.truncate(65536);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (&data[..], vec![], vec![]));
}

#[test]
fn medium_data_with_sender_transfer() {
    let data: Vec<u8> = (0..65536).map(|i| (i % 251) as u8).collect();
    let data: &[u8] = &data[..];
    let (super_tx, super_rx) = platform::channel().unwrap();
    let (sub_tx, sub_rx) = platform::channel().unwrap();
    super_tx.send(data, vec![OsIpcChannel::Sender(sub_tx)], vec![]).unwrap();
    let (_, mut received_channels, _) = super_rx.recv().unwrap();
    assert_eq!(received_channels.len(), 1);
    let sub_tx = received_channels.pop().unwrap().to_sender();
    sub_tx.send(data, vec![], vec![]).unwrap();
    let (mut received_data, received_channels, received_shared_memory_regions) =
        sub_rx.recv().unwrap();
    received_data.truncate(65536);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[test]
fn big_data() {
    let (tx, rx) = platform::channel().unwrap();
    let thread = thread::spawn(move || {
        let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
        let data: &[u8] = &data[..];
        tx.send(data, vec![], vec![]).unwrap();
    });
    let (mut received_data, received_channels, received_shared_memory_regions) =
        rx.recv().unwrap();
    let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
    let data: &[u8] = &data[..];
    received_data.truncate(1024 * 1024);
    assert_eq!(received_data.len(), data.len());
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (&data[..], vec![], vec![]));
    thread.join().unwrap();
}

#[test]
fn big_data_with_sender_transfer() {
    let (super_tx, super_rx) = platform::channel().unwrap();
    let (sub_tx, sub_rx) = platform::channel().unwrap();
    let thread = thread::spawn(move || {
        let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
        let data: &[u8] = &data[..];
        super_tx.send(data, vec![OsIpcChannel::Sender(sub_tx)], vec![]).unwrap();
    });
    let (mut received_data, mut received_channels, received_shared_memory_regions) =
        super_rx.recv().unwrap();
    let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
    let data: &[u8] = &data[..];
    received_data.truncate(1024 * 1024);
    assert_eq!(received_data.len(), data.len());
    assert_eq!(&received_data[..], &data[..]);
    assert_eq!(received_channels.len(), 1);
    assert_eq!(received_shared_memory_regions.len(), 0);

    let data: Vec<u8> = (0..65536).map(|i| (i % 251) as u8).collect();
    let data: &[u8] = &data[..];
    let sub_tx = received_channels[0].to_sender();
    sub_tx.send(data, vec![], vec![]).unwrap();
    let (mut received_data, received_channels, received_shared_memory_regions) =
        sub_rx.recv().unwrap();
    received_data.truncate(65536);
    assert_eq!(received_data.len(), data.len());
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (&data[..], vec![], vec![]));
    thread.join().unwrap();
}

#[cfg(all(not(feature = "force-inprocess"), any(target_os = "linux",
                                                target_os = "freebsd")))]
fn with_n_fds(n: usize, size: usize) {
    let (sender_fds, receivers): (Vec<_>, Vec<_>) = (0..n).map(|_| platform::channel().unwrap())
                                                    .map(|(tx, rx)| (OsIpcChannel::Sender(tx), rx))
                                                    .unzip();
    let (super_tx, super_rx) = platform::channel().unwrap();

    let data: Vec<u8> = (0..size).map(|i| (i % 251) as u8).collect();
    super_tx.send(&data[..], sender_fds, vec![]).unwrap();
    let (mut received_data, received_channels, received_shared_memory_regions) =
        super_rx.recv().unwrap();

    received_data.truncate(size);
    assert_eq!(received_data.len(), data.len());
    assert_eq!(&received_data[..], &data[..]);
    assert_eq!(received_channels.len(), receivers.len());
    assert_eq!(received_shared_memory_regions.len(), 0);

    let data: Vec<u8> = (0..65536).map(|i| (i % 251) as u8).collect();
    for (mut sender_fd, sub_rx) in received_channels.into_iter().zip(receivers.into_iter()) {
        let sub_tx = sender_fd.to_sender();
        sub_tx.send(&data[..], vec![], vec![]).unwrap();
        let (mut received_data, received_channels, received_shared_memory_regions) =
            sub_rx.recv().unwrap();
        received_data.truncate(65536);
        assert_eq!(received_data.len(), data.len());
        assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
                   (&data[..], vec![], vec![]));
    }
}

// These tests only apply to platforms that need fragmentation.
#[cfg(all(not(feature = "force-inprocess"), any(target_os = "linux",
                                                target_os = "freebsd")))]
mod fragment_tests {
    use platform;
    use super::with_n_fds;

    lazy_static! {
        static ref FRAGMENT_SIZE: usize = {
            platform::OsIpcSender::get_max_fragment_size()
        };
    }

    #[test]
    fn full_packet() {
        with_n_fds(0, *FRAGMENT_SIZE);
    }

    #[test]
    fn full_packet_with_1_fds() {
        with_n_fds(1, *FRAGMENT_SIZE);
    }
    #[test]
    fn full_packet_with_2_fds() {
        with_n_fds(2, *FRAGMENT_SIZE);
    }
    #[test]
    fn full_packet_with_3_fds() {
        with_n_fds(3, *FRAGMENT_SIZE);
    }
    #[test]
    fn full_packet_with_4_fds() {
        with_n_fds(4, *FRAGMENT_SIZE);
    }
    #[test]
    fn full_packet_with_5_fds() {
        with_n_fds(5, *FRAGMENT_SIZE);
    }
    #[test]
    fn full_packet_with_6_fds() {
        with_n_fds(6, *FRAGMENT_SIZE);
    }

    // MAX_FDS_IN_CMSG is currently 64.
    #[test]
    fn full_packet_with_64_fds() {
        with_n_fds(64, *FRAGMENT_SIZE);
    }

    #[test]
    fn overfull_packet() {
        with_n_fds(0, *FRAGMENT_SIZE + 1);
    }

    // In fragmented transfers, one FD is used up for the dedicated channel.
    #[test]
    fn overfull_packet_with_63_fds() {
        with_n_fds(63, *FRAGMENT_SIZE + 1);
    }
}

macro_rules! create_big_data_with_n_fds {
    ($name:ident, $n:expr) => (
        #[test]
        fn $name() {
            let (sender_fds, receivers): (Vec<_>, Vec<_>) = (0..$n).map(|_| platform::channel().unwrap())
                                                            .map(|(tx, rx)| (OsIpcChannel::Sender(tx), rx))
                                                            .unzip();
            let (super_tx, super_rx) = platform::channel().unwrap();
            let thread = thread::spawn(move || {
                let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
                let data: &[u8] = &data[..];
                super_tx.send(data, sender_fds, vec![]).unwrap();
            });
            let (mut received_data, received_channels, received_shared_memory_regions) =
                super_rx.recv().unwrap();
            thread.join().unwrap();

            let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
            let data: &[u8] = &data[..];
            received_data.truncate(1024 * 1024);
            assert_eq!(received_data.len(), data.len());
            assert_eq!(&received_data[..], &data[..]);
            assert_eq!(received_channels.len(), receivers.len());
            assert_eq!(received_shared_memory_regions.len(), 0);

            let data: Vec<u8> = (0..65536).map(|i| (i % 251) as u8).collect();
            let data: &[u8] = &data[..];
            for (mut sender_fd, sub_rx) in received_channels.into_iter().zip(receivers.into_iter()) {
                let sub_tx = sender_fd.to_sender();
                sub_tx.send(data, vec![], vec![]).unwrap();
                let (mut received_data, received_channels, received_shared_memory_regions) =
                    sub_rx.recv().unwrap();
                received_data.truncate(65536);
                assert_eq!(received_data.len(), data.len());
                assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
                           (&data[..], vec![], vec![]));
            }
        }
    )
}

create_big_data_with_n_fds!(big_data_with_0_fds, 0);
create_big_data_with_n_fds!(big_data_with_1_fds, 1);
create_big_data_with_n_fds!(big_data_with_2_fds, 2);
create_big_data_with_n_fds!(big_data_with_3_fds, 3);
create_big_data_with_n_fds!(big_data_with_4_fds, 4);
create_big_data_with_n_fds!(big_data_with_5_fds, 5);
create_big_data_with_n_fds!(big_data_with_6_fds, 6);

#[test]
fn concurrent_senders() {
    let num_senders = 3;

    let (tx, rx) = platform::channel().unwrap();

    let threads: Vec<_> = (0..num_senders).map(|i| {
        let tx = tx.clone();
        thread::spawn(move || {
            let data: Vec<u8> = (0.. 1024 * 1024).map(|j| (j % 13) as u8 | i << 4).collect();
            let data: &[u8] = &data[..];
            tx.send(data, vec![], vec![]).unwrap();
        })
    }).collect();

    let mut received_vals: Vec<u8> = vec![];
    for _ in 0..num_senders {
        let (mut received_data, received_channels, received_shared_memory_regions) =
            rx.recv().unwrap();
        let val = received_data[0] >> 4;
        received_vals.push(val);
        let data: Vec<u8> = (0.. 1024 * 1024).map(|j| (j % 13) as u8 | val << 4).collect();
        let data: &[u8] = &data[..];
        received_data.truncate(1024 * 1024);
        assert_eq!(received_data.len(), data.len());
        assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
                   (&data[..], vec![], vec![]));
    }
    assert!(rx.try_recv().is_err()); // There should be no further messages pending.
    received_vals.sort();
    assert_eq!(received_vals, (0..num_senders).collect::<Vec<_>>()); // Got exactly the values we sent.

    for thread in threads {
        thread.join().unwrap();
    }
}

#[test]
fn receiver_set() {
    let (tx0, rx0) = platform::channel().unwrap();
    let (tx1, rx1) = platform::channel().unwrap();
    let mut rx_set = OsIpcReceiverSet::new().unwrap();
    let rx0_id = rx_set.add(rx0).unwrap();
    let rx1_id = rx_set.add(rx1).unwrap();

    let data: &[u8] = b"1234567";
    tx0.send(data, vec![], vec![]).unwrap();
    let (received_id, mut received_data, _, _) =
        rx_set.select().unwrap().into_iter().next().unwrap().unwrap();
    received_data.truncate(7);
    assert_eq!(received_id, rx0_id);
    assert_eq!(received_data, data);

    tx1.send(data, vec![], vec![]).unwrap();
    let (received_id, mut received_data, _, _) =
        rx_set.select().unwrap().into_iter().next().unwrap().unwrap();
    received_data.truncate(7);
    assert_eq!(received_id, rx1_id);
    assert_eq!(received_data, data);

    tx0.send(data, vec![], vec![]).unwrap();
    tx1.send(data, vec![], vec![]).unwrap();
    let (mut received0, mut received1) = (false, false);
    while !received0 || !received1 {
        for result in rx_set.select().unwrap().into_iter() {
            let (received_id, mut received_data, _, _) = result.unwrap();
            received_data.truncate(7);
            assert_eq!(received_data, data);
            assert!(received_id == rx0_id || received_id == rx1_id);
            if received_id == rx0_id {
                assert!(!received0);
                received0 = true;
            } else if received_id == rx1_id {
                assert!(!received1);
                received1 = true;
            }
        }
    }
}

#[cfg(not(any(feature = "force-inprocess", target_os = "android")))]
#[test]
fn server_accept_first() {
    let (server, name) = OsIpcOneShotServer::new().unwrap();
    let data: &[u8] = b"1234567";

    thread::spawn(move || {
        thread::sleep(Duration::from_millis(30));
        let tx = OsIpcSender::connect(name).unwrap();
        tx.send(data, vec![], vec![]).unwrap();
    });

    let (_, mut received_data, received_channels, received_shared_memory_regions) =
        server.accept().unwrap();
    received_data.truncate(7);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[test]
fn server_connect_first() {
    let (server, name) = OsIpcOneShotServer::new().unwrap();
    let data: &[u8] = b"1234567";

    thread::spawn(move || {
        let tx = OsIpcSender::connect(name).unwrap();
        tx.send(data, vec![], vec![]).unwrap();
    });

    thread::sleep(Duration::from_millis(30));
    let (_, mut received_data, received_channels, received_shared_memory_regions) =
        server.accept().unwrap();
    received_data.truncate(7);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[cfg(not(any(feature = "force-inprocess", target_os = "windows", target_os = "android")))]
#[test]
fn cross_process() {
    let (server, name) = OsIpcOneShotServer::new().unwrap();
    let data: &[u8] = b"1234567";

    let child_pid = unsafe { fork(|| {
        let tx = OsIpcSender::connect(name).unwrap();
        tx.send(data, vec![], vec![]).unwrap();
        libc::exit(0);
    })};

    let (_, mut received_data, received_channels, received_shared_memory_regions) =
        server.accept().unwrap();
    child_pid.wait();
    received_data.truncate(7);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[cfg(not(any(feature = "force-inprocess", target_os = "windows", target_os = "android")))]
#[test]
fn cross_process_sender_transfer() {
    let (server, name) = OsIpcOneShotServer::new().unwrap();

    let child_pid = unsafe { fork(|| {
        let super_tx = OsIpcSender::connect(name).unwrap();
        let (sub_tx, sub_rx) = platform::channel().unwrap();
        let data: &[u8] = b"foo";
        super_tx.send(data, vec![OsIpcChannel::Sender(sub_tx)], vec![]).unwrap();
        sub_rx.recv().unwrap();
        let data: &[u8] = b"bar";
        super_tx.send(data, vec![], vec![]).unwrap();
        libc::exit(0);
    })};

    let (super_rx, _, mut received_channels, _) = server.accept().unwrap();
    assert_eq!(received_channels.len(), 1);
    let sub_tx = received_channels.pop().unwrap().to_sender();
    let data: &[u8] = b"baz";
    sub_tx.send(data, vec![], vec![]).unwrap();

    let data: &[u8] = b"bar";
    let (mut received_data, received_channels, received_shared_memory_regions) =
        super_rx.recv().unwrap();
    child_pid.wait();
    received_data.truncate(3);
    assert_eq!((&received_data[..], received_channels, received_shared_memory_regions),
               (data, vec![], vec![]));
}

#[test]
fn no_senders_notification() {
    let (sender, receiver) = platform::channel().unwrap();
    drop(sender);
    let result = receiver.recv();
    assert!(result.is_err());
    assert!(result.unwrap_err().channel_is_closed());
}

#[test]
fn shared_memory() {
    let (tx, rx) = platform::channel().unwrap();
    let data: &[u8] = b"1234567";
    let shmem_data = OsIpcSharedMemory::from_byte(0xba, 1024 * 1024);
    tx.send(data, vec![], vec![shmem_data]).unwrap();
    let (mut received_data, received_channels, received_shared_memory) = rx.recv().unwrap();
    received_data.truncate(7);
    assert_eq!((&received_data[..], received_channels), (data, Vec::new()));
    assert_eq!(received_shared_memory[0].len(), 1024 * 1024);
    assert!(received_shared_memory[0].iter().all(|byte| *byte == 0xba));
}

#[test]
fn shared_memory_clone() {
    let shmem_data_0 = OsIpcSharedMemory::from_byte(0xba, 1024 * 1024);
    let shmem_data_1 = shmem_data_0.clone();
    assert_eq!(&shmem_data_0[..], &shmem_data_1[..]);
}

#[test]
fn try_recv() {
    let (tx, rx) = platform::channel().unwrap();
    assert!(rx.try_recv().is_err());
    let data: &[u8] = b"1234567";
    tx.send(data, Vec::new(), Vec::new()).unwrap();
    let (mut received_data, received_channels, received_shared_memory) = rx.try_recv().unwrap();
    received_data.truncate(7);
    assert_eq!((&received_data[..], received_channels, received_shared_memory),
               (data, Vec::new(), Vec::new()));
    assert!(rx.try_recv().is_err());
}

#[test]
fn try_recv_large() {
    let (tx, rx) = platform::channel().unwrap();
    assert!(rx.try_recv().is_err());

    let thread = thread::spawn(move || {
        let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
        let data: &[u8] = &data[..];
        tx.send(data, vec![], vec![]).unwrap();
    });

    let mut result;
    while {
        result = rx.try_recv();
        result.is_err()
    } {}
    thread.join().unwrap();
    let (mut received_data, received_channels, received_shared_memory) = result.unwrap();

    let data: Vec<u8> = (0.. 1024 * 1024).map(|i| (i % 251) as u8).collect();
    let data: &[u8] = &data[..];
    received_data.truncate(1024 * 1024);
    assert_eq!((&received_data[..], received_channels, received_shared_memory),
               (data, vec![], vec![]));
    assert!(rx.try_recv().is_err());
}

#[test]
fn try_recv_large_delayed() {
    // These settings work well on my system when doing cargo test --release.
    // Haven't found a good way to test this with non-release builds...
    let num_senders = 10;
    let thread_delay = 50;
    let msg_size = 512 * 1024;

    let delay = {
        fn delay_iterations(iterations: u64) {
            // Let's hope rustc won't ever be able to optimise this away...
            let mut v = vec![];
            for _ in 0..iterations {
                v.push(0);
                v.pop();
            }
        }

        let iterations_per_ms;
        {
            let (mut iterations, mut time_per_run) = (1u64, Duration::new(0, 0));
            while time_per_run < Duration::new(0, 10_000_000) {
                iterations *= 10;
                let start = Instant::now();
                delay_iterations(iterations);
                time_per_run = start.elapsed();
            }
            // This assumes time_per_run stays below one second.
            // Unless something weird happens, we shoud be safely within this margin...
            iterations_per_ms = iterations * 1_000_000 / time_per_run.subsec_nanos() as u64;
        }

        Arc::new(move |ms: u64| delay_iterations(ms * iterations_per_ms))
    };

    let (tx, rx) = platform::channel().unwrap();
    assert!(rx.try_recv().is_err());

    let threads: Vec<_> = (0..num_senders).map(|i| {
        let tx = tx.clone();
        let delay = delay.clone();
        thread::spawn(move || {
            let data: Vec<u8> = (0..msg_size).map(|j| (j % 13) as u8 | i << 4).collect();
            let data: &[u8] = &data[..];
            delay(thread_delay);
            tx.send(data, vec![], vec![]).unwrap();
        })
    }).collect();

    let mut received_vals: Vec<u8> = vec![];
    for _ in 0..num_senders {
        let mut result;
        while {
            result = rx.try_recv();
            result.is_err()
        } {}
        let (mut received_data, received_channels, received_shared_memory) = result.unwrap();
        received_data.truncate(msg_size);

        let val = received_data[0] >> 4;
        received_vals.push(val);
        let data: Vec<u8> = (0..msg_size).map(|j| (j % 13) as u8 | val << 4).collect();
        let data: &[u8] = &data[..];
        assert_eq!(received_data.len(), data.len());
        assert_eq!((&received_data[..], received_channels, received_shared_memory),
                   (data, vec![], vec![]));
    }
    assert!(rx.try_recv().is_err()); // There should be no further messages pending.
    received_vals.sort();
    assert_eq!(received_vals, (0..num_senders).collect::<Vec<_>>()); // Got exactly the values we sent.

    for thread in threads {
        thread.join().unwrap();
    }
}

#[cfg(feature = "unstable")]
mod sync_test {
    use platform;

    trait SyncTest {
        fn test_not_sync();
    }

    impl<T> SyncTest for T {
        default fn test_not_sync() {}
    }

    impl<T: Sync> SyncTest for T {
        fn test_not_sync() {
            panic!("`OsIpcSender` should not be `Sync`");
        }
    }

    #[test]
    fn receiver_not_sync() {
        platform::OsIpcSender::test_not_sync();
    }
}
