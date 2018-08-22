use {TryRead, TryWrite};
use std::mem;
use mio::*;
use std::io;
use mio::deprecated::{EventLoop, Handler};
use mio::deprecated::unix::{PipeReader, PipeWriter};
use std::process::{Command, Stdio, Child};


struct SubprocessClient {
    stdin: Option<PipeWriter>,
    stdout: Option<PipeReader>,
    stderr: Option<PipeReader>,
    stdin_token : Token,
    stdout_token : Token,
    stderr_token : Token,
    output : Vec<u8>,
    output_stderr : Vec<u8>,
    input : Vec<u8>,
    input_offset : usize,
    buf : [u8; 65536],
}


// Sends a message and expects to receive the same exact message, one at a time
impl SubprocessClient {
    fn new(stdin: Option<PipeWriter>, stdout : Option<PipeReader>, stderr : Option<PipeReader>, data : &[u8]) -> SubprocessClient {
        SubprocessClient {
            stdin: stdin,
            stdout: stdout,
            stderr: stderr,
            stdin_token : Token(0),
            stdout_token : Token(1),
            stderr_token : Token(2),
            output : Vec::<u8>::new(),
            output_stderr : Vec::<u8>::new(),
            buf : [0; 65536],
            input : data.to_vec(),
            input_offset : 0,
        }
    }

    fn readable(&mut self, event_loop: &mut EventLoop<SubprocessClient>) -> io::Result<()> {
        let mut eof = false;
        match self.stdout {
            None => unreachable!(),
            Some (ref mut stdout) => match stdout.try_read(&mut self.buf[..]) {
                Ok(None) => {
                }
                Ok(Some(r)) => {
                    if r == 0 {
                        eof = true;
                    } else {
                         self.output.extend(&self.buf[0..r]);
                    }
                }
                Err(e) => {
                    return Err(e);
                }
            }
        };
        if eof {
            drop(self.stdout.take());
            match self.stderr {
                None => event_loop.shutdown(),
                Some(_) => {},
            }
        }
        return Ok(());
    }

    fn readable_stderr(&mut self, event_loop: &mut EventLoop<SubprocessClient>) -> io::Result<()> {
        let mut eof = false;
        match self.stderr {
            None => unreachable!(),
            Some(ref mut stderr) => match stderr.try_read(&mut self.buf[..]) {
                Ok(None) => {
                }
                Ok(Some(r)) => {
                    if r == 0 {
                        eof = true;
                    } else {
                        self.output_stderr.extend(&self.buf[0..r]);
                    }
                }
                Err(e) => {
                    return Err(e);
                }
            }
        };
        if eof {
                        drop(self.stderr.take());
                        match self.stdout {
                            None => event_loop.shutdown(),
                            Some(_) => {},
                        }
        }
        return Ok(());
    }

    fn writable(&mut self, event_loop: &mut EventLoop<SubprocessClient>) -> io::Result<()> {
        let mut ok = true;
        match self.stdin {
            None => unreachable!(),
            Some(ref mut stdin) => match stdin.try_write(&(&self.input)[self.input_offset..]) {
                Ok(None) => {
                },
                Ok(Some(r)) => {
                    if r == 0 {
                        ok = false;
                    } else {
                        self.input_offset += r;
                    }
                },
                Err(_) => {
                    ok = false;
                },
            }
        }
        if self.input_offset == self.input.len() || !ok {
            drop(self.stdin.take());
            match self.stderr {
                None => match self.stdout {
                            None => event_loop.shutdown(),
                            Some(_) => {},
                },
                Some(_) => {},
            }
        }
        return Ok(());
    }

}

impl Handler for SubprocessClient {
    type Timeout = usize;
    type Message = ();

    fn ready(&mut self, event_loop: &mut EventLoop<SubprocessClient>, token: Token,
             _: Ready) {
        if token == self.stderr_token {
            let _x = self.readable_stderr(event_loop);
        } else {
            let _x = self.readable(event_loop);
        }
        if token == self.stdin_token {
            let _y = self.writable(event_loop);
        }
    }
}




const TEST_DATA : [u8; 1024 * 4096] = [42; 1024 * 4096];
pub fn subprocess_communicate(mut process : Child, input : &[u8]) -> (Vec<u8>, Vec<u8>) {
    let mut event_loop = EventLoop::<SubprocessClient>::new().unwrap();
    let stdin : Option<PipeWriter>;
    let stdin_exists : bool;
    match process.stdin {
      None => stdin_exists = false,
      Some(_) => stdin_exists = true,
    }
    if stdin_exists {
        match PipeWriter::from_stdin(process.stdin.take().unwrap()) {
            Err(e) => panic!(e),
            Ok(pipe) => stdin = Some(pipe),
        }
    } else {
        stdin = None;
    }
    let stdout_exists : bool;
    let stdout : Option<PipeReader>;
    match process.stdout {
      None => stdout_exists = false,
      Some(_) => stdout_exists = true,
    }
    if stdout_exists {
        match PipeReader::from_stdout(process.stdout.take().unwrap()) {
            Err(e) => panic!(e),
            Ok(pipe) => stdout = Some(pipe),
        }
    } else {
        stdout = None;
    }
    let stderr_exists : bool;
    let stderr : Option<PipeReader>;
    match process.stderr {
      None => stderr_exists = false,
      Some(_) => stderr_exists = true,
    }
    if stderr_exists {
        match PipeReader::from_stderr(process.stderr.take().unwrap()) {
              Err(e) => panic!(e),
              Ok(pipe) => stderr = Some(pipe),
        }
    } else {
        stderr = None
    }

    let mut subprocess = SubprocessClient::new(stdin,
                                               stdout,
                                               stderr,
                                               input);
    match subprocess.stdout {
       Some(ref sub_stdout) => event_loop.register(sub_stdout, subprocess.stdout_token, Ready::readable(),
                                                   PollOpt::level()).unwrap(),
       None => {},
    }

    match subprocess.stderr {
        Some(ref sub_stderr) => event_loop.register(sub_stderr, subprocess.stderr_token, Ready::readable(),
                        PollOpt::level()).unwrap(),
        None => {},
    }

    // Connect to the server
    match subprocess.stdin {
        Some (ref sub_stdin) => event_loop.register(sub_stdin, subprocess.stdin_token, Ready::writable(),
                        PollOpt::level()).unwrap(),
         None => {},
    }

    // Start the event loop
    event_loop.run(&mut subprocess).unwrap();
    let _ = process.wait();

    let ret_stdout = mem::replace(&mut subprocess.output, Vec::<u8>::new());
    let ret_stderr = mem::replace(&mut subprocess.output_stderr, Vec::<u8>::new());
    return (ret_stdout, ret_stderr);
}

#[test]
fn test_subprocess_pipe() {
    let process =
           Command::new("/bin/cat")
           .stdin(Stdio::piped())
           .stdout(Stdio::piped())
           .stderr(Stdio::piped())
           .spawn().unwrap();
     let (ret_stdout, ret_stderr) = subprocess_communicate(process, &TEST_DATA[..]);
     assert_eq!(TEST_DATA.len(), ret_stdout.len());
     assert_eq!(0usize, ret_stderr.len());
     let mut i : usize = 0;
     for item in TEST_DATA.iter() {
         assert_eq!(*item, ret_stdout[i]);
         i += 1;
     }
}
