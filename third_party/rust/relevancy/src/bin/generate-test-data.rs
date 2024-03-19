use relevancy::{
    url_hash::{hash_url, UrlHash},
    Interest,
};
use std::{collections::HashMap, fs::File, io::Write};

// Generate a set of test data and output it to the `test-data` file.
//
// This is meant to be a placeholder until we can get this data stored in remote settings.

const TEST_INTEREST_DATA: &[(&str, Interest)] = &[
    ("https://espn.com/", Interest::Sports),
    ("https://dogs.com/", Interest::Animals),
    ("https://cars.com/", Interest::Autos),
    ("https://www.vouge.com/", Interest::Fashion),
    ("https://slashdot.org/", Interest::Tech),
    ("https://www.nascar.com/", Interest::Autos),
    ("https://www.nascar.com/", Interest::Sports),
];

fn main() {
    let mut interest_map: HashMap<Interest, Vec<UrlHash>> =
        HashMap::from_iter(Interest::all().into_iter().map(|i| (i, vec![])));
    for (url, interest) in TEST_INTEREST_DATA {
        if let Some(hash) = hash_url(url) {
            interest_map.get_mut(interest).unwrap().push(hash)
        }
    }

    let mut f = File::create("test-data").expect("Error opening file");
    // Loop over all possible interests
    for interest in Interest::all() {
        // Get the list of URL hashes for that interest
        let hashes = interest_map.get(&interest).unwrap();
        // Write the count
        f.write_all(&(hashes.len() as u32).to_le_bytes())
            .expect("Error writing file");
        // Write the hashes
        for hash in hashes {
            f.write_all(hash).expect("Error writing file");
        }
    }
}
