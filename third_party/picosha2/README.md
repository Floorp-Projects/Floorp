# PicoSHA2 - a C++ SHA256 hash generator

Copyright &copy; 2017 okdshin

## Introduction

PicoSHA2 is a tiny SHA256 hash generator for C++ with following properties:

- header-file only
- no external dependencies (only uses standard C++ libraries)
- STL-friendly
- licensed under MIT License

## Generating SHA256 hash and hash hex string

```cpp
// any STL sequantial container (vector, list, dequeue...)
std::string src_str = "The quick brown fox jumps over the lazy dog";

std::vector<unsigned char> hash(picosha2::k_digest_size);
picosha2::hash256(src_str.begin(), src_str.end(), hash.begin(), hash.end());

std::string hex_str = picosha2::bytes_to_hex_string(hash.begin(), hash.end());
```

## Generating SHA256 hash and hash hex string from byte stream

```cpp
picosha2::hash256_one_by_one hasher;
...
hasher.process(block.begin(), block.end());
...
hasher.finish();

std::vector<unsigned char> hash(picosha2::k_digest_size);
hasher.get_hash_bytes(hash.begin(), hash.end());

std::string hex_str = picosha2::get_hash_hex_string(hasher);
```

The file `example/interactive_hasher.cpp` has more detailed information.

## Generating SHA256 hash from a binary file

```cpp
std::ifstream f("file.txt", std::ios::binary);
std::vector<unsigned char> s(picosha2::k_digest_size);
picosha2::hash256(f, s.begin(), s.end());
```

This `hash256` may use less memory than reading whole of the file.

## Generating SHA256 hash hex string from std::string

```cpp
std::string src_str = "The quick brown fox jumps over the lazy dog";
std::string hash_hex_str;
picosha2::hash256_hex_string(src_str, hash_hex_str);
std::cout << hash_hex_str << std::endl;
//this output is "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"
```

```cpp
std::string src_str = "The quick brown fox jumps over the lazy dog";
std::string hash_hex_str = picosha2::hash256_hex_string(src_str);
std::cout << hash_hex_str << std::endl;
//this output is "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"
```

```cpp
std::string src_str = "The quick brown fox jumps over the lazy dog.";//add '.'
std::string hash_hex_str = picosha2::hash256_hex_string(src_str.begin(), src_str.end());
std::cout << hash_hex_str << std::endl;
//this output is "ef537f25c895bfa782526529a9b63d97aa631564d5d789c2b765448c8635fb6c"
```

## Generating SHA256 hash hex string from byte sequence

```cpp
std::vector<unsigned char> src_vect(...);
std::string hash_hex_str;
picosha2::hash256_hex_string(src_vect, hash_hex_str);
```

```cpp
std::vector<unsigned char> src_vect(...);
std::string hash_hex_str = picosha2::hash256_hex_string(src_vect);
```

```cpp
unsigned char src_c_array[picosha2::k_digest_size] = {...};
std::string hash_hex_str;
picosha2::hash256_hex_string(src_c_array, src_c_array+picosha2::k_digest_size, hash_hex_str);
```

```cpp
unsigned char src_c_array[picosha2::k_digest_size] = {...};
std::string hash_hex_str = picosha2::hash256_hex_string(src_c_array, src_c_array+picosha2::k_digest_size);
```


## Generating SHA256 hash byte sequence from STL sequential container

```cpp
//any STL sequantial container (vector, list, dequeue...)
std::string src_str = "The quick brown fox jumps over the lazy dog";

//any STL sequantial containers (vector, list, dequeue...)
std::vector<unsigned char> hash(picosha2::k_digest_size);

// in: container, out: container
picosha2::hash256(src_str, hash);
```

```cpp
//any STL sequantial container (vector, list, dequeue...)
std::string src_str = "The quick brown fox jumps over the lazy dog";

//any STL sequantial containers (vector, list, dequeue...)
std::vector<unsigned char> hash(picosha2::k_digest_size);

// in: iterator pair, out: contaner
picosha2::hash256(src_str.begin(), src_str.end(), hash);
```

```cpp
std::string src_str = "The quick brown fox jumps over the lazy dog";
unsigned char hash_byte_c_array[picosha2::k_digest_size];
// in: container, out: iterator(pointer) pair
picosha2::hash256(src_str, hash_byte_c_array, hash_byte_c_array+picosha2::k_digest_size);
```

```cpp
std::string src_str = "The quick brown fox jumps over the lazy dog";
std::vector<unsigned char> hash(picosha2::k_digest_size);
// in: iterator pair, out: iterator pair
picosha2::hash256(src_str.begin(), src_str.end(), hash.begin(), hash.end());
```
