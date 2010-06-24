#ifndef nsHtml5NamedCharacters_h_
#define nsHtml5NamedCharacters_h_

#include "prtypes.h"
#include "jArray.h"
#include "nscore.h"
#include "nsDebug.h"
#include "prlog.h"
#include "nsMemory.h"

class nsHtml5NamedCharacters
{
  public:
    static jArray<jArray<PRInt8,PRInt32>,PRInt32> NAMES;
    static const PRUnichar VALUES[][2];
    static PRUnichar** WINDOWS_1252;
    static void initializeStatics();
    static void releaseStatics();
};

#endif // nsHtml5NamedCharacters_h_
