use audioipc::codec::{Codec, LengthDelimitedCodec};
use audioipc::messages::DeviceInfo;
use audioipc::ClientMessage;
use audioipc2 as audioipc;
use bytes::BytesMut;
use criterion::{criterion_group, criterion_main, BatchSize, Criterion};

fn bench(c: &mut Criterion, name: &str, msg: impl Fn() -> ClientMessage) {
    let mut codec: LengthDelimitedCodec<ClientMessage, ClientMessage> =
        LengthDelimitedCodec::default();
    let mut buf = BytesMut::with_capacity(8192);
    c.bench_function(&format!("encode/{}", name), |b| {
        b.iter_batched(
            || msg(),
            |msg| {
                codec.encode(msg, &mut buf).unwrap();
                buf.clear();
            },
            BatchSize::SmallInput,
        )
    });

    let mut codec: LengthDelimitedCodec<ClientMessage, ClientMessage> =
        LengthDelimitedCodec::default();
    let mut buf = BytesMut::with_capacity(8192);
    codec.encode(msg(), &mut buf).unwrap();
    c.bench_function(&format!("decode/{}", name), |b| {
        b.iter_batched_ref(
            || buf.clone(),
            |buf| {
                codec.decode(buf).unwrap().unwrap();
            },
            BatchSize::SmallInput,
        )
    });

    let mut codec: LengthDelimitedCodec<ClientMessage, ClientMessage> =
        LengthDelimitedCodec::default();
    let mut buf = BytesMut::with_capacity(8192);
    c.bench_function(&format!("roundtrip/{}", name), |b| {
        b.iter_batched(
            || msg(),
            |msg| {
                codec.encode(msg, &mut buf).unwrap();
                codec.decode(&mut buf).unwrap().unwrap();
            },
            BatchSize::SmallInput,
        )
    });
}

pub fn criterion_benchmark(c: &mut Criterion) {
    bench(c, "tiny", || ClientMessage::ClientConnected);
    bench(c, "small", || ClientMessage::StreamPosition(0));
    bench(c, "medium", || {
        ClientMessage::ContextEnumeratedDevices(make_device_vec(2))
    });
    bench(c, "large", || {
        ClientMessage::ContextEnumeratedDevices(make_device_vec(20))
    });
    bench(c, "huge", || {
        ClientMessage::ContextEnumeratedDevices(make_device_vec(128))
    });
}

criterion_group!(benches, criterion_benchmark);
criterion_main!(benches);

fn make_device_vec(n: usize) -> Vec<DeviceInfo> {
    let mut devices = Vec::with_capacity(n);
    for i in 0..n {
        let device = DeviceInfo {
            devid: i,
            device_id: Some(vec![0u8; 64]),
            friendly_name: Some(vec![0u8; 64]),
            group_id: Some(vec![0u8; 64]),
            vendor_name: Some(vec![0u8; 64]),
            device_type: 0,
            state: 0,
            preferred: 0,
            format: 0,
            default_format: 0,
            max_channels: 0,
            default_rate: 0,
            max_rate: 0,
            min_rate: 0,
            latency_lo: 0,
            latency_hi: 0,
        };
        devices.push(device);
    }
    devices
}
