PROG = 7z.exe
MY_CONSOLE = 1
CFLAGS = $(CFLAGS) -I ../../../  \
  -DCOMPRESS_MT \
  -DEXTERNAL_LZMA \
  -DEXTERNAL_CODECS \
  -DBREAK_HANDLER \
  -DBENCH_MT \

!IFNDEF UNDER_CE
CFLAGS = $(CFLAGS) -DWIN_LONG_PATH -D_7ZIP_LARGE_PAGES -DSUPPORT_DEVICE_FILE
!ENDIF


CONSOLE_OBJS = \
  $O\ConsoleClose.obj \
  $O\ExtractCallbackConsole.obj \
  $O\List.obj \
  $O\Main.obj \
  $O\MainAr.obj \
  $O\OpenCallbackConsole.obj \
  $O\PercentPrinter.obj \
  $O\UpdateCallbackConsole.obj \
  $O\UserInputUtils.obj \

COMMON_OBJS = \
  $O\CommandLineParser.obj \
  $O\CRC.obj \
  $O\IntToString.obj \
  $O\ListFileUtils.obj \
  $O\NewHandler.obj \
  $O\StdInStream.obj \
  $O\StdOutStream.obj \
  $O\MyString.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\UTFConvert.obj \
  $O\MyVector.obj \
  $O\Wildcard.obj \

WIN_OBJS = \
  $O\DLL.obj \
  $O\Error.obj \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\FileName.obj \
  $O\MemoryLock.obj \
  $O\PropVariant.obj \
  $O\PropVariantConversions.obj \
  $O\Registry.obj \
  $O\System.obj \
  $O\Time.obj \

7ZIP_COMMON_OBJS = \
  $O\FilePathAutoRename.obj \
  $O\FileStreams.obj \
  $O\ProgressUtils.obj \
  $O\StreamUtils.obj \

UI_COMMON_OBJS = \
  $O\ArchiveCommandLine.obj \
  $O\ArchiveExtractCallback.obj \
  $O\ArchiveOpenCallback.obj \
  $O\DefaultName.obj \
  $O\EnumDirItems.obj \
  $O\Extract.obj \
  $O\ExtractingFilePath.obj \
  $O\LoadCodecs.obj \
  $O\OpenArchive.obj \
  $O\PropIDUtils.obj \
  $O\SetProperties.obj \
  $O\SortUtils.obj \
  $O\TempFiles.obj \
  $O\Update.obj \
  $O\UpdateAction.obj \
  $O\UpdateCallback.obj \
  $O\UpdatePair.obj \
  $O\UpdateProduce.obj \
  $O\WorkDir.obj \

AR_COMMON_OBJS = \
  $O\OutStreamWithCRC.obj \

LZMA_BENCH_OBJS = \
  $O\LzmaBench.obj \
  $O\LzmaBenchCon.obj \

C_OBJS = \
  $O\Alloc.obj \
  $O\Threads.obj \

!include "../../Crc2.mak"

OBJS = \
  $O\StdAfx.obj \
  $(CONSOLE_OBJS) \
  $(COMMON_OBJS) \
  $(WIN_OBJS) \
  $(7ZIP_COMMON_OBJS) \
  $(UI_COMMON_OBJS) \
  $(AR_COMMON_OBJS) \
  $O\CopyCoder.obj \
  $(LZMA_BENCH_OBJS) \
  $(C_OBJS) \
  $(CRC_OBJS) \
  $O\resource.res

!include "../../../Build.mak"

$(CONSOLE_OBJS): $(*B).cpp
	$(COMPL)
$(COMMON_OBJS): ../../../Common/$(*B).cpp
	$(COMPL)
$(WIN_OBJS): ../../../Windows/$(*B).cpp
	$(COMPL)
$(7ZIP_COMMON_OBJS): ../../Common/$(*B).cpp
	$(COMPL)
$(UI_COMMON_OBJS): ../Common/$(*B).cpp
	$(COMPL)
$(AR_COMMON_OBJS): ../../Archive/Common/$(*B).cpp
	$(COMPL)
$O\CopyCoder.obj: ../../Compress/$(*B).cpp
	$(COMPL)
$(LZMA_BENCH_OBJS): ../../Compress/LZMA_Alone/$(*B).cpp
	$(COMPL)
$(C_OBJS): ../../../../C/$(*B).c
	$(COMPL_O2)
!include "../../Crc.mak"
