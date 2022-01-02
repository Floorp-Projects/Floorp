/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::*;

// Currently the mozdevice API is not safe for multiple requests at the same
// time. It is recommended to run each of the unit tests on its own. Also adb
// specific tests cannot be run in CI yet. To check those locally, also run
// the ignored tests.
//
// Use the following command to accomplish that:
//
//     $ cargo test -- --ignored --test-threads=1

use crate::{append_components, AndroidStorage, AndroidStorageInput};
use std::collections::BTreeSet;
use std::panic;
use std::path::PathBuf;
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

fn run_device_test<F>(test: F)
where
    F: FnOnce(&Device, &TempDir, &UnixPath) + panic::UnwindSafe,
{
    let host = Host {
        ..Default::default()
    };
    let device = host
        .device_or_default::<String>(None, AndroidStorageInput::Auto)
        .expect("device_or_default");

    let tmp_dir = tempdir().expect("create temp dir");
    let remote_path = UnixPath::new("/data/local/tmp/mozdevice/");

    let _ = device.remove(remote_path);

    let result = panic::catch_unwind(|| test(&device, &tmp_dir, &remote_path));

    let _ = device.kill_forward_all_ports();
    // let _ = device.kill_reverse_all_ports();

    assert!(result.is_ok())
}

#[test]
#[ignore]
fn host_features() {
    let host = Host {
        ..Default::default()
    };

    let set = host.features::<BTreeSet<_>>().expect("to query features");
    assert!(set.contains("cmd"));
    assert!(set.contains("shell_v2"));
}

#[test]
#[ignore]
fn host_devices() {
    let host = Host {
        ..Default::default()
    };

    let set: BTreeSet<_> = host.devices().expect("to query devices");
    assert_eq!(1, set.len());
}

#[test]
#[ignore]
fn host_device_or_default() {
    let host = Host {
        ..Default::default()
    };

    let devices: Vec<_> = host.devices().expect("to query devices");
    let expected_device = devices.first().expect("found a device");

    let device = host
        .device_or_default::<String>(Some(&expected_device.serial), AndroidStorageInput::App)
        .expect("connected device with serial");
    assert_eq!(device.run_as_package, None);
    assert_eq!(device.serial, expected_device.serial);
    assert!(device.tempfile.starts_with("/data/local/tmp"));
}

#[test]
#[ignore]
fn host_device_or_default_invalid_serial() {
    let host = Host {
        ..Default::default()
    };

    host.device_or_default::<String>(Some(&"foobar".to_owned()), AndroidStorageInput::Auto)
        .expect_err("invalid serial");
}

#[test]
#[ignore]
fn host_device_or_default_no_serial() {
    let host = Host {
        ..Default::default()
    };

    let devices: Vec<_> = host.devices().expect("to query devices");
    let expected_device = devices.first().expect("found a device");

    let device = host
        .device_or_default::<String>(None, AndroidStorageInput::Auto)
        .expect("connected device with serial");
    assert_eq!(device.serial, expected_device.serial);
}

#[test]
#[ignore]
fn host_device_or_default_storage_as_app() {
    let host = Host {
        ..Default::default()
    };

    let device = host
        .device_or_default::<String>(None, AndroidStorageInput::App)
        .expect("connected device");
    assert_eq!(device.storage, AndroidStorage::App);
}

#[test]
#[ignore]
fn host_device_or_default_storage_as_auto() {
    let host = Host {
        ..Default::default()
    };

    let device = host
        .device_or_default::<String>(None, AndroidStorageInput::Auto)
        .expect("connected device");
    assert_eq!(device.storage, AndroidStorage::Sdcard);
}

#[test]
#[ignore]
fn host_device_or_default_storage_as_internal() {
    let host = Host {
        ..Default::default()
    };

    let device = host
        .device_or_default::<String>(None, AndroidStorageInput::Internal)
        .expect("connected device");
    assert_eq!(device.storage, AndroidStorage::Internal);
}

#[test]
#[ignore]
fn host_device_or_default_storage_as_sdcard() {
    let host = Host {
        ..Default::default()
    };

    let device = host
        .device_or_default::<String>(None, AndroidStorageInput::Sdcard)
        .expect("connected device");
    assert_eq!(device.storage, AndroidStorage::Sdcard);
}

#[test]
#[ignore]
fn device_shell_command() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
        assert_eq!(
            "Linux\n",
            device
                .execute_host_shell_command("uname")
                .expect("to have shell output")
        );
    });
}

#[test]
#[ignore]
fn device_forward_port_hardcoded() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
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
// #[ignore]
// TODO: "adb server response to `forward tcp:0 ...` was not a u16: \"000559464\"")
// fn device_forward_port_system_allocated() {
//     run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
//         let local_port = device.forward_port(0, 3037).expect("local_port");
//         assert_ne!(local_port, 0);
//         // TODO: check with forward --list
//     });
// }

#[test]
#[ignore]
fn device_kill_forward_port_no_forwarded_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
        device
            .kill_forward_port(3038)
            .expect_err("adb error: listener 'tcp:3038' ");
    });
}

#[test]
#[ignore]
fn device_kill_forward_port_twice() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
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
#[ignore]
fn device_kill_forward_all_ports_no_forwarded_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
        device
            .kill_forward_all_ports()
            .expect("to not fail for no forwarded ports");
    });
}

#[test]
#[ignore]
fn device_kill_forward_all_ports_twice() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
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
#[ignore]
fn device_reverse_port_hardcoded() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
        assert_eq!(4035, device.reverse_port(4035, 4036).expect("remote_port"));
        // TODO: check with reverse --list
    });
}

// #[test]
// #[ignore]
// TODO: No adb response: ParseInt(ParseIntError { kind: Empty })
// fn device_reverse_port_system_allocated() {
//     run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
//         let reverse_port = device.reverse_port(0, 4037).expect("remote port");
//         assert_ne!(reverse_port, 0);
//         // TODO: check with reverse --list
//     });
// }

#[test]
#[ignore]
fn device_kill_reverse_port_no_reverse_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
        device
            .kill_reverse_port(4038)
            .expect_err("listener 'tcp:4038' not found");
    });
}

// #[test]
// #[ignore]
// TODO: "adb error: adb server response did not contain expected hexstring length: \"\""
// fn device_kill_reverse_port_twice() {
//     run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
//         let remote_port = device
//             .reverse_port(4039, 4040)
//             .expect("reversed local port");
//         assert_eq!(remote_port, 4039);
//         // TODO: check with reverse --list
//         device
//             .kill_reverse_port(remote_port)
//             .expect("to remove reverse port");
//         device
//             .kill_reverse_port(remote_port)
//             .expect_err("listener 'tcp:4039' not found");
//     });
// }

#[test]
#[ignore]
fn device_kill_reverse_all_ports_no_reversed_port() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
        device
            .kill_reverse_all_ports()
            .expect("to not fail for no reversed ports");
    });
}

#[test]
#[ignore]
fn device_kill_reverse_all_ports_twice() {
    run_device_test(|device: &Device, _: &TempDir, _: &UnixPath| {
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
#[ignore]
fn device_push() {
    run_device_test(
        |device: &Device, _: &TempDir, remote_root_path: &UnixPath| {
            fn adjust_mode(mode: u32) -> u32 {
                // Adjust the mode by copying the user permissions to
                // group and other as indicated in
                // [send_impl](https://android.googlesource.com/platform/system/core/+/master/adb/daemon/file_sync_service.cpp#516).
                // This ensures that group and other can both access a
                // file if the user can access it.
                let mut m = mode & 0o777;
                m |= (m >> 3) & 0o070;
                m |= (m >> 3) & 0o007;
                m
            }

            fn get_permissions(mode: u32) -> String {
                // Convert the mode integer into the string representation
                // of the mode returned by `ls`. This assumes the object is
                // a file and not a directory.
                let mut perms = vec!["-", "r", "w", "x", "r", "w", "x", "r", "w", "x"];
                let mut bit_pos = 0;
                while bit_pos < 9 {
                    if (1 << bit_pos) & mode == 0 {
                        perms[9 - bit_pos] = "-"
                    }
                    bit_pos += 1;
                }
                perms.concat()
            }
            let content = "test";
            let remote_path = remote_root_path.join("foo.bar");

            let modes = vec![0o421, 0o644, 0o666, 0o777];
            for mode in modes {
                let adjusted_mode = adjust_mode(mode);
                let adjusted_perms = get_permissions(adjusted_mode);
                device
                    .push(
                        &mut io::BufReader::new(content.as_bytes()),
                        &remote_path,
                        mode,
                    )
                    .expect("file has been pushed");

                let output = device
                    .execute_host_shell_command(&format!("ls -l {}", remote_path.display()))
                    .expect("host shell command for 'ls' to succeed");

                assert!(output.contains(remote_path.to_str().unwrap()));
                assert!(output.starts_with(&adjusted_perms));
            }

            let output = device
                .execute_host_shell_command(&format!("ls -ld {}", remote_root_path.display()))
                .expect("host shell command for 'ls parent' to succeed");

            assert!(output.contains(remote_root_path.to_str().unwrap()));
            assert!(output.starts_with("drwxrwxrwx"));

            let file_content = device
                .execute_host_shell_command(&format!("cat {}", remote_path.display()))
                .expect("host shell command for 'cat' to succeed");

            assert_eq!(file_content, content);
        },
    );
}

#[test]
#[ignore]
fn device_push_dir() {
    run_device_test(
        |device: &Device, tmp_dir: &TempDir, remote_root_path: &UnixPath| {
            let content = "test";

            let files = [
                PathBuf::from("foo1.bar"),
                PathBuf::from("foo2.bar"),
                PathBuf::from("bar").join("foo3.bar"),
                PathBuf::from("bar")
                    .join("more")
                    .join("baz")
                    .join("moar")
                    .join("foo3.bar"),
            ];

            for file in files.iter() {
                let path = tmp_dir.path().join(&file);
                let _ = std::fs::create_dir_all(path.parent().unwrap());

                let f = File::create(path).expect("to create file");
                let mut f = io::BufWriter::new(f);
                f.write_all(content.as_bytes()).expect("to write data");
            }

            device
                .push_dir(tmp_dir.path(), &remote_root_path, 0o777)
                .expect("to push_dir");

            for file in files.iter() {
                let path = append_components(remote_root_path, file).unwrap();
                let output = device
                    .execute_host_shell_command(&format!("ls {}", path.display()))
                    .expect("host shell command for 'ls' to succeed");

                assert!(output.contains(path.to_str().unwrap()));
            }
        },
    );
}

#[test]
fn format_own_device_error_types() {
    assert_eq!(
        format!("{}", DeviceError::InvalidStorage),
        "Invalid storage".to_string()
    );
    assert_eq!(
        format!("{}", DeviceError::MissingPackage),
        "Missing package".to_string()
    );
    assert_eq!(
        format!("{}", DeviceError::MultipleDevices),
        "Multiple Android devices online".to_string()
    );

    assert_eq!(
        format!("{}", DeviceError::Adb("foo".to_string())),
        "foo".to_string()
    );
}
