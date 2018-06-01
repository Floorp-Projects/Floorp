#include "HashStore.h"
#include "nsPrintfCString.h"
#include "string.h"
#include "gtest/gtest.h"
#include "mozilla/Unused.h"

using namespace mozilla;
using namespace mozilla::safebrowsing;

static const char* kFilesInV2[] = {".pset", ".sbstore"};
static const char* kFilesInV4[] = {".pset", ".metadata"};

#define V2_TABLE  "gtest-malware-simple"
#define V4_TABLE1 "goog-malware-proto"
#define V4_TABLE2 "goog-phish-proto"

#define ROOT_DIR        NS_LITERAL_STRING("safebrowsing")
#define SB_FILE(x, y)   NS_ConvertUTF8toUTF16(nsPrintfCString("%s%s",x, y))

template<typename T, size_t N>
void CheckFileExist(const char* table, const T (&files)[N], bool expectExists)
{
  for (uint32_t i = 0; i < N; i++) {
    // This is just a quick way to know if this is v4 table
    NS_ConvertUTF8toUTF16 SUB_DIR(strstr(table, "-proto") ? "google4" : "");
    nsCOMPtr<nsIFile> file =
      GetFile(nsTArray<nsString> { ROOT_DIR, SUB_DIR, SB_FILE(table, files[i]) });

    bool exists;
    file->Exists(&exists);

    ASSERT_EQ(expectExists, exists) << file->HumanReadablePath().get();
  }
}

TEST(UrlClassifierFailUpdate, CheckTableReset)
{
  const bool FULL_UPDATE = true;
  const bool PARTIAL_UPDATE = false;

  // Apply V2 update
  {
    RefPtr<TableUpdateV2> update = new TableUpdateV2(NS_LITERAL_CSTRING(V2_TABLE));
    Unused << update->NewAddChunk(1);

    ApplyUpdate(update);

    // A successful V2 update should create .pset & .sbstore files
    CheckFileExist(V2_TABLE, kFilesInV2, true);
  }

  // Helper function to generate table update data
  auto func = [](RefPtr<TableUpdateV4> update, bool full, const char* str) {
    update->SetFullUpdate(full);
    nsCString prefix(str);
    update->NewPrefixes(prefix.Length(), prefix);
  };

  // Apply V4 update for table1
  {
    RefPtr<TableUpdateV4> update = new TableUpdateV4(NS_LITERAL_CSTRING(V4_TABLE1));
    func(update, FULL_UPDATE, "test_prefix");

    ApplyUpdate(update);

    // A successful V4 update should create .pset & .metadata files
    CheckFileExist(V4_TABLE1, kFilesInV4, true);
  }

  // Apply V4 update for table2
  {
    RefPtr<TableUpdateV4> update = new TableUpdateV4(NS_LITERAL_CSTRING(V4_TABLE2));
    func(update, FULL_UPDATE, "test_prefix");

    ApplyUpdate(update);

    CheckFileExist(V4_TABLE2, kFilesInV4, true);
  }

  // Apply V4 update with the same prefix in previous full udpate
  // This should cause an update error.
  {
    RefPtr<TableUpdateV4> update = new TableUpdateV4(NS_LITERAL_CSTRING(V4_TABLE1));
    func(update, PARTIAL_UPDATE, "test_prefix");

    ApplyUpdate(update);

    // A fail update should remove files for that table
    CheckFileExist(V4_TABLE1, kFilesInV4, false);

    // A fail update should NOT remove files for the other tables
    CheckFileExist(V2_TABLE, kFilesInV2, true);
    CheckFileExist(V4_TABLE2, kFilesInV4, true);
  }
}
