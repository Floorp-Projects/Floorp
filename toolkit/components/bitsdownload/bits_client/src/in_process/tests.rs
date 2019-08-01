/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// These are full integration tests that use the BITS service.

// TODO
// It may make sense to restrict how many tests can run at once. BITS is only supposed to support
// four simultaneous notifications per user, it is not impossible that this test suite could
// exceed that.

#![cfg(test)]
extern crate bits;
extern crate lazy_static;
extern crate rand;
extern crate regex;
extern crate tempfile;

use std::ffi::{OsStr, OsString};
use std::fs::{self, File};
use std::io::{Read, Write};
use std::net::{SocketAddr, TcpListener, TcpStream};
use std::panic;
use std::sync::{Arc, Condvar, Mutex};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

use self::{
    bits::BackgroundCopyManager,
    lazy_static::lazy_static,
    rand::{thread_rng, Rng},
    regex::bytes::Regex,
    tempfile::{Builder, TempDir},
};
use super::{
    super::{BitsJobState, Error},
    BitsProxyUsage, InProcessClient, StartJobSuccess,
};

static SERVER_ADDRESS: [u8; 4] = [127, 0, 0, 1];

lazy_static! {
    static ref TEST_MUTEX: Mutex<()> = Mutex::new(());
}

fn format_job_name(name: &str) -> OsString {
    format!("InProcessClient Test {}", name).into()
}

fn format_dir_prefix(tmp_dir: &TempDir) -> OsString {
    let mut dir = tmp_dir.path().to_path_buf().into_os_string();
    dir.push("\\");
    dir
}

fn cancel_jobs(name: &OsStr) {
    BackgroundCopyManager::connect()
        .unwrap()
        .cancel_jobs_by_name(name)
        .unwrap();
}

struct HttpServerResponses {
    body: Box<[u8]>,
    delay: u64,
    //error: Box<[u8]>,
}

struct MockHttpServerHandle {
    port: u16,
    join: Option<JoinHandle<Result<(), ()>>>,
    shutdown: Arc<(Mutex<bool>, Condvar)>,
}

impl MockHttpServerHandle {
    fn shutdown(&mut self) {
        if self.join.is_none() {
            return;
        }

        {
            let &(ref lock, ref cvar) = &*self.shutdown;
            let mut shutdown = lock.lock().unwrap();

            if !*shutdown {
                *shutdown = true;
                cvar.notify_all();
            }
        }
        // Wake up the server from `accept()`. Will fail if the server wasn't listening.
        let _ = TcpStream::connect_timeout(
            &(SERVER_ADDRESS, self.port).into(),
            Duration::from_millis(10_000),
        );

        match self.join.take().unwrap().join() {
            Ok(_) => {}
            Err(p) => panic::resume_unwind(p),
        }
    }

    fn format_url(&self, name: &str) -> OsString {
        format!(
            "http://{}/{}",
            SocketAddr::from((SERVER_ADDRESS, self.port)),
            name
        )
        .into()
    }
}

fn mock_http_server(name: &'static str, responses: HttpServerResponses) -> MockHttpServerHandle {
    let mut bind_retries = 10;
    let shutdown = Arc::new((Mutex::new(false), Condvar::new()));
    let caller_shutdown = shutdown.clone();

    let (listener, port) = loop {
        let port = thread_rng().gen_range(1024, 0x1_0000u32) as u16;
        match TcpListener::bind(SocketAddr::from((SERVER_ADDRESS, port))) {
            Ok(listener) => {
                break (listener, port);
            }
            r @ Err(_) => {
                if bind_retries == 0 {
                    r.unwrap();
                }
                bind_retries -= 1;
                continue;
            }
        }
    };

    let join = thread::Builder::new()
        .name(format!("mock_http_server {}", name))
        .spawn(move || {
            // returns Err(()) if server should shut down immediately
            fn check_shutdown(shutdown: &Arc<(Mutex<bool>, Condvar)>) -> Result<(), ()> {
                if *shutdown.0.lock().unwrap() {
                    Err(())
                } else {
                    Ok(())
                }
            }
            fn sleep(shutdown: &Arc<(Mutex<bool>, Condvar)>, delay_millis: u64) -> Result<(), ()> {
                let sleep_start = Instant::now();
                let sleep_end = sleep_start + Duration::from_millis(delay_millis);

                let (ref lock, ref cvar) = **shutdown;
                let mut shutdown_requested = lock.lock().unwrap();
                loop {
                    if *shutdown_requested {
                        return Err(());
                    }

                    let before_wait = Instant::now();
                    if before_wait >= sleep_end {
                        return Ok(());
                    }
                    let wait_dur = sleep_end - before_wait;
                    shutdown_requested = cvar.wait_timeout(shutdown_requested, wait_dur).unwrap().0;
                }
            }

            let error_404 = Regex::new(r"^((GET)|(HEAD)) [[:print:]]*/error_404 ").unwrap();
            let error_500 = Regex::new(r"^((GET)|(HEAD)) [[:print:]]*/error_500 ").unwrap();

            loop {
                let (mut socket, _addr) = listener.accept().expect("accept should succeed");

                socket
                    .set_read_timeout(Some(Duration::from_millis(10_000)))
                    .unwrap();
                let mut s = Vec::new();
                for b in Read::by_ref(&mut socket).bytes() {
                    if b.is_err() {
                        eprintln!("read error {:?}", b);
                        break;
                    }
                    let b = b.unwrap();
                    s.push(b);
                    if s.ends_with(b"\r\n\r\n") {
                        break;
                    }
                    check_shutdown(&shutdown)?;
                }

                // request received

                check_shutdown(&shutdown)?;

                // Special error URIs
                if error_404.is_match(&s) {
                    sleep(&shutdown, responses.delay)?;
                    let result = socket
                        .write(b"HTTP/1.1 404 Not Found\r\n\r\n")
                        .and_then(|_| socket.flush());
                    if let Err(e) = result {
                        eprintln!("error writing 404 header {:?}", e);
                    }
                    continue;
                }

                if error_500.is_match(&s) {
                    sleep(&shutdown, responses.delay)?;
                    let result = socket
                        .write(b"HTTP/1.1 500 Internal Server Error\r\n\r\n")
                        .and_then(|_| socket.flush());
                    if let Err(e) = result {
                        eprintln!("error writing 500 header {:?}", e);
                    }
                    continue;
                }

                // Response with a body.
                if s.starts_with(b"HEAD") || s.starts_with(b"GET") {
                    let result = socket
                        .write(
                            format!(
                                "HTTP/1.1 200 OK\r\nContent-Length: {}\r\n\r\n",
                                responses.body.len()
                            )
                            .as_bytes(),
                        )
                        .and_then(|_| socket.flush());
                    if let Err(e) = result {
                        eprintln!("error writing header {:?}", e);
                        continue;
                    }
                }

                if s.starts_with(b"GET") {
                    sleep(&shutdown, responses.delay)?;
                    let result = socket.write(&responses.body).and_then(|_| socket.flush());
                    if let Err(e) = result {
                        eprintln!("error writing content {:?}", e);
                        continue;
                    }
                }
            }
        });

    MockHttpServerHandle {
        port,
        join: Some(join.unwrap()),
        shutdown: caller_shutdown,
    }
}

// Test wrapper to ensure jobs are canceled, set up name strings
macro_rules! test {
    (fn $name:ident($param:ident : &str, $tmpdir:ident : &TempDir) $body:block) => {
        #[test]
        fn $name() {
            let $param = stringify!($name);
            let $tmpdir = &Builder::new().prefix($param).tempdir().unwrap();

            let result = panic::catch_unwind(|| $body);

            cancel_jobs(&format_job_name($param));

            if let Err(e) = result {
                panic::resume_unwind(e);
            }
        }
    };
}

test! {
    fn start_monitor_and_cancel(name: &str, tmp_dir: &TempDir) {
        let mut server = mock_http_server(name, HttpServerResponses {
            body: name.to_owned().into_boxed_str().into_boxed_bytes(),
            delay: 10_000,
        });

        let mut client = InProcessClient::new(format_job_name(name), tmp_dir.path().into()).unwrap();

        let no_progress_timeout_secs = 60;
        let interval = 10_000;
        let timeout = 10_000;

        let (StartJobSuccess {guid}, mut monitor) =
            client.start_job(
                server.format_url(name),
                name.into(),
                BitsProxyUsage::Preconfig,
                no_progress_timeout_secs,
                interval,
                ).unwrap();

        // cancel in ~250ms
        let _join = thread::Builder::new()
            .spawn(move || {
                thread::sleep(Duration::from_millis(250));
                client.cancel_job(guid).unwrap();
            });

        let start = Instant::now();

        // First immediate report
        monitor.get_status(timeout).expect("should initially be ok").unwrap();

        // ~250ms the cancel should cause an immediate disconnect (otherwise we wouldn't get
        // an update until 10s when the transfer completes or the interval expires)
        match monitor.get_status(timeout) {
            Err(Error::NotConnected) => {},
            Ok(r) => panic!("unexpected success from get_status() {:?}", r),
            Err(e) => panic!("unexpected failure from get_status() {:?}", e),
        }
        assert!(start.elapsed() < Duration::from_millis(9_000));

        server.shutdown();
    }
}

test! {
    fn start_monitor_and_complete(name: &str, tmp_dir: &TempDir) {
        let file_path = tmp_dir.path().join(name);

        let mut server = mock_http_server(name, HttpServerResponses {
            body: name.to_owned().into_boxed_str().into_boxed_bytes(),
            delay: 500,
        });

        let mut client = InProcessClient::new(format_job_name(name), format_dir_prefix(tmp_dir)).unwrap();

        let no_progress_timeout_secs = 60;
        let interval = 100;
        let timeout = 10_000;

        let (StartJobSuccess {guid}, mut monitor) =
            client.start_job(
                server.format_url(name),
                name.into(),
                BitsProxyUsage::Preconfig,
                no_progress_timeout_secs,
                interval,
                ).unwrap();

        let start = Instant::now();

        // Get status reports until transfer finishes (~500ms)
        let mut completed = false;
        loop {
            match monitor.get_status(timeout) {
                Err(e) => {
                    if completed {
                        break;
                    } else {
                        panic!("monitor failed before completion {:?}", e);
                    }
                }
                Ok(Ok(status)) => match BitsJobState::from(status.state) {
                    BitsJobState::Queued | BitsJobState::Connecting
                        | BitsJobState::Transferring => {
                            //eprintln!("{:?}", BitsJobState::from(status.state));
                            //eprintln!("{:?}", status);

                            // As long as there is no error, setting the timeout to 0 will not
                            // fail an active transfer.
                            client.set_no_progress_timeout(guid.clone(), 0).unwrap();
                        }
                    BitsJobState::Transferred => {
                        client.complete_job(guid.clone()).unwrap();
                        completed = true;
                    }
                    _ => {
                        panic!(format!("{:?}", status));
                    }
                }
                Ok(Err(e)) => panic!(format!("{:?}", e)),
            }

            // Timeout to prevent waiting forever
            assert!(start.elapsed() < Duration::from_millis(60_000));
        }


        // Verify file contents
        let result = panic::catch_unwind(|| {
            let mut file = File::open(file_path.clone()).unwrap();
            let mut v = Vec::new();
            file.read_to_end(&mut v).unwrap();
            assert_eq!(v, name.as_bytes());
        });

        let _ = fs::remove_file(file_path);

        if let Err(e) = result {
            panic::resume_unwind(e);
        }

        // Save this for last to ensure the file is removed.
        server.shutdown();
    }
}

test! {
    fn async_transferred_notification(name: &str, tmp_dir: &TempDir) {
        let mut server = mock_http_server(name, HttpServerResponses {
            body: name.to_owned().into_boxed_str().into_boxed_bytes(),
            delay: 250,
        });

        let mut client = InProcessClient::new(format_job_name(name), format_dir_prefix(tmp_dir)).unwrap();

        let no_progress_timeout_secs = 60;
        let interval = 60_000;
        let timeout = 10_000;

        let (_, mut monitor) =
            client.start_job(
                server.format_url(name),
                name.into(),
                BitsProxyUsage::Preconfig,
                no_progress_timeout_secs,
                interval,
                ).unwrap();

        // Start the timer now, the initial job creation may be delayed by BITS service startup.
        let start = Instant::now();

        // First report, immediate
        let report_1 = monitor.get_status(timeout).expect("should initially be ok").unwrap();
        let elapsed_to_report_1 = start.elapsed();

        // Transferred notification should come when the job completes in ~250 ms, otherwise we
        // will be stuck until timeout.
        let report_2 = monitor.get_status(timeout).expect("should get status update").unwrap();
        let elapsed_to_report_2 = start.elapsed();
        assert!(elapsed_to_report_2 < Duration::from_millis(9_000));
        assert_eq!(report_2.state, BitsJobState::Transferred);

        let short_timeout = 500;
        let report_3 = monitor.get_status(short_timeout);
        let elapsed_to_report_3 = start.elapsed();

        if let Ok(report_3) = report_3 {
          panic!("should be disconnected\n\
                  report_1 ({}.{:03}): {:?}\n\
                  report_2 ({}.{:03}): {:?}\n\
                  report_3 ({}.{:03}): {:?}",
                  elapsed_to_report_1.as_secs(), elapsed_to_report_1.subsec_millis(), report_1,
                  elapsed_to_report_2.as_secs(), elapsed_to_report_2.subsec_millis(), report_2,
                  elapsed_to_report_3.as_secs(), elapsed_to_report_3.subsec_millis(), report_3,
                  );
        }

        server.shutdown();

        // job will be cancelled by macro
    }
}

test! {
    fn change_interval(name: &str, tmp_dir: &TempDir) {
        let mut server = mock_http_server(name, HttpServerResponses {
            body: name.to_owned().into_boxed_str().into_boxed_bytes(),
            delay: 1000,
        });

        let mut client = InProcessClient::new(format_job_name(name), format_dir_prefix(tmp_dir)).unwrap();

        let no_progress_timeout_secs = 0;
        let interval = 60_000;
        let timeout = 10_000;

        let (StartJobSuccess { guid }, mut monitor) =
            client.start_job(
                server.format_url(name),
                name.into(),
                BitsProxyUsage::Preconfig,
                no_progress_timeout_secs,
                interval,
                ).unwrap();

        let start = Instant::now();

        // reduce monitor interval in ~250ms to 500ms
        let _join = thread::Builder::new()
            .spawn(move || {
                thread::sleep(Duration::from_millis(250));
                client.set_update_interval(guid, 500).unwrap();
            });

        // First immediate report
        monitor.get_status(timeout).expect("should initially be ok").unwrap();

        // Next report should be rescheduled to 500ms by the spawned thread, otherwise no status
        // until the original 10s interval.
        monitor.get_status(timeout).expect("expected second status").unwrap();
        assert!(start.elapsed() < Duration::from_millis(9_000));
        assert!(start.elapsed() > Duration::from_millis(400));

        server.shutdown();

        // job will be cancelled by macro
    }
}

test! {
    fn async_error_notification(name: &str, tmp_dir: &TempDir) {
        let mut server = mock_http_server(name, HttpServerResponses {
            body: name.to_owned().into_boxed_str().into_boxed_bytes(),
            delay: 100,
        });

        let mut client = InProcessClient::new(format_job_name(name), format_dir_prefix(tmp_dir)).unwrap();

        let no_progress_timeout_secs = 60;
        let interval = 60_000;
        let timeout = 10_000;

        let (_, mut monitor) =
            client.start_job(
                server.format_url("error_404"),
                name.into(),
                BitsProxyUsage::Preconfig,
                no_progress_timeout_secs,
                interval,
                ).unwrap();

        // Start the timer now, the initial job creation may be delayed by BITS service startup.
        let start = Instant::now();

        // First immediate report
        monitor.get_status(timeout).expect("should initially be ok").unwrap();

        // Error notification should come with HEAD response in 100ms, otherwise no status until
        // 10s interval or timeout.
        let status = monitor.get_status(timeout).expect("should get status update").unwrap();
        assert!(start.elapsed() < Duration::from_millis(9_000));
        assert_eq!(status.state, BitsJobState::Error);

        server.shutdown();

        // job will be cancelled by macro
    }
}

test! {
    fn transient_error(name: &str, tmp_dir: &TempDir) {
        let mut server = mock_http_server(name, HttpServerResponses {
            body: name.to_owned().into_boxed_str().into_boxed_bytes(),
            delay: 100,
        });

        let mut client = InProcessClient::new(format_job_name(name), format_dir_prefix(tmp_dir)).unwrap();

        let no_progress_timeout_secs = 60;
        let interval = 1_000;
        let timeout = 10_000;

        let (StartJobSuccess { guid }, mut monitor) =
            client.start_job(
                server.format_url("error_500"),
                name.into(),
                BitsProxyUsage::Preconfig,
                no_progress_timeout_secs,
                interval,
                ).unwrap();

        // Start the timer now, the initial job creation may be delayed by BITS service startup.
        let start = Instant::now();

        // First immediate report
        monitor.get_status(timeout).expect("should initially be ok").unwrap();

        // Transient error notification should come when the interval expires in ~1s.
        let status = monitor.get_status(timeout).expect("should get status update").unwrap();
        assert!(start.elapsed() > Duration::from_millis(900));
        assert!(start.elapsed() < Duration::from_millis(9_000));
        assert_eq!(status.state, BitsJobState::TransientError);

        // Lower no progress timeout to 0
        let set_timeout_at = Instant::now();
        client.set_no_progress_timeout(guid, 0).unwrap();

        // Should convert the transient error to a permanent error immediately.
        let status = monitor.get_status(timeout).expect("should get status update").unwrap();
        assert!(set_timeout_at.elapsed() < Duration::from_millis(500));
        assert_eq!(status.state, BitsJobState::Error);

        server.shutdown();

        // job will be cancelled by macro
    }
}

test! {
    fn transient_to_permanent_error(name: &str, tmp_dir: &TempDir) {
        let mut server = mock_http_server(name, HttpServerResponses {
            body: name.to_owned().into_boxed_str().into_boxed_bytes(),
            delay: 100,
        });

        let mut client = InProcessClient::new(format_job_name(name), format_dir_prefix(tmp_dir)).unwrap();

        let no_progress_timeout_secs = 0;
        let interval = 1_000;
        let timeout = 10_000;

        let (_, mut monitor) =
            client.start_job(
                server.format_url("error_500"),
                name.into(),
                BitsProxyUsage::Preconfig,
                no_progress_timeout_secs,
                interval,
                ).unwrap();

        // Start the timer now, the initial job creation may be delayed by BITS service startup.
        let start = Instant::now();

        // First immediate report
        monitor.get_status(timeout).expect("should initially be ok").unwrap();

        // 500 is a transient error, but with no_progress_timeout_secs = 0 it should immediately
        // produce an error notification with the HEAD response in 100ms. Otherwise no status
        // until 10s interval or timeout.
        let status = monitor.get_status(timeout).expect("should get status update").unwrap();
        assert!(start.elapsed() < Duration::from_millis(500));
        assert_eq!(status.state, BitsJobState::Error);

        server.shutdown();

        // job will be cancelled by macro
    }
}
