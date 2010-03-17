#ifndef nsHtml5NamedCharacters_h__
#define nsHtml5NamedCharacters_h__

#include "prtypes.h"
#include "jArray.h"
#include "nscore.h"

class nsHtml5NamedCharacters
{
  public:
    static jArray<jArray<PRInt8,PRInt32>,PRInt32> NAMES;
    static const PRUnichar VALUES[][2];
    static PRUnichar** WINDOWS_1252;
    static const PRInt32* const HILO_ACCEL[];
    static void initializeStatics();
    static void releaseStatics();
};

#endif // nsHtml5NamedCharacters_h__
