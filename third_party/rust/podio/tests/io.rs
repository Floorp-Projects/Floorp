extern crate podio;

use std::io::{self, Read};
use podio::ReadPodExt;

struct TestReader {
    state: Option<io::Result<usize>>,
}

impl TestReader {
    fn new(first_return: io::Result<usize>) -> TestReader {
        TestReader { state: Some(first_return) }
    }

    fn get(&mut self) -> io::Result<u32> {
        self.read_u32::<podio::LittleEndian>()
    }

    fn test(first_return: io::Result<usize>) -> io::Result<u32> {
        TestReader::new(first_return).get()
    }
}

impl Read for TestReader {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        match self.state.take().unwrap_or(Ok(1)) {
            Ok(n) => {
                for i in buf.iter_mut().take(n) {
                    *i = 0xA5;
                }
                Ok(n)
            },
            e @ Err(..) => e,
        }
    }
}

#[test]
fn interrupted() {
    // Getting an io::ErrorKind::Interrupted should be retried, and thus succeed
    let first_read = Err(io::Error::new(io::ErrorKind::Interrupted, "Interrupted"));
    assert_eq!(TestReader::test(first_read).unwrap(), 0xA5A5A5A5);
}

#[test]
fn eof() {
    // Getting a Ok(0) implies an unexpected EOF
    let first_read = Ok(0);
    assert!(TestReader::test(first_read).is_err());
}

#[test]
fn err() {
    // Getting any other error is still an error
    let first_read = Err(io::Error::new(io::ErrorKind::Other, "Other"));
    assert!(TestReader::test(first_read).is_err());
}
