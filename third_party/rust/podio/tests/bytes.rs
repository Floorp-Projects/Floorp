extern crate podio;

use std::io;
use podio::{LittleEndian, BigEndian};
use podio::{ReadPodExt, WritePodExt};

#[test]
fn write_be() {
    let buf: &mut [u8] = &mut [0u8; 8];
    let mut writer = io::Cursor::new(buf);

    writer.set_position(0);
    writer.write_u64::<BigEndian>(0x01_23_45_67_89_ab_cd_ef).unwrap();
    assert_eq!(&writer.get_ref()[0..8], &[0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef]);

    writer.set_position(0);
    writer.write_u32::<BigEndian>(0x01_23_45_67).unwrap();
    assert_eq!(&writer.get_ref()[0..4], &[0x01, 0x23, 0x45, 0x67]);

    writer.set_position(0);
    writer.write_u16::<BigEndian>(0x01_23).unwrap();
    assert_eq!(&writer.get_ref()[0..2], &[0x01, 0x23]);
}

#[test]
fn write_le() {
    let buf: &mut [u8] = &mut [0u8; 8];
    let mut writer = io::Cursor::new(buf);

    writer.set_position(0);
    writer.write_u64::<LittleEndian>(0x01_23_45_67_89_ab_cd_ef).unwrap();
    assert_eq!(&writer.get_ref()[0..8], &[0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01]);

    writer.set_position(0);
    writer.write_u32::<LittleEndian>(0x01_23_45_67).unwrap();
    assert_eq!(&writer.get_ref()[0..4], &[0x67, 0x45, 0x23, 0x01]);

    writer.set_position(0);
    writer.write_u16::<LittleEndian>(0x01_23).unwrap();
    assert_eq!(&writer.get_ref()[0..2], &[0x23, 0x01]);
}

#[test]
fn write_octet() {
    let buf: &mut [u8] = &mut [0u8; 8];
    let mut writer = io::Cursor::new(buf);

    writer.set_position(0);
    writer.write_u8(0x01).unwrap();
    assert_eq!(&writer.get_ref()[0..1], &[0x01]);
}

#[test]
fn write_float() {
    let buf: &mut [u8] = &mut [0u8; 8];
    let mut writer = io::Cursor::new(buf);

    writer.set_position(0);
    writer.write_f32::<LittleEndian>(10.12f32).unwrap();
    assert_eq!(&writer.get_ref()[0..4], &[0x85, 0xEB, 0x21, 0x41]);

    writer.set_position(0);
    writer.write_f32::<BigEndian>(10.12f32).unwrap();
    assert_eq!(&writer.get_ref()[0..4], &[0x41, 0x21, 0xEB, 0x85]);

    writer.set_position(0);
    writer.write_f64::<LittleEndian>(10.12f64).unwrap();
    assert_eq!(&writer.get_ref()[0..8], &[0x3D, 0x0A, 0xD7, 0xA3, 0x70, 0x3D, 0x24, 0x40]);

    writer.set_position(0);
    writer.write_f64::<BigEndian>(10.12f64).unwrap();
    assert_eq!(&writer.get_ref()[0..8], &[0x40, 0x24, 0x3D, 0x70, 0xA3, 0xD7, 0x0A, 0x3D]);
}

#[test]
fn read_be() {
    let buf: &[u8] = &[0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef];
    let mut reader = io::Cursor::new(buf);

    reader.set_position(0);
    assert_eq!(reader.read_u64::<BigEndian>().unwrap(), 0x0123456789abcdef);

    reader.set_position(0);
    assert_eq!(reader.read_u32::<BigEndian>().unwrap(), 0x01234567);

    reader.set_position(0);
    assert_eq!(reader.read_u16::<BigEndian>().unwrap(), 0x0123);
}

#[test]
fn read_le() {
    let buf: &[u8] = &[0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef];
    let mut reader = io::Cursor::new(buf);

    reader.set_position(0);
    assert_eq!(reader.read_u64::<LittleEndian>().unwrap(), 0xefcdab8967452301);

    reader.set_position(0);
    assert_eq!(reader.read_u32::<LittleEndian>().unwrap(), 0x67452301);

    reader.set_position(0);
    assert_eq!(reader.read_u16::<LittleEndian>().unwrap(), 0x2301);
}

#[test]
fn read_octet() {
    let buf: &[u8] = &[0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef];
    let mut reader = io::Cursor::new(buf);

    reader.set_position(0);
    assert_eq!(reader.read_u8().unwrap(), 0x01);
}

#[test]
fn read_float() {
    let mut buf: &[u8] = &[0x41, 0x21, 0xEB, 0x85];
    assert_eq!(buf.read_f32::<BigEndian>().unwrap(), 10.12f32);

    let mut buf: &[u8] = &[0x85, 0xEB, 0x21, 0x41];
    assert_eq!(buf.read_f32::<LittleEndian>().unwrap(), 10.12f32);

    let mut buf: &[u8] = &[0x40, 0x24, 0x3D, 0x70, 0xA3, 0xD7, 0x0A, 0x3D];
    assert_eq!(buf.read_f64::<BigEndian>().unwrap(), 10.12f64);

    let mut buf: &[u8] = &[0x3D, 0x0A, 0xD7, 0xA3, 0x70, 0x3D, 0x24, 0x40];
    assert_eq!(buf.read_f64::<LittleEndian>().unwrap(), 10.12f64);
}

#[test]
fn read_exact() {
    let mut buf: &[u8] = &[1, 2, 3, 4, 5, 6, 7, 8];
    assert_eq!(buf.read_exact(2).unwrap(), [1,2]);
    assert_eq!(buf.read_exact(1).unwrap(), [3]);
    assert_eq!(buf.read_exact(0).unwrap(), []);
    assert_eq!(buf.read_exact(5).unwrap(), [4,5,6,7,8]);
    assert_eq!(buf.read_exact(0).unwrap(), []);
    assert!(buf.read_exact(1).is_err());
    assert_eq!(buf.read_exact(0).unwrap(), []);
}
