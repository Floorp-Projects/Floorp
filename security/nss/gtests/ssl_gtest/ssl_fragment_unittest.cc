/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secerr.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include "gtest_utils.h"
#include "scoped_ptrs.h"
#include "tls_connect.h"
#include "tls_filter.h"
#include "tls_parser.h"

namespace nss_test {

// This class cuts every unencrypted handshake record into two parts.
class RecordFragmenter : public PacketFilter {
 public:
  RecordFragmenter(bool is_dtls13)
      : is_dtls13_(is_dtls13), sequence_number_(0), splitting_(true) {}

 private:
  class HandshakeSplitter {
   public:
    HandshakeSplitter(bool is_dtls13, const DataBuffer& input,
                      DataBuffer* output, uint64_t* sequence_number)
        : is_dtls13_(is_dtls13),
          input_(input),
          output_(output),
          cursor_(0),
          sequence_number_(sequence_number) {}

   private:
    void WriteRecord(TlsRecordHeader& record_header,
                     DataBuffer& record_fragment) {
      TlsRecordHeader fragment_header(
          record_header.variant(), record_header.version(),
          record_header.content_type(), *sequence_number_);
      ++*sequence_number_;
      if (::g_ssl_gtest_verbose) {
        std::cerr << "Fragment: " << fragment_header << ' ' << record_fragment
                  << std::endl;
      }
      cursor_ = fragment_header.Write(output_, cursor_, record_fragment);
    }

    bool SplitRecord(TlsRecordHeader& record_header, DataBuffer& record) {
      TlsParser parser(record);
      while (parser.remaining()) {
        TlsHandshakeFilter::HandshakeHeader handshake_header;
        DataBuffer handshake_body;
        bool complete = false;
        if (!handshake_header.Parse(&parser, record_header, DataBuffer(),
                                    &handshake_body, &complete)) {
          ADD_FAILURE() << "couldn't parse handshake header";
          return false;
        }
        if (!complete) {
          ADD_FAILURE() << "don't want to deal with fragmented messages";
          return false;
        }

        DataBuffer record_fragment;
        // We can't fragment handshake records that are too small.
        if (handshake_body.len() < 2) {
          handshake_header.Write(&record_fragment, 0U, handshake_body);
          WriteRecord(record_header, record_fragment);
          continue;
        }

        size_t cut = handshake_body.len() / 2;
        handshake_header.WriteFragment(&record_fragment, 0U, handshake_body, 0U,
                                       cut);
        WriteRecord(record_header, record_fragment);

        handshake_header.WriteFragment(&record_fragment, 0U, handshake_body,
                                       cut, handshake_body.len() - cut);
        WriteRecord(record_header, record_fragment);
      }
      return true;
    }

   public:
    bool Split() {
      TlsParser parser(input_);
      while (parser.remaining()) {
        TlsRecordHeader header;
        DataBuffer record;
        if (!header.Parse(is_dtls13_, 0, &parser, &record)) {
          ADD_FAILURE() << "bad record header";
          return false;
        }

        if (::g_ssl_gtest_verbose) {
          std::cerr << "Record: " << header << ' ' << record << std::endl;
        }

        // Don't touch packets from a non-zero epoch.  Leave these unmodified.
        if ((header.sequence_number() >> 48) != 0ULL) {
          cursor_ = header.Write(output_, cursor_, record);
          continue;
        }

        // Just rewrite the sequence number (CCS only).
        if (header.content_type() != ssl_ct_handshake) {
          EXPECT_EQ(ssl_ct_change_cipher_spec, header.content_type());
          WriteRecord(header, record);
          continue;
        }

        if (!SplitRecord(header, record)) {
          return false;
        }
      }
      return true;
    }

   private:
    bool is_dtls13_;
    const DataBuffer& input_;
    DataBuffer* output_;
    size_t cursor_;
    uint64_t* sequence_number_;
  };

 protected:
  virtual PacketFilter::Action Filter(const DataBuffer& input,
                                      DataBuffer* output) override {
    if (!splitting_) {
      return KEEP;
    }

    output->Allocate(input.len());
    HandshakeSplitter splitter(is_dtls13_, input, output, &sequence_number_);
    if (!splitter.Split()) {
      // If splitting fails, we obviously reached encrypted packets.
      // Stop splitting from that point onward.
      splitting_ = false;
      return KEEP;
    }

    return CHANGE;
  }

 private:
  bool is_dtls13_;
  uint64_t sequence_number_;
  bool splitting_;
};

TEST_P(TlsConnectDatagram, FragmentClientPackets) {
  bool is_dtls13 = version_ >= SSL_LIBRARY_VERSION_TLS_1_3;
  client_->SetFilter(std::make_shared<RecordFragmenter>(is_dtls13));
  Connect();
  SendReceive();
}

TEST_P(TlsConnectDatagram, FragmentServerPackets) {
  bool is_dtls13 = version_ >= SSL_LIBRARY_VERSION_TLS_1_3;
  server_->SetFilter(std::make_shared<RecordFragmenter>(is_dtls13));
  Connect();
  SendReceive();
}

}  // namespace nss_test
