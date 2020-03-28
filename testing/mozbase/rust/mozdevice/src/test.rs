use crate::*;

// Currently the API is not safe for multiple requests at the same time. It is
// recommended to run each of the unit tests on its own. Use the following
// command to accomplish that:
//
//     $ cargo test -- --test-threads=1

use std::collections::BTreeSet;
use std::panic;
use tempfile::{tempdir, TempDir};

#[test]
fn read_length_from_valid_string() {
    fn test(message: &str) -> Result<usize> {
        read_length(&mut io::BufReader::new(message.as_bytes()))
    }

    assert_eq!(test("0000").unwrap(), 0);
    assert_eq!(test("0001").unwrap(), 1);
    assert_eq!(test("000F").unwrap(), 15);
    assert_eq!(test("00FF").unwrap(), 255);
    assert_eq!(test("0FFF").unwrap(), 4095);
    assert_eq!(test("FFFF").unwrap(), 65535);

    assert_eq!(test("FFFF0").unwrap(), 65535);
}

#[test]
fn read_length_from_invalid_string() {
    fn test(message: &str) -> Result<usize> {
        read_length(&mut io::BufReader::new(message.as_bytes()))
    }

    test("").expect_err("empty string");
    test("G").expect_err("invalid hex character");
    test("-1").expect_err("negative number");
    test("000").expect_err("shorter than 4 bytes");
}

#[test]
fn encode_message_with_valid_string() {
    assert_eq!(encode_message("").unwrap(), "0000".to_string());
    assert_eq!(encode_message("a").unwrap(), "0001a".to_string());
    assert_eq!(
        encode_message(&"a".repeat(15)).unwrap(),
        format!("000F{}", "a".repeat(15))
    );
    assert_eq!(
        encode_message(&"a".repeat(255)).unwrap(),
        format!("00FF{}", "a".repeat(255))
    );
    assert_eq!(
        encode_message(&"a".repeat(4095)).unwrap(),
        format!("0FFF{}", "a".repeat(4095))
    );
    assert_eq!(
        encode_message(&"a".repeat(65535)).unwrap(),
        format!("FFFF{}", "a".repeat(65535))
    );
}

#[test]
fn encode_message_with_invalid_string() {
    encode_message(&"a".repeat(65536)).expect_err("string lengths exceeds 4 bytes");
}

fn run_device_test<F>(test: F) -> ()
where
    F: FnOnce(&Device, &TempDir, &Path) -> () + panic::UnwindSafe,
{
    fn clean_remote_dir(device: &Device, path: &Path) {
        let command = format!("rm -r {}", path.display());
        let _ = device.execute_host_shell_command(&command);
    }

    let host = Host {
        ..Default::default()
    };
    let device = host
        .device_or_default::<String>(None)
        .expect("device_or_default");

    let tmp_dir = tempdir().expect("create temp dir");
    let remote_path = Path::new("/mnt/sdcard/mozdevice/");

    clean_remote_dir(&device, remote_path);

    let result = panic::catch_unwind(|| test(&device, &tmp_dir, &remote_path));

    let _ = device.kill_forward_all_ports();
    // let _ = device.kill_reverse_all_ports();

    assert!(result.is_ok())
}

#[test]
fn host_features() {
    let host = Host {
        ..Default::default()
    };

    let set = host.features::<BTreeSet<_>>().expect("to query features");
    assert!(set.contains("cmd"));
    assert!(set.contains("shell_v2"));
}

#[test]
fn host_devices() {
    let host = Host {
        ..Default::default()
    };

    let set: BTreeSet<_> = host.devices().expect("to query devices");
    assert_eq!(1, set.len());
}

#[test]
fn host_device_or_default_valid_serial() {
    let host = Host {
        ..Default::default()
    };

    let devices: Vec<_> = host.devices().expect("to query devices");
    let expected_device = devices.first().expect("found a device");

    let device = host
        .device_or_default::<String>(Some(&expected_device.serial))
        .expect("connected device with serial");
    assert_eq!(device.serial, expected_device.serial);
}

#[test]
fn host_device_or_default_invalid_serial() {
    let host = Host {
        ..Default::default()
    };

    host.device_or_default::<String>(Some(&"foobar".to_owned()))
        .expect_err("invalid serial");
}

#[test]
fn host_device_or_default_no_serial() {
    let host = Host {
        ..Default::default()
    };

    let devices: Vec<_> = host.devices().expect("to query devices");
    let expected_device = devices.first().expect("found a device");

    let device = host
        .device_or_default::<String>(None)
        .expect("connected device with serial");
    assert_eq!(device.serial, expected_device.serial);
}

#[test]
fn device_shell_command() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        assert_eq!(
            "Linux\n",
            device
                .execute_host_shell_command("uname")
                .expect("to have shell output")
        );
    });
}

#[test]
fn device_forward_port_hardcoded() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        assert_eq!(
            3035,
            device
                .forward_port(3035, 3036)
                .expect("forwarded local port")
        );
        // TODO: check with forward --list
    });
}

// #[test]
// TODO: "adb server response to `forward tcp:0 ...` was not a u16: \"000559464\"")
fn device_forward_port_system_allocated() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        let local_port = device.forward_port(0, 3037).expect("local_port");
        assert_ne!(local_port, 0);
        // TODO: check with forward --list
    });
}

#[test]
fn device_kill_forward_port_no_forwarded_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        device
            .kill_forward_port(3038)
            .expect_err("adb error: listener 'tcp:3038' ");
    });
}

#[test]
fn device_kill_forward_port_twice() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        let local_port = device
            .forward_port(3039, 3040)
            .expect("forwarded local port");
        assert_eq!(local_port, 3039);
        // TODO: check with forward --list
        device
            .kill_forward_port(local_port)
            .expect("to remove forwarded port");
        device
            .kill_forward_port(local_port)
            .expect_err("adb error: listener 'tcp:3039' ");
    });
}

#[test]
fn device_kill_forward_all_ports_no_forwarded_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        device
            .kill_forward_all_ports()
            .expect("to not fail for no forwarded ports");
    });
}

#[test]
fn device_kill_forward_all_ports_twice() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        let local_port1 = device
            .forward_port(3039, 3040)
            .expect("forwarded local port");
        assert_eq!(local_port1, 3039);
        let local_port2 = device
            .forward_port(3041, 3042)
            .expect("forwarded local port");
        assert_eq!(local_port2, 3041);
        // TODO: check with forward --list
        device
            .kill_forward_all_ports()
            .expect("to remove all forwarded ports");
        device
            .kill_forward_all_ports()
            .expect("to not fail for no forwarded ports");
    });
}

#[test]
fn device_reverse_port_hardcoded() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        assert_eq!(4035, device.reverse_port(4035, 4036).expect("remote_port"));
        // TODO: check with reverse --list
    });
}

// #[test]
// TODO: No adb response: ParseInt(ParseIntError { kind: Empty })
fn device_reverse_port_system_allocated() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        let reverse_port = device.reverse_port(0, 4037).expect("remote port");
        assert_ne!(reverse_port, 0);
        // TODO: check with reverse --list
    });
}

#[test]
fn device_kill_reverse_port_no_reverse_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        device
            .kill_reverse_port(4038)
            .expect_err("listener 'tcp:4038' not found");
    });
}

// #[test]
// TODO: "adb error: adb server response did not contain expected hexstring length: \"\""
fn device_kill_reverse_port_twice() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        let remote_port = device
            .reverse_port(4039, 4040)
            .expect("reversed local port");
        assert_eq!(remote_port, 4039);
        // TODO: check with reverse --list
        device
            .kill_reverse_port(remote_port)
            .expect("to remove reverse port");
        device
            .kill_reverse_port(remote_port)
            .expect_err("listener 'tcp:4039' not found");
    });
}

#[test]
fn device_kill_reverse_all_ports_no_reversed_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        device
            .kill_reverse_all_ports()
            .expect("to not fail for no reversed ports");
    });
}

#[test]
fn device_kill_reverse_all_ports_twice() {
    run_device_test(|device: &Device, _: &TempDir, _: &Path| {
        let local_port1 = device
            .forward_port(4039, 4040)
            .expect("forwarded local port");
        assert_eq!(local_port1, 4039);
        let local_port2 = device
            .forward_port(4041, 4042)
            .expect("forwarded local port");
        assert_eq!(local_port2, 4041);
        // TODO: check with reverse --list
        device
            .kill_reverse_all_ports()
            .expect("to remove all reversed ports");
        device
            .kill_reverse_all_ports()
            .expect("to not fail for no reversed ports");
    });
}

#[test]
fn device_push() {
    run_device_test(|device: &Device, _: &TempDir, remote_root_path: &Path| {
        let content = "test";
        let remote_path = remote_root_path.join("foo.bar");

        device
            .push(
                &mut io::BufReader::new(content.as_bytes()),
                &remote_path,
                0o777,
            )
            .expect("file has been pushed");

        let output = device
            .execute_host_shell_command(&format!("ls {}", remote_path.display()))
            .expect("host shell command for 'ls' to succeed");

        assert!(output.contains(remote_path.to_str().unwrap()));

        let file_content = device
            .execute_host_shell_command(&format!("cat {}", remote_path.display()))
            .expect("host shell command for 'cat' to succeed");

        assert_eq!(file_content, content);
    });
}

#[test]
fn device_push_dir() {
    run_device_test(
        |device: &Device, tmp_dir: &TempDir, remote_root_path: &Path| {
            let content = "test";

            let files = [
                Path::new("foo1.bar"),
                Path::new("foo2.bar"),
                Path::new("bar/foo3.bar"),
                Path::new("bar/more/foo3.bar"),
            ];

            for file in files.iter() {
                let path = tmp_dir.path().join(file);
                let _ = std::fs::create_dir_all(path.parent().unwrap());

                let f = File::create(path).expect("to create file");
                let mut f = io::BufWriter::new(f);
                f.write_all(content.as_bytes()).expect("to write data");
            }

            device
                .push_dir(tmp_dir.path(), &remote_root_path, 0o777)
                .expect("to push_dir");

            for file in files.iter() {
                let path = remote_root_path.join(file);
                let output = device
                    .execute_host_shell_command(&format!("ls {}", path.display()))
                    .expect("host shell command for 'ls' to succeed");

                assert!(output.contains(path.to_str().unwrap()));
            }
        },
    );
}
