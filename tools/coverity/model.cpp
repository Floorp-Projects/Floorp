/*
Coverity model file in order to avoid false-positive
*/

#define NULL (void *)0

typedef unsigned char   jsbytecode;
typedef unsigned short  uint16_t;
typedef unsigned int    uint32_t;
typedef unsigned int    int32_t;
typedef unsigned char   uint8_t;

static const uint16_t CHUNK_HEAD_SIZE = 8;

void assert(bool expr) {
  if (!expr) {
    __coverity_panic__();
  }
}

#define ERREXIT(cinfo, err) __coverity_panic__();

void  MOZ_ASSUME_UNREACHABLE(char * str) {
  __coverity_panic__();
}

static void MOZ_ReportAssertionFailure(const char* aStr, const char* aFilename,
    int aLine) {
  __coverity_panic__();
}

static void MOZ_ReportCrash(const char* aStr, const char* aFilename,
    int aLine) {
  __coverity_panic__();
}

#define MOZ_ASSERT(expr, msg) assert(!!(expr))

#define MOZ_ASSERT(expr) assert(!!(expr))

#define NS_ASSERTION(expr, msg) assert(!!(expr))

#define PORT_Assert(expr) assert(!!(expr))

#define PR_ASSERT(expr) assert(!!(expr))

#define NS_RUNTIMEABORT(msg) __coverity_panic__()

int GET_JUMP_OFFSET(jsbytecode* pc) {
  __coverity_tainted_data_sanitize__(&pc[1]);
  __coverity_tainted_data_sanitize__(&pc[2]);
  __coverity_tainted_data_sanitize__(&pc[3]);
  __coverity_tainted_data_sanitize__(&pc[4]);

  return 0;
}


// Data sanity checkers
#define XPT_SWAB16(data) __coverity_tainted_data_sanitize__(&data)

#define XPT_SWAB32(data) __coverity_tainted_data_sanitize__(&data)


static unsigned GET_UINT24(const jsbytecode* pc) {
  __coverity_tainted_data_sanitize__(static_cast<void*>(pc));
  //return unsigned((pc[1] << 16) | (pc[2] << 8) | pc[3]);
  return 0;
}


class HeaderParser {

private:
  class ChunkHeader {

    uint8_t mRaw[CHUNK_HEAD_SIZE];

    HeaderParser::ChunkHeader::ChunkSize() const {
      __coverity_tainted_data_sanitize__(static_cast<void*>(&mRaw[4]));
      __coverity_tainted_data_sanitize__(static_cast<void*>(&mRaw[5]));
      __coverity_tainted_data_sanitize__(static_cast<void*>(&mRaw[6]));
      __coverity_tainted_data_sanitize__(static_cast<void*>(&mRaw[7]));

      return ((mRaw[7] << 24) | (mRaw[6] << 16) | (mRaw[5] << 8) | (mRaw[4]));
    }
  };
};

void NS_DebugBreak(uint32_t aSeverity, const char* aStr, const char* aExpr,
    const char* aFile, int32_t aLine) {
  __coverity_panic__();
}

static inline void Swap(uint32_t* value) {
  __coverity_tainted_data_sanitize__(static_cast<void*>(&value));
  *value =  (*value >> 24) |
           ((*value >> 8)  & 0x0000ff00) |
           ((*value << 8)  & 0x00ff0000) |
            (*value << 24);
}

static uint32_t xtolong (const uint8_t *ll) {
  __coverity_tainted_data_sanitize__(static_cast<void*>(&ll[0]));
  __coverity_tainted_data_sanitize__(static_cast<void*>(&ll[1]));
  __coverity_tainted_data_sanitize__(static_cast<void*>(&ll[2]));
  __coverity_tainted_data_sanitize__(static_cast<void*>(&ll[3]));

  return (uint32_t)( (ll [0] <<  0) |
                     (ll [1] <<  8) |
                     (ll [2] << 16) |
                     (ll [3] << 24) );
}

class ByteReader {
public:
  const uint8_t* Read(size_t aCount);
  uint32_t ReadU24() {
    const uint8_t *ptr = Read(3);
    if (!ptr) {
      MOZ_ASSERT(false);
      return 0;
    }
    __coverity_tainted_data_sanitize__(static_cast<void*>(&ptr[0]));
    __coverity_tainted_data_sanitize__(static_cast<void*>(&ptr[1]));
    __coverity_tainted_data_sanitize__(static_cast<void*>(&ptr[2]));
    return ptr[0] << 16 | ptr[1] << 8 | ptr[2];
  }
};

