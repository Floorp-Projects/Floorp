# This makefile contains a list of 7-zip files required for extraction only

7ZIPCPPSRCS = \
	7zDecode.cpp \
	7zExtract.cpp \
	7zFolderOutStream.cpp \
	7zHandler.cpp \
	7zHeader.cpp \
	7zIn.cpp \
	ArchiveExports.cpp \
	CoderMixer2.cpp \
	CoderMixer2MT.cpp \
	CrossThreadProgress.cpp \
	ItemNameUtils.cpp \
	OutStreamWithCRC.cpp \
	CreateCoder.cpp \
	FilePathAutoRename.cpp \
	FileStreams.cpp \
	FilterCoder.cpp \
	InBuffer.cpp \
	LimitedStreams.cpp \
	LockedStream.cpp \
	OutBuffer.cpp \
	ProgressUtils.cpp \
	StreamBinder.cpp \
	StreamObjects.cpp \
	StreamUtils.cpp \
	VirtThread.cpp \
	Bcj2Coder.cpp \
	BcjCoder.cpp \
	BranchCoder.cpp \
	CopyCoder.cpp \
	Lzma2Decoder.cpp \
	LzmaDecoder.cpp \
	DefaultName.cpp \
	ExtractingFilePath.cpp \
	IntToString.cpp \
	MyString.cpp \
	MyVector.cpp \
	NewHandler.cpp \
	StringConvert.cpp \
	Wildcard.cpp \
	DLL.cpp \
	Error.cpp \
	FileDir.cpp \
	FileFind.cpp \
	FileIO.cpp \
	FileName.cpp \
	PropVariant.cpp \
	PropVariantConversions.cpp \
	Synchronization.cpp \
	$(NULL)

7ZIPCSRCS = \
	7zCrc.c \
	Alloc.c \
	Bra86.c \
	Lzma2Dec.c \
	LzmaDec.c \
	Sha256.c \
	Threads.c \
	$(NULL)

vpath %.cpp $(7ZIPSRCDIR)/CPP/7zip/Archive
vpath %.cpp $(7ZIPSRCDIR)/CPP/7zip/Archive/7z
vpath %.cpp $(7ZIPSRCDIR)/CPP/7zip/Archive/Common
vpath %.cpp $(7ZIPSRCDIR)/CPP/7zip/Common
vpath %.cpp $(7ZIPSRCDIR)/CPP/7zip/Compress
vpath %.cpp $(7ZIPSRCDIR)/CPP/7zip/UI/Common
vpath %.cpp $(7ZIPSRCDIR)/CPP/Common
vpath %.cpp $(7ZIPSRCDIR)/CPP/Windows
vpath %.c $(7ZIPSRCDIR)/C
