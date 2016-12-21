/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "gtest/gtest.h"
#include "Helpers.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIBufferedStreams.h"
#include "nsIClassInfo.h"
#include "nsICloneableInputStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIPipe.h"
#include "nsISeekableStream.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "prprf.h"
#include "prinrval.h"

using namespace mozilla;

#define ITERATIONS      33333
char kTestPattern[] = "My hovercraft is full of eels.\n";

bool gTrace = false;

static nsresult
WriteAll(nsIOutputStream *os, const char *buf, uint32_t bufLen, uint32_t *lenWritten)
{
    const char *p = buf;
    *lenWritten = 0;
    while (bufLen) {
        uint32_t n;
        nsresult rv = os->Write(p, bufLen, &n);
        if (NS_FAILED(rv)) return rv;
        p += n;
        bufLen -= n;
        *lenWritten += n;
    }
    return NS_OK;
}

class nsReceiver final : public nsIRunnable {
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD Run() override {
        nsresult rv;
        char buf[101];
        uint32_t count;
        PRIntervalTime start = PR_IntervalNow();
        while (true) {
            rv = mIn->Read(buf, 100, &count);
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                break;
            }
            if (count == 0) {
//                printf("EOF count = %d\n", mCount);
                break;
            }

            if (gTrace) {
                buf[count] = '\0';
                printf("read: %s\n", buf);
            }
            mCount += count;
        }
        PRIntervalTime end = PR_IntervalNow();
        printf("read  %d bytes, time = %dms\n", mCount,
               PR_IntervalToMilliseconds(end - start));
        return rv;
    }

    explicit nsReceiver(nsIInputStream* in) : mIn(in), mCount(0) {
    }

    uint32_t GetBytesRead() { return mCount; }

private:
    ~nsReceiver() {}

protected:
    nsCOMPtr<nsIInputStream> mIn;
    uint32_t            mCount;
};

NS_IMPL_ISUPPORTS(nsReceiver, nsIRunnable)

nsresult
TestPipe(nsIInputStream* in, nsIOutputStream* out)
{
    RefPtr<nsReceiver> receiver = new nsReceiver(in);
    if (!receiver)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    nsCOMPtr<nsIThread> thread;
    rv = NS_NewNamedThread("TestPipe", getter_AddRefs(thread), receiver);
    if (NS_FAILED(rv)) return rv;

    uint32_t total = 0;
    PRIntervalTime start = PR_IntervalNow();
    for (uint32_t i = 0; i < ITERATIONS; i++) {
        uint32_t writeCount;
        char *buf = PR_smprintf("%d %s", i, kTestPattern);
        uint32_t len = strlen(buf);
        rv = WriteAll(out, buf, len, &writeCount);
        if (gTrace) {
            printf("wrote: ");
            for (uint32_t j = 0; j < writeCount; j++) {
                putc(buf[j], stdout);
            }
            printf("\n");
        }
        PR_smprintf_free(buf);
        if (NS_FAILED(rv)) return rv;
        total += writeCount;
    }
    rv = out->Close();
    if (NS_FAILED(rv)) return rv;

    PRIntervalTime end = PR_IntervalNow();

    thread->Shutdown();

    printf("wrote %d bytes, time = %dms\n", total,
           PR_IntervalToMilliseconds(end - start));
    EXPECT_EQ(receiver->GetBytesRead(), total);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsShortReader final : public nsIRunnable {
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD Run() override {
        nsresult rv;
        char buf[101];
        uint32_t count;
        uint32_t total = 0;
        while (true) {
            //if (gTrace)
            //    printf("calling Read\n");
            rv = mIn->Read(buf, 100, &count);
            if (NS_FAILED(rv)) {
                printf("read failed\n");
                break;
            }
            if (count == 0) {
                break;
            }

            if (gTrace) {
                // For next |printf()| call and possible others elsewhere.
                buf[count] = '\0';

                printf("read %d bytes: %s\n", count, buf);
            }

            Received(count);
            total += count;
        }
        printf("read  %d bytes\n", total);
        return rv;
    }

    explicit nsShortReader(nsIInputStream* in) : mIn(in), mReceived(0) {
        mMon = new ReentrantMonitor("nsShortReader");
    }

    void Received(uint32_t count) {
        ReentrantMonitorAutoEnter mon(*mMon);
        mReceived += count;
        mon.Notify();
    }

    uint32_t WaitForReceipt(const uint32_t aWriteCount) {
        ReentrantMonitorAutoEnter mon(*mMon);
        uint32_t result = mReceived;

        while (result < aWriteCount) {
            mon.Wait();

            EXPECT_TRUE(mReceived > result);
            result = mReceived;
        }

        mReceived = 0;
        return result;
    }

private:
    ~nsShortReader() {}

protected:
    nsCOMPtr<nsIInputStream> mIn;
    uint32_t                 mReceived;
    ReentrantMonitor*        mMon;
};

NS_IMPL_ISUPPORTS(nsShortReader, nsIRunnable)

nsresult
TestShortWrites(nsIInputStream* in, nsIOutputStream* out)
{
    RefPtr<nsShortReader> receiver = new nsShortReader(in);
    if (!receiver)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;

    nsCOMPtr<nsIThread> thread;
    rv = NS_NewNamedThread("TestShortWrites", getter_AddRefs(thread),
                           receiver);
    if (NS_FAILED(rv)) return rv;

    uint32_t total = 0;
    for (uint32_t i = 0; i < ITERATIONS; i++) {
        uint32_t writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        uint32_t len = strlen(buf);
        len = len * rand() / RAND_MAX;
        len = std::min(1u, len);
        rv = WriteAll(out, buf, len, &writeCount);
        if (NS_FAILED(rv)) return rv;
        EXPECT_EQ(writeCount, len);
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);
        PR_smprintf_free(buf);
        //printf("calling Flush\n");
        out->Flush();
        //printf("calling WaitForReceipt\n");

#ifdef DEBUG
        const uint32_t received =
          receiver->WaitForReceipt(writeCount);
        EXPECT_EQ(received, writeCount);
#endif
    }
    rv = out->Close();
    if (NS_FAILED(rv)) return rv;

    thread->Shutdown();

    printf("wrote %d bytes\n", total);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsPump final : public nsIRunnable
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    NS_IMETHOD Run() override {
        nsresult rv;
        uint32_t count;
        while (true) {
            rv = mOut->WriteFrom(mIn, ~0U, &count);
            if (NS_FAILED(rv)) {
                printf("Write failed\n");
                break;
            }
            if (count == 0) {
                printf("EOF count = %d\n", mCount);
                break;
            }

            if (gTrace) {
                printf("Wrote: %d\n", count);
            }
            mCount += count;
        }
        mOut->Close();
        return rv;
    }

    nsPump(nsIInputStream* in,
           nsIOutputStream* out)
        : mIn(in), mOut(out), mCount(0) {
    }

private:
    ~nsPump() {}

protected:
    nsCOMPtr<nsIInputStream>      mIn;
    nsCOMPtr<nsIOutputStream>     mOut;
    uint32_t                            mCount;
};

NS_IMPL_ISUPPORTS(nsPump, nsIRunnable)

TEST(Pipes, ChainedPipes)
{
    nsresult rv;
    if (gTrace) {
        printf("TestChainedPipes\n");
    }

    nsCOMPtr<nsIInputStream> in1;
    nsCOMPtr<nsIOutputStream> out1;
    rv = NS_NewPipe(getter_AddRefs(in1), getter_AddRefs(out1), 20, 1999);
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsIInputStream> in2;
    nsCOMPtr<nsIOutputStream> out2;
    rv = NS_NewPipe(getter_AddRefs(in2), getter_AddRefs(out2), 200, 401);
    if (NS_FAILED(rv)) return;

    RefPtr<nsPump> pump = new nsPump(in1, out2);
    if (pump == nullptr) return;

    nsCOMPtr<nsIThread> thread;
    rv = NS_NewNamedThread("ChainedPipePump", getter_AddRefs(thread), pump);
    if (NS_FAILED(rv)) return;

    RefPtr<nsReceiver> receiver = new nsReceiver(in2);
    if (receiver == nullptr) return;

    nsCOMPtr<nsIThread> receiverThread;
    rv = NS_NewNamedThread("ChainedPipeRecv", getter_AddRefs(receiverThread),
                           receiver);
    if (NS_FAILED(rv)) return;

    uint32_t total = 0;
    for (uint32_t i = 0; i < ITERATIONS; i++) {
        uint32_t writeCount;
        char* buf = PR_smprintf("%d %s", i, kTestPattern);
        uint32_t len = strlen(buf);
        len = len * rand() / RAND_MAX;
        len = std::max(1u, len);
        rv = WriteAll(out1, buf, len, &writeCount);
        if (NS_FAILED(rv)) return;
        EXPECT_EQ(writeCount, len);
        total += writeCount;

        if (gTrace)
            printf("wrote %d bytes: %s\n", writeCount, buf);

        PR_smprintf_free(buf);
    }
    if (gTrace) {
        printf("wrote total of %d bytes\n", total);
    }
    rv = out1->Close();
    if (NS_FAILED(rv)) return;

    thread->Shutdown();
    receiverThread->Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

void
RunTests(uint32_t segSize, uint32_t segCount)
{
    nsresult rv;
    nsCOMPtr<nsIInputStream> in;
    nsCOMPtr<nsIOutputStream> out;
    uint32_t bufSize = segSize * segCount;
    if (gTrace) {
        printf("Testing New Pipes: segment size %d buffer size %d\n", segSize, bufSize);
        printf("Testing long writes...\n");
    }
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out), segSize, bufSize);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    rv = TestPipe(in, out);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    if (gTrace) {
        printf("Testing short writes...\n");
    }
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out), segSize, bufSize);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
    rv = TestShortWrites(in, out);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
}

TEST(Pipes, Main)
{
    RunTests(16, 1);
    RunTests(4096, 16);
}

////////////////////////////////////////////////////////////////////////////////

namespace {

static const uint32_t DEFAULT_SEGMENT_SIZE = 4 * 1024;

// An alternate pipe testing routing that uses NS_ConsumeStream() instead of
// manual read loop.
static void TestPipe2(uint32_t aNumBytes,
                      uint32_t aSegmentSize = DEFAULT_SEGMENT_SIZE)
{
  nsCOMPtr<nsIInputStream> reader;
  nsCOMPtr<nsIOutputStream> writer;

  uint32_t maxSize = std::max(aNumBytes, aSegmentSize);

  nsresult rv = NS_NewPipe(getter_AddRefs(reader), getter_AddRefs(writer),
                           aSegmentSize, maxSize);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> inputData;
  testing::CreateData(aNumBytes, inputData);
  testing::WriteAllAndClose(writer, inputData);
  testing::ConsumeAndValidateStream(reader, inputData);
}

} // namespace

TEST(Pipes, Blocking_32k)
{
  TestPipe2(32 * 1024);
}

TEST(Pipes, Blocking_64k)
{
  TestPipe2(64 * 1024);
}

TEST(Pipes, Blocking_128k)
{
  TestPipe2(128 * 1024);
}

////////////////////////////////////////////////////////////////////////////////

namespace {

// Utility routine to validate pipe clone before.  There are many knobs.
//
// aTotalBytes              Total number of bytes to write to the pipe.
// aNumWrites               How many separate write calls should be made.  Bytes
//                          are evenly distributed over these write calls.
// aNumInitialClones        How many clones of the pipe input stream should be
//                          made before writing begins.
// aNumToCloseAfterWrite    How many streams should be closed after each write.
//                          One stream is always kept open.  This verifies that
//                          closing one stream does not effect other open
//                          streams.
// aNumToCloneAfterWrite    How many clones to create after each write.  Occurs
//                          after closing any streams.  This tests cloning
//                          active streams on a pipe that is being written to.
// aNumStreamToReadPerWrite How many streams to read fully after each write.
//                          This tests reading cloned streams at different rates
//                          while the pipe is being written to.
static void TestPipeClone(uint32_t aTotalBytes,
                          uint32_t aNumWrites,
                          uint32_t aNumInitialClones,
                          uint32_t aNumToCloseAfterWrite,
                          uint32_t aNumToCloneAfterWrite,
                          uint32_t aNumStreamsToReadPerWrite,
                          uint32_t aSegmentSize = DEFAULT_SEGMENT_SIZE)
{
  nsCOMPtr<nsIInputStream> reader;
  nsCOMPtr<nsIOutputStream> writer;

  uint32_t maxSize = std::max(aTotalBytes, aSegmentSize);

  // Use async input streams so we can NS_ConsumeStream() the current data
  // while the pipe is still being written to.
  nsresult rv = NS_NewPipe(getter_AddRefs(reader), getter_AddRefs(writer),
                           aSegmentSize, maxSize,
                           true, false); // non-blocking - reader, writer
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsICloneableInputStream> cloneable = do_QueryInterface(reader);
  ASSERT_TRUE(cloneable);
  ASSERT_TRUE(cloneable->GetCloneable());

  nsTArray<nsCString> outputDataList;

  nsTArray<nsCOMPtr<nsIInputStream>> streamList;

  // first stream is our original reader from the pipe
  streamList.AppendElement(reader);
  outputDataList.AppendElement();

  // Clone the initial input stream the specified number of times
  // before performing any writes.
  for (uint32_t i = 0; i < aNumInitialClones; ++i) {
    nsCOMPtr<nsIInputStream>* clone = streamList.AppendElement();
    rv = cloneable->Clone(getter_AddRefs(*clone));
    ASSERT_TRUE(NS_SUCCEEDED(rv));
    ASSERT_TRUE(*clone);

    outputDataList.AppendElement();
  }

  nsTArray<char> inputData;
  testing::CreateData(aTotalBytes, inputData);

  const uint32_t bytesPerWrite = ((aTotalBytes - 1)/ aNumWrites) + 1;
  uint32_t offset = 0;
  uint32_t remaining = aTotalBytes;
  uint32_t nextStreamToRead = 0;

  while (remaining) {
    uint32_t numToWrite = std::min(bytesPerWrite, remaining);
    testing::Write(writer, inputData, offset, numToWrite);
    offset += numToWrite;
    remaining -= numToWrite;

    // Close the specified number of streams.  This allows us to
    // test that one closed clone does not break other open clones.
    for (uint32_t i = 0; i < aNumToCloseAfterWrite &&
                         streamList.Length() > 1; ++i) {

      uint32_t lastIndex = streamList.Length() - 1;
      streamList[lastIndex]->Close();
      streamList.RemoveElementAt(lastIndex);
      outputDataList.RemoveElementAt(lastIndex);

      if (nextStreamToRead >= streamList.Length()) {
        nextStreamToRead = 0;
      }
    }

    // Create the specified number of clones.  This lets us verify
    // that we can create clones in the middle of pipe reading and
    // writing.
    for (uint32_t i = 0; i < aNumToCloneAfterWrite; ++i) {
      nsCOMPtr<nsIInputStream>* clone = streamList.AppendElement();
      rv = cloneable->Clone(getter_AddRefs(*clone));
      ASSERT_TRUE(NS_SUCCEEDED(rv));
      ASSERT_TRUE(*clone);

      // Initialize the new output data to make whats been read to data for
      // the original stream.  First stream is always the original stream.
      nsCString* outputData = outputDataList.AppendElement();
      *outputData = outputDataList[0];
    }

    // Read the specified number of streams.  This lets us verify that we
    // can read from the clones at different rates while the pipe is being
    // written to.
    for (uint32_t i = 0; i < aNumStreamsToReadPerWrite; ++i) {
      nsCOMPtr<nsIInputStream>& stream = streamList[nextStreamToRead];
      nsCString& outputData = outputDataList[nextStreamToRead];

      // Can't use ConsumeAndValidateStream() here because we're not
      // guaranteed the exact amount read.  It should just be at least
      // as many as numToWrite.
      nsAutoCString tmpOutputData;
      rv = NS_ConsumeStream(stream, UINT32_MAX, tmpOutputData);
      ASSERT_TRUE(rv == NS_BASE_STREAM_WOULD_BLOCK || NS_SUCCEEDED(rv));
      ASSERT_GE(tmpOutputData.Length(), numToWrite);

      outputData += tmpOutputData;

      nextStreamToRead += 1;
      if (nextStreamToRead >= streamList.Length()) {
        // Note: When we wrap around on the streams being read, its possible
        //       we will trigger a segment to be deleted from the pipe.  It
        //       would be nice to validate this here, but we don't have any
        //       QI'able interface that would let us check easily.

        nextStreamToRead = 0;
      }
    }
  }

  rv = writer->Close();
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsDependentCSubstring inputString(inputData.Elements(), inputData.Length());

  // Finally, read the remaining bytes from each stream.  This may be
  // different amounts of data depending on how much reading we did while
  // writing.  Verify that the end result matches the input data.
  for (uint32_t i = 0; i < streamList.Length(); ++i) {
    nsCOMPtr<nsIInputStream>& stream = streamList[i];
    nsCString& outputData = outputDataList[i];

    nsAutoCString tmpOutputData;
    rv = NS_ConsumeStream(stream, UINT32_MAX, tmpOutputData);
    ASSERT_TRUE(rv == NS_BASE_STREAM_WOULD_BLOCK || NS_SUCCEEDED(rv));
    stream->Close();

    // Append to total amount read from the stream
    outputData += tmpOutputData;

    ASSERT_EQ(inputString.Length(), outputData.Length());
    ASSERT_TRUE(inputString.Equals(outputData));
  }
}

} // namespace

TEST(Pipes, Clone_BeforeWrite_ReadAtEnd)
{
  TestPipeClone(32 * 1024, // total bytes
                16,        // num writes
                3,         // num initial clones
                0,         // num streams to close after each write
                0,         // num clones to add after each write
                0);        // num streams to read after each write
}

TEST(Pipes, Clone_BeforeWrite_ReadDuringWrite)
{
  // Since this reads all streams on every write, it should trigger the
  // pipe cursor roll back optimization.  Currently we can only verify
  // this with logging.

  TestPipeClone(32 * 1024, // total bytes
                16,        // num writes
                3,         // num initial clones
                0,         // num streams to close after each write
                0,         // num clones to add after each write
                4);        // num streams to read after each write
}

TEST(Pipes, Clone_DuringWrite_ReadAtEnd)
{
  TestPipeClone(32 * 1024, // total bytes
                16,        // num writes
                0,         // num initial clones
                0,         // num streams to close after each write
                1,         // num clones to add after each write
                0);        // num streams to read after each write
}

TEST(Pipes, Clone_DuringWrite_ReadDuringWrite)
{
  TestPipeClone(32 * 1024, // total bytes
                16,        // num writes
                0,         // num initial clones
                0,         // num streams to close after each write
                1,         // num clones to add after each write
                1);        // num streams to read after each write
}

TEST(Pipes, Clone_DuringWrite_ReadDuringWrite_CloseDuringWrite)
{
  // Since this reads streams faster than we clone new ones, it should
  // trigger pipe segment deletion periodically.  Currently we can
  // only verify this with logging.

  TestPipeClone(32 * 1024, // total bytes
                16,        // num writes
                1,         // num initial clones
                1,         // num streams to close after each write
                2,         // num clones to add after each write
                3);        // num streams to read after each write
}

TEST(Pipes, Write_AsyncWait)
{
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  const uint32_t segmentSize = 1024;
  const uint32_t numSegments = 1;

  nsresult rv = NS_NewPipe2(getter_AddRefs(reader), getter_AddRefs(writer),
                            true, true,  // non-blocking - reader, writer
                            segmentSize, numSegments);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> inputData;
  testing::CreateData(segmentSize, inputData);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_EQ(NS_BASE_STREAM_WOULD_BLOCK, rv);

  RefPtr<testing::OutputStreamCallback> cb =
    new testing::OutputStreamCallback();

  rv = writer->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(cb->Called());

  testing::ConsumeAndValidateStream(reader, inputData);

  ASSERT_TRUE(cb->Called());
}

TEST(Pipes, Write_AsyncWait_Clone)
{
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  const uint32_t segmentSize = 1024;
  const uint32_t numSegments = 1;

  nsresult rv = NS_NewPipe2(getter_AddRefs(reader), getter_AddRefs(writer),
                            true, true,  // non-blocking - reader, writer
                            segmentSize, numSegments);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> clone;
  rv = NS_CloneInputStream(reader, getter_AddRefs(clone));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> inputData;
  testing::CreateData(segmentSize, inputData);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // This attempts to write data beyond the original pipe size limit.  It
  // should fail since neither side of the clone has been read yet.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_EQ(NS_BASE_STREAM_WOULD_BLOCK, rv);

  RefPtr<testing::OutputStreamCallback> cb =
    new testing::OutputStreamCallback();

  rv = writer->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(cb->Called());

  // Consume data on the original stream, but the clone still has not been read.
  testing::ConsumeAndValidateStream(reader, inputData);

  // A clone that is not being read should not stall the other input stream
  // reader.  Therefore the writer callback should trigger when the fastest
  // reader drains the other input stream.
  ASSERT_TRUE(cb->Called());

  // Attempt to write data.  This will buffer data beyond the pipe size limit in
  // order for the clone stream to still work.  This is allowed because the
  // other input stream has drained its buffered segments and is ready for more
  // data.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Again, this should fail since the origin stream has not been read again.
  // The pipe size should still restrict how far ahead we can buffer even
  // when there is a cloned stream not being read.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_FAILED(rv));

  cb = new testing::OutputStreamCallback();
  rv = writer->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // The write should again be blocked since we have written data and the
  // main reader is at its maximum advance buffer.
  ASSERT_FALSE(cb->Called());

  nsTArray<char> expectedCloneData;
  expectedCloneData.AppendElements(inputData);
  expectedCloneData.AppendElements(inputData);

  // We should now be able to consume the entire backlog of buffered data on
  // the cloned stream.
  testing::ConsumeAndValidateStream(clone, expectedCloneData);

  // Draining the clone side should also trigger the AsyncWait() writer
  // callback
  ASSERT_TRUE(cb->Called());

  // Finally, we should be able to consume the remaining data on the original
  // reader.
  testing::ConsumeAndValidateStream(reader, inputData);
}

TEST(Pipes, Write_AsyncWait_Clone_CloseOriginal)
{
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  const uint32_t segmentSize = 1024;
  const uint32_t numSegments = 1;

  nsresult rv = NS_NewPipe2(getter_AddRefs(reader), getter_AddRefs(writer),
                            true, true,  // non-blocking - reader, writer
                            segmentSize, numSegments);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> clone;
  rv = NS_CloneInputStream(reader, getter_AddRefs(clone));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> inputData;
  testing::CreateData(segmentSize, inputData);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // This attempts to write data beyond the original pipe size limit.  It
  // should fail since neither side of the clone has been read yet.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_EQ(NS_BASE_STREAM_WOULD_BLOCK, rv);

  RefPtr<testing::OutputStreamCallback> cb =
    new testing::OutputStreamCallback();

  rv = writer->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(cb->Called());

  // Consume data on the original stream, but the clone still has not been read.
  testing::ConsumeAndValidateStream(reader, inputData);

  // A clone that is not being read should not stall the other input stream
  // reader.  Therefore the writer callback should trigger when the fastest
  // reader drains the other input stream.
  ASSERT_TRUE(cb->Called());

  // Attempt to write data.  This will buffer data beyond the pipe size limit in
  // order for the clone stream to still work.  This is allowed because the
  // other input stream has drained its buffered segments and is ready for more
  // data.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Again, this should fail since the origin stream has not been read again.
  // The pipe size should still restrict how far ahead we can buffer even
  // when there is a cloned stream not being read.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_FAILED(rv));

  cb = new testing::OutputStreamCallback();
  rv = writer->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // The write should again be blocked since we have written data and the
  // main reader is at its maximum advance buffer.
  ASSERT_FALSE(cb->Called());

  // Close the original reader input stream.  This was the fastest reader,
  // so we should have a single stream that is buffered beyond our nominal
  // limit.
  reader->Close();

  // Because the clone stream is still buffered the writable callback should
  // not be fired.
  ASSERT_FALSE(cb->Called());

  // And we should not be able to perform a write.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_FAILED(rv));

  // Create another clone stream.  Now we have two streams that exceed our
  // maximum size limit
  nsCOMPtr<nsIInputStream> clone2;
  rv = NS_CloneInputStream(clone, getter_AddRefs(clone2));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> expectedCloneData;
  expectedCloneData.AppendElements(inputData);
  expectedCloneData.AppendElements(inputData);

  // We should now be able to consume the entire backlog of buffered data on
  // the cloned stream.
  testing::ConsumeAndValidateStream(clone, expectedCloneData);

  // The pipe should now be writable because we have two open streams, one of which
  // is completely drained.
  ASSERT_TRUE(cb->Called());

  // Write again to reach our limit again.
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // The stream is again non-writeable.
  cb = new testing::OutputStreamCallback();
  rv = writer->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_FALSE(cb->Called());

  // Close the empty stream.  This is different from our previous close since
  // before we were closing a stream with some data still buffered.
  clone->Close();

  // The pipe should not be writable.  The second clone is still fully buffered
  // over our limit.
  ASSERT_FALSE(cb->Called());
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_FAILED(rv));

  // Finally consume all of the buffered data on the second clone.
  expectedCloneData.AppendElements(inputData);
  testing::ConsumeAndValidateStream(clone2, expectedCloneData);

  // Draining the final clone should make the pipe writable again.
  ASSERT_TRUE(cb->Called());
}

TEST(Pipes, Read_AsyncWait)
{
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  const uint32_t segmentSize = 1024;
  const uint32_t numSegments = 1;

  nsresult rv = NS_NewPipe2(getter_AddRefs(reader), getter_AddRefs(writer),
                            true, true,  // non-blocking - reader, writer
                            segmentSize, numSegments);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> inputData;
  testing::CreateData(segmentSize, inputData);

  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();

  rv = reader->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(cb->Called());

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_TRUE(cb->Called());

  testing::ConsumeAndValidateStream(reader, inputData);
}

TEST(Pipes, Read_AsyncWait_Clone)
{
  nsCOMPtr<nsIAsyncInputStream> reader;
  nsCOMPtr<nsIAsyncOutputStream> writer;

  const uint32_t segmentSize = 1024;
  const uint32_t numSegments = 1;

  nsresult rv = NS_NewPipe2(getter_AddRefs(reader), getter_AddRefs(writer),
                            true, true,  // non-blocking - reader, writer
                            segmentSize, numSegments);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIInputStream> clone;
  rv = NS_CloneInputStream(reader, getter_AddRefs(clone));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIAsyncInputStream> asyncClone = do_QueryInterface(clone);
  ASSERT_TRUE(asyncClone);

  nsTArray<char> inputData;
  testing::CreateData(segmentSize, inputData);

  RefPtr<testing::InputStreamCallback> cb =
    new testing::InputStreamCallback();

  RefPtr<testing::InputStreamCallback> cb2 =
    new testing::InputStreamCallback();

  rv = reader->AsyncWait(cb, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(cb->Called());

  rv = asyncClone->AsyncWait(cb2, 0, 0, nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(cb2->Called());

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_TRUE(cb->Called());
  ASSERT_TRUE(cb2->Called());

  testing::ConsumeAndValidateStream(reader, inputData);
}

namespace {

nsresult
CloseDuringReadFunc(nsIInputStream *aReader,
                    void* aClosure,
                    const char* aFromSegment,
                    uint32_t aToOffset,
                    uint32_t aCount,
                    uint32_t* aWriteCountOut)
{
  MOZ_RELEASE_ASSERT(aReader);
  MOZ_RELEASE_ASSERT(aClosure);
  MOZ_RELEASE_ASSERT(aFromSegment);
  MOZ_RELEASE_ASSERT(aWriteCountOut);
  MOZ_RELEASE_ASSERT(aToOffset == 0);

  // This is insanity and you probably should not do this under normal
  // conditions.  We want to simulate the case where the pipe is closed
  // (possibly from other end on another thread) simultaneously with the
  // read.  This is the easiest way to do trigger this case in a synchronous
  // gtest.
  MOZ_ALWAYS_SUCCEEDS(aReader->Close());

  nsTArray<char>* buffer = static_cast<nsTArray<char>*>(aClosure);
  buffer->AppendElements(aFromSegment, aCount);

  *aWriteCountOut = aCount;

  return NS_OK;
}

void
TestCloseDuringRead(uint32_t aSegmentSize, uint32_t aDataSize)
{
  nsCOMPtr<nsIInputStream> reader;
  nsCOMPtr<nsIOutputStream> writer;

  const uint32_t maxSize = aSegmentSize;

  nsresult rv = NS_NewPipe(getter_AddRefs(reader), getter_AddRefs(writer),
                           aSegmentSize, maxSize);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> inputData;

  testing::CreateData(aDataSize, inputData);

  uint32_t numWritten = 0;
  rv = writer->Write(inputData.Elements(), inputData.Length(), &numWritten);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<char> outputData;

  uint32_t numRead = 0;
  rv = reader->ReadSegments(CloseDuringReadFunc, &outputData,
                            inputData.Length(), &numRead);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ(inputData.Length(), numRead);

  ASSERT_EQ(inputData, outputData);

  uint64_t available;
  rv = reader->Available(&available);
  ASSERT_EQ(NS_BASE_STREAM_CLOSED, rv);
}

} // namespace

TEST(Pipes, Close_During_Read_Partial_Segment)
{
  TestCloseDuringRead(1024, 512);
}

TEST(Pipes, Close_During_Read_Full_Segment)
{
  TestCloseDuringRead(1024, 1024);
}

TEST(Pipes, Interfaces)
{
  nsCOMPtr<nsIInputStream> reader;
  nsCOMPtr<nsIOutputStream> writer;

  nsresult rv = NS_NewPipe(getter_AddRefs(reader), getter_AddRefs(writer));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIAsyncInputStream> readerType1 = do_QueryInterface(reader);
  ASSERT_TRUE(readerType1);

  nsCOMPtr<nsISeekableStream> readerType2 = do_QueryInterface(reader);
  ASSERT_TRUE(readerType2);

  nsCOMPtr<nsISearchableInputStream> readerType3 = do_QueryInterface(reader);
  ASSERT_TRUE(readerType3);

  nsCOMPtr<nsICloneableInputStream> readerType4 = do_QueryInterface(reader);
  ASSERT_TRUE(readerType4);

  nsCOMPtr<nsIClassInfo> readerType5 = do_QueryInterface(reader);
  ASSERT_TRUE(readerType5);

  nsCOMPtr<nsIBufferedInputStream> readerType6 = do_QueryInterface(reader);
  ASSERT_TRUE(readerType6);
}
