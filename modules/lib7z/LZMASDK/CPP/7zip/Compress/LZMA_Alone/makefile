PROG = lzma.exe
MY_CONSOLE = 1
CFLAGS = $(CFLAGS) \
  -DCOMPRESS_MF_MT \
  -DBENCH_MT \

LZMA_OBJS = \
  $O\LzmaAlone.obj \
  $O\LzmaBench.obj \
  $O\LzmaBenchCon.obj \

LZMA_OPT_OBJS = \
  $O\LzmaDecoder.obj \
  $O\LzmaEncoder.obj \

COMMON_OBJS = \
  $O\CommandLineParser.obj \
  $O\CRC.obj \
  $O\IntToString.obj \
  $O\MyString.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\MyVector.obj

WIN_OBJS = \
  $O\System.obj

7ZIP_COMMON_OBJS = \
  $O\CWrappers.obj \
  $O\InBuffer.obj \
  $O\OutBuffer.obj \
  $O\StreamUtils.obj \

C_OBJS = \
  $O\7zCrc.obj \
  $O\Alloc.obj \
  $O\Bra86.obj \
  $O\LzFind.obj \
  $O\LzFindMt.obj \
  $O\Lzma86Dec.obj \
  $O\Lzma86Enc.obj \
  $O\LzmaDec.obj \
  $O\LzmaEnc.obj \
  $O\Threads.obj \

OBJS = \
  $O\StdAfx.obj \
  $(LZMA_OBJS) \
  $(LZMA_OPT_OBJS) \
  $(COMMON_OBJS) \
  $(WIN_OBJS) \
  $(7ZIP_COMMON_OBJS) \
  $(C_OBJS) \
  $O\FileStreams.obj \
  $O\FileIO.obj \

!include "../../../Build.mak"


$(LZMA_OBJS): $(*B).cpp
	$(COMPL)
$(LZMA_OPT_OBJS): ../$(*B).cpp
	$(COMPL_O2)
$(COMMON_OBJS): ../../../Common/$(*B).cpp
	$(COMPL)
$(WIN_OBJS): ../../../Windows/$(*B).cpp
	$(COMPL)
$(7ZIP_COMMON_OBJS): ../../Common/$(*B).cpp
	$(COMPL)
$O\FileStreams.obj: ../../Common/FileStreams.cpp
	$(COMPL)
$O\FileIO.obj: ../../../Windows/FileIO.cpp
	$(COMPL)
$(C_OBJS): ../../../../C/$(*B).c
	$(COMPL_O2)
