/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "shared.h"
#include "tls_parser.h"

#include "ssl.h"
extern "C" {
#include "sslimpl.h"
}

using namespace nss_test;

// Number of additional bytes in the TLS header.
// Used to properly skip DTLS seqnums.
static size_t gExtraHeaderBytes = 0;

// Helper class to simplify TLS record manipulation.
class Record {
 public:
  static std::unique_ptr<Record> Create(const uint8_t *data, size_t size,
                                        size_t remaining) {
    return std::unique_ptr<Record>(new Record(data, size, remaining));
  }

  void insert_before(const std::unique_ptr<Record> &other) {
    assert(data_ && size_ > 0);

    // Copy data in case other == this.
    uint8_t buf[size_];
    memcpy(buf, data_, size_);

    uint8_t *dest = const_cast<uint8_t *>(other->data());
    // Make room for the record we want to insert.
    memmove(dest + size_, other->data(), other->size() + other->remaining());
    // Insert the record.
    memcpy(dest, buf, size_);
  }

  void truncate(size_t length) {
    assert(length >= 5 + gExtraHeaderBytes);
    uint8_t *dest = const_cast<uint8_t *>(data_);
    size_t l = length - (5 + gExtraHeaderBytes);
    dest[3] = (l >> 8) & 0xff;
    dest[4] = l & 0xff;
    memmove(dest + length, data_ + size_, remaining_);
  }

  void drop() {
    uint8_t *dest = const_cast<uint8_t *>(data_);
    memmove(dest, data_ + size_, remaining_);
  }

  const uint8_t *data() { return data_; }
  size_t remaining() { return remaining_; }
  size_t size() { return size_; }

 private:
  Record(const uint8_t *data, size_t size, size_t remaining)
      : data_(data), remaining_(remaining), size_(size) {}

  const uint8_t *data_;
  size_t remaining_;
  size_t size_;
};

// Parse records contained in a given TLS transcript.
std::vector<std::unique_ptr<Record>> ParseRecords(const uint8_t *data,
                                                  size_t size) {
  std::vector<std::unique_ptr<Record>> records;
  TlsParser parser(data, size);

  while (parser.remaining()) {
    size_t offset = parser.consumed();

    // Skip type, version, and DTLS seqnums.
    if (!parser.Skip(3 + gExtraHeaderBytes)) {
      break;
    }

    DataBuffer fragment;
    if (!parser.ReadVariable(&fragment, 2)) {
      break;
    }

    records.push_back(Record::Create(data + offset,
                                     fragment.len() + 5 + gExtraHeaderBytes,
                                     parser.remaining()));
  }

  return records;
}

namespace TlsMutators {

// Handle seqnums in DTLS transcripts.
void SetIsDTLS() { gExtraHeaderBytes = 8; }

// Mutator that drops whole TLS records.
size_t DropRecord(uint8_t *data, size_t size, size_t max_size,
                  unsigned int seed) {
  std::mt19937 rng(seed);

  // Find TLS records in the corpus.
  auto records = ParseRecords(data, size);
  if (records.empty()) {
    return 0;
  }

  // Pick a record to drop at random.
  std::uniform_int_distribution<size_t> dist(0, records.size() - 1);
  auto &rec = records.at(dist(rng));

  // Drop the record.
  rec->drop();

  // Return the new final size.
  return size - rec->size();
}

// Mutator that shuffles TLS records in a transcript.
size_t ShuffleRecords(uint8_t *data, size_t size, size_t max_size,
                      unsigned int seed) {
  std::mt19937 rng(seed);

  // Store the original corpus.
  uint8_t buf[size];
  memcpy(buf, data, size);

  // Find TLS records in the corpus.
  auto records = ParseRecords(buf, sizeof(buf));
  if (records.empty()) {
    return 0;
  }

  // Find offset of first record in target buffer.
  uint8_t *dest = const_cast<uint8_t *>(ParseRecords(data, size).at(0)->data());

  // Shuffle record order.
  std::shuffle(records.begin(), records.end(), rng);

  // Write records to their new positions.
  for (auto &rec : records) {
    memcpy(dest, rec->data(), rec->size());
    dest += rec->size();
  }

  // Final size hasn't changed.
  return size;
}

// Mutator that duplicates a single TLS record and randomly inserts it.
size_t DuplicateRecord(uint8_t *data, size_t size, size_t max_size,
                       unsigned int seed) {
  std::mt19937 rng(seed);

  // Find TLS records in the corpus.
  const auto records = ParseRecords(data, size);
  if (records.empty()) {
    return 0;
  }

  // Pick a record to duplicate at random.
  std::uniform_int_distribution<size_t> dist(0, records.size() - 1);
  auto &rec = records.at(dist(rng));
  if (size + rec->size() > max_size) {
    return 0;
  }

  // Insert before random record.
  rec->insert_before(records.at(dist(rng)));

  // Return the new final size.
  return size + rec->size();
}

// Mutator that truncates a TLS record.
size_t TruncateRecord(uint8_t *data, size_t size, size_t max_size,
                      unsigned int seed) {
  std::mt19937 rng(seed);

  // Find TLS records in the corpus.
  const auto records = ParseRecords(data, size);
  if (records.empty()) {
    return 0;
  }

  // Pick a record to truncate at random.
  std::uniform_int_distribution<size_t> dist(0, records.size() - 1);
  auto &rec = records.at(dist(rng));

  // Need a record with data.
  if (rec->size() <= 5 + gExtraHeaderBytes) {
    return 0;
  }

  // Truncate.
  std::uniform_int_distribution<size_t> dist2(5 + gExtraHeaderBytes,
                                              rec->size() - 1);
  size_t new_length = dist2(rng);
  rec->truncate(new_length);

  // Return the new final size.
  return size + new_length - rec->size();
}

// Mutator that splits a TLS record in two.
size_t FragmentRecord(uint8_t *data, size_t size, size_t max_size,
                      unsigned int seed) {
  std::mt19937 rng(seed);

  // We can't deal with DTLS yet.
  if (gExtraHeaderBytes > 0) {
    return 0;
  }

  if (size + 5 > max_size) {
    return 0;
  }

  // Find TLS records in the corpus.
  const auto records = ParseRecords(data, size);
  if (records.empty()) {
    return 0;
  }

  // Pick a record to fragment at random.
  std::uniform_int_distribution<size_t> rand_record(0, records.size() - 1);
  auto &rec = records.at(rand_record(rng));
  uint8_t *rdata = const_cast<uint8_t *>(rec->data());
  size_t length = rec->size();
  size_t content_length = length - 5;

  if (content_length < 2) {
    return 0;
  }

  // Assign a new length to the first fragment.
  std::uniform_int_distribution<size_t> rand_size(1, content_length - 1);
  size_t first_length = rand_size(rng);
  size_t second_length = content_length - first_length;
  rdata[3] = (first_length >> 8) & 0xff;
  rdata[4] = first_length & 0xff;
  uint8_t *second_record = rdata + 5 + first_length;

  // Make room for the header of the second record.
  memmove(second_record + 5, second_record,
          rec->remaining() + content_length - first_length);

  // Write second header.
  memcpy(second_record, rdata, 3);
  second_record[3] = (second_length >> 8) & 0xff;
  second_record[4] = second_length & 0xff;

  return size + 5;
}

// Cross-over function that merges and shuffles two transcripts.
size_t CrossOver(const uint8_t *data1, size_t size1, const uint8_t *data2,
                 size_t size2, uint8_t *out, size_t max_out_size,
                 unsigned int seed) {
  std::mt19937 rng(seed);

  // Find TLS records in the corpus.
  auto records1 = ParseRecords(data1, size1);
  if (records1.empty()) {
    return 0;
  }

  {  // Merge the two vectors.
    auto records2 = ParseRecords(data2, size2);
    if (records2.empty()) {
      return 0;
    }
    std::move(records2.begin(), records2.end(), std::back_inserter(records1));
  }

  // Shuffle record order.
  std::shuffle(records1.begin(), records1.end(), rng);

  size_t total = 0;
  for (auto &rec : records1) {
    size_t length = rec->size();
    if (total + length > max_out_size) {
      break;
    }

    // Write record to its new position.
    memcpy(out + total, rec->data(), length);
    total += length;
  }

  return total;
}

}  // namespace TlsMutators
