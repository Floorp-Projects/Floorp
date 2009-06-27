#ifndef nsHtml5NamedCharacters_h__
#define nsHtml5NamedCharacters_h__

#include "prtypes.h"
#include "jArray.h"
#include "nscore.h"

class nsHtml5NamedCharacters
{
  public:
    static jArray<jArray<PRUnichar,PRInt32>,PRInt32> NAMES;
    static jArray<PRUnichar,PRInt32>* VALUES;
    static PRUnichar** WINDOWS_1252;
    static void initializeStatics();
    static void releaseStatics();
};

#endif // nsHtml5NamedCharacters_h__
