
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // for debugging only

#include <string>
#include <map>
#include "common/unique_string.h"

///////////////////////////////////////////////////////////////////
// UniqueString
//
class UniqueString {
 public:
  UniqueString(string str) { str_ = strdup(str.c_str()); }
  ~UniqueString() { free((void*)str_); }
  const char* str_;
};

class UniqueStringUniverse {
 public:
  UniqueStringUniverse() {};
  const UniqueString* findOrCopy(string str)
  {
    std::map<string, UniqueString*>::iterator it;
    it = map_.find(str);
    if (it == map_.end()) {
      UniqueString* ustr = new UniqueString(str);
      map_[str] = ustr;
      fprintf(stderr, "UniqueString %d = \"%s\"\n",
              (int)map_.size(), str.c_str());
      return ustr;
    } else {
      return it->second;
    }
  }
 private:
  std::map<string, UniqueString*> map_;
};
//
///////////////////////////////////////////////////////////////////


static UniqueStringUniverse* sUSU = NULL;


// This isn't threadsafe.
const UniqueString* toUniqueString(string str)
{
  if (!sUSU) {
    sUSU = new UniqueStringUniverse();
  }
  return sUSU->findOrCopy(str);
}

// This isn't threadsafe.
const UniqueString* toUniqueString_n(char* str, size_t n)
{
  if (!sUSU) {
    sUSU = new UniqueStringUniverse();
  }
  string key(str);
  key.resize(n);
  return sUSU->findOrCopy(key);
}

const char index(const UniqueString* us, int ix)
{
  return us->str_[ix];
}

const char* const fromUniqueString(const UniqueString* ustr)
{
  return ustr->str_;
}
