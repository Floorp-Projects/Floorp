#include "XZStream.h"

#include <algorithm>
#include <cstring>
#include "mozilla/Assertions.h"
#include "Logging.h"

// LZMA dictionary size, should have a minimum size for the given compression
// rate, see XZ Utils docs for details.
static const uint32_t kDictSize = 1 << 24;

static const size_t kFooterSize = 12;

// Parses a variable-length integer (VLI),
// see http://tukaani.org/xz/xz-file-format.txt for details.
static size_t
ParseVarLenInt(const uint8_t* aBuf, size_t aBufSize, uint64_t* aValue)
{
  if (!aBufSize) {
    return 0;
  }
  aBufSize = std::min(size_t(9), aBufSize);

  *aValue = aBuf[0] & 0x7F;
  size_t i = 0;

  while (aBuf[i++] & 0x80) {
    if (i >= aBufSize || aBuf[i] == 0x0) {
      return 0;
    }
    *aValue |= static_cast<uint64_t>(aBuf[i] & 0x7F) << (i * 7);
  }
  return i;
}

/* static */ bool
XZStream::IsXZ(const void* aBuf, size_t aBufSize)
{
  static const uint8_t kXzMagic[] = {0xfd, '7', 'z', 'X', 'Z', 0x0};
  MOZ_ASSERT(aBuf);
  return aBufSize > sizeof(kXzMagic) &&
         !memcmp(reinterpret_cast<const void*>(kXzMagic), aBuf, sizeof(kXzMagic));
}

XZStream::XZStream(const void* aInBuf, size_t aInSize)
  : mInBuf(static_cast<const uint8_t*>(aInBuf))
  , mUncompSize(0)
  , mDec(nullptr)
{
  mBuffers.in = mInBuf;
  mBuffers.in_pos = 0;
  mBuffers.in_size = aInSize;
}

XZStream::~XZStream()
{
  xz_dec_end(mDec);
}

bool
XZStream::Init()
{
#ifdef XZ_USE_CRC64
  xz_crc64_init();
#endif
  xz_crc32_init();

  mDec = xz_dec_init(XZ_DYNALLOC, kDictSize);

  if (!mDec) {
    return false;
  }

  mUncompSize = ParseUncompressedSize();

  return true;
}

size_t
XZStream::Decode(void* aOutBuf, size_t aOutSize)
{
  if (!mDec) {
    return 0;
  }

  mBuffers.out = static_cast<uint8_t*>(aOutBuf);
  mBuffers.out_pos = 0;
  mBuffers.out_size = aOutSize;

  while (mBuffers.in_pos < mBuffers.in_size &&
         mBuffers.out_pos < mBuffers.out_size) {
    const xz_ret ret = xz_dec_run(mDec, &mBuffers);

    switch (ret) {
      case XZ_STREAM_END:
        // Stream ended, the next loop iteration should terminate.
        MOZ_ASSERT(mBuffers.in_pos == mBuffers.in_size);
        MOZ_FALLTHROUGH;
#ifdef XZ_DEC_ANY_CHECK
      case XZ_UNSUPPORTED_CHECK:
        // Ignore unsupported check.
        MOZ_FALLTHROUGH;
#endif
      case XZ_OK:
        // Chunk decoded, proceed.
        break;

      case XZ_MEM_ERROR:
        ERROR("XZ decoding: memory allocation failed");
        return 0;

      case XZ_MEMLIMIT_ERROR:
        ERROR("XZ decoding: memory usage limit reached");
        return 0;

      case XZ_FORMAT_ERROR:
        ERROR("XZ decoding: invalid stream format");
        return 0;

      case XZ_OPTIONS_ERROR:
        ERROR("XZ decoding: unsupported header options");
        return 0;

      case XZ_DATA_ERROR:
        MOZ_FALLTHROUGH;
      case XZ_BUF_ERROR:
        ERROR("XZ decoding: corrupt input stream");
        return 0;

      default:
        MOZ_ASSERT_UNREACHABLE("XZ decoding: unknown error condition");
        return 0;
    }
  }
  return mBuffers.out_pos;
}

size_t
XZStream::RemainingInput() const
{
  return mBuffers.in_size - mBuffers.in_pos;
}

size_t
XZStream::Size() const
{
  return mBuffers.in_size;
}

size_t
XZStream::UncompressedSize() const
{
  return mUncompSize;
}

size_t
XZStream::ParseIndexSize() const
{
  static const uint8_t kFooterMagic[] = {'Y', 'Z'};

  const uint8_t* footer = mInBuf + mBuffers.in_size - kFooterSize;
  // The magic bytes are at the end of the footer.
  if (memcmp(reinterpret_cast<const void*>(kFooterMagic),
             footer + kFooterSize - sizeof(kFooterMagic),
             sizeof(kFooterMagic))) {
    // Not a valid footer at stream end.
    return 0;
  }
  // Backward size is a 32 bit LE integer field positioned after the 32 bit CRC32
  // code. It encodes the index size as a multiple of 4 bytes with a minimum
  // size of 4 bytes.
  const uint32_t backwardSize = *(footer + 4);
  return (backwardSize + 1) * 4;
}

size_t
XZStream::ParseUncompressedSize() const
{
  static const uint8_t kIndexIndicator[] = {0x0};

  const size_t indexSize = ParseIndexSize();
  if (!indexSize) {
    return 0;
  }
  // The footer follows directly the index, so we can use it as a reference.
  const uint8_t* end = mInBuf + mBuffers.in_size;
  const uint8_t* index = end - kFooterSize - indexSize;

  // The index consists of a one byte indicator followed by a VLI field for the
  // number of records (1 expected) followed by a list of records. One record
  // contains a VLI field for unpadded size followed by a VLI field for
  // uncompressed size.
  if (memcmp(reinterpret_cast<const void*>(kIndexIndicator),
             index, sizeof(kIndexIndicator))) {
    // Not a valid index.
    return 0;
  }

  index += sizeof(kIndexIndicator);
  uint64_t numRecords = 0;
  index += ParseVarLenInt(index, end - index, &numRecords);
  if (!numRecords) {
    return 0;
  }
  uint64_t unpaddedSize = 0;
  index += ParseVarLenInt(index, end - index, &unpaddedSize);
  if (!unpaddedSize) {
    return 0;
  }
  uint64_t uncompressedSize = 0;
  index += ParseVarLenInt(index, end - index, &uncompressedSize);

  return uncompressedSize;
}
