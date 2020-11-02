use std::{
    collections::hash_map::{HashMap, RandomState},
    sync::Arc,
    thread, time,
};

#[test]
fn fill_ten() {
    type Map = HashMap<u8, String, RandomState>;
    let s = RandomState::new();
    let map = storage_map::StorageMap::<parking_lot::RawRwLock, Map>::with_hasher(s);
    let arc = Arc::new(map);
    let join_handles = (0..10000usize)
        .map(|i| {
            let arc = Arc::clone(&arc);
            let key = (i % 10) as u8;
            thread::spawn(move || {
                arc.get_or_create_with(&key, || {
                    thread::sleep(time::Duration::new(0, 100));
                    format!("{}", i)
                });
            })
        })
        .collect::<Vec<_>>();
    for handle in join_handles {
        let _ = handle.join();
    }
}

#[test]
fn whole_write() {
    type Map = HashMap<u8, String, RandomState>;
    let map =
        storage_map::StorageMap::<parking_lot::RawRwLock, Map>::with_hasher(RandomState::new());
    map.get_or_create_with(&3, || "three".to_owned());
    map.get_or_create_with(&5, || "five".to_owned());
    let mut guard = map.whole_write();
    for (_key, _value) in guard.drain() {
        //nothing
    }
}
