// PercentPrinter.h

#ifndef __PERCENTPRINTER_H
#define __PERCENTPRINTER_H

#include "Common/Types.h"
#include "Common/StdOutStream.h"

class CPercentPrinter
{
  UInt64 m_MinStepSize;
  UInt64 m_PrevValue;
  UInt64 m_CurValue;
  UInt64 m_Total;
  int m_NumExtraChars;
public:
  CStdOutStream *OutStream;

  CPercentPrinter(UInt64 minStepSize = 1): m_MinStepSize(minStepSize),
      m_PrevValue(0), m_CurValue(0), m_Total(1), m_NumExtraChars(0) {}
  void SetTotal(UInt64 total) { m_Total = total; m_PrevValue = 0; }
  void SetRatio(UInt64 doneValue) { m_CurValue = doneValue; }
  void PrintString(const char *s);
  void PrintString(const wchar_t *s);
  void PrintNewLine();
  void ClosePrint();
  void RePrintRatio();
  void PrintRatio();
};

#endif
