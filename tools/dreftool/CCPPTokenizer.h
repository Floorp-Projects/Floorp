#ifndef _CTOKENIZER
#define _CTOKENIZER

#include "sharedtypes.h"
#include <typeinfo.h>
#include "CScanner.h"
#include "nsDeque.h"

class CToken;

class CCPPTokenizer {
public:
              CCPPTokenizer();
              ~CCPPTokenizer();

  virtual int32 tokenize(CScanner& aScanner);

          CToken* getTokenAt(int anIndex);
          int32   getCount() {return mTokens.GetSize();}
protected:
  nsDeque  mTokens;
};

#endif 


