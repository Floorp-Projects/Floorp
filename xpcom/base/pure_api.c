/*
 * Header file of Pure API function declarations.
 *
 * Explicitly no copyright.
 * You may recompile and redistribute these definitions as required.
 *
 * NOTE1: In some situations when compiling with MFC, you should
 *        enable the setting 'Not using precompiled headers' in Visual C++
 *        to avoid a compiler diagnostic.
 *
 * NOTE2: This file works through the use of deep magic.  Calls to functions
 *        in this file are replaced with calls into the OCI runtime system
 *        when an instrumented version of this program is run.
 *
 * NOTE3: The static vars avoidGy_n (where n is a unique number) are used
 *        to prevent optimizing the functions away when compiler option
 *        /Gy is set. This is needed so that NOTE2 works properly.
 */
static int avoidGy_1;
static int avoidGy_2;
static int avoidGy_3;
static int avoidGy_4;
static int avoidGy_5;
static int avoidGy_6;
static int avoidGy_7;
static int avoidGy_8;
static int avoidGy_9;
static int avoidGy_10;
static int avoidGy_11;
static int avoidGy_12;
static int avoidGy_13;
static int avoidGy_14;
static int avoidGy_15;
static int avoidGy_16;
static int avoidGy_17;
static int avoidGy_18;
static int avoidGy_19;
static int avoidGy_20;
static int avoidGy_21;
static int avoidGy_22;
static int avoidGy_23;
static int avoidGy_24;
static int avoidGy_25;
static int avoidGy_26;
static int avoidGy_27;
static int avoidGy_28;
static int avoidGy_29;
static int avoidGy_30;
static int avoidGy_31;
static int avoidGy_32;
static int avoidGy_33;
static int avoidGy_34;
static int avoidGy_35;
static int avoidGy_36;
static int avoidGy_37;
static int avoidGy_38;
static int avoidGy_39;
static int avoidGy_40;
static int avoidGy_41;
static int avoidGy_42;
static int avoidGy_43;
static int avoidGy_44;
static int avoidGy_45;
static int avoidGy_46;
static int avoidGy_47;
static int avoidGy_48;
static int avoidGy_49;
static int avoidGy_50;
static int avoidGy_51;
static int avoidGy_52;
static int avoidGy_53;
static int avoidGy_54;
static int avoidGy_55;
static int avoidGy_56;
static int avoidGy_57;
static int avoidGy_58;
static int avoidGy_59;
static int avoidGy_60;
static int avoidGy_PL_01;
__declspec(dllexport) int __cdecl PurePrintf(const char *fmt, ...) { avoidGy_1++; fmt; return 0; }
__declspec(dllexport) int __cdecl PurifyIsRunning(void) { avoidGy_2++; return 0; }
__declspec(dllexport) int __cdecl PurifyPrintf(const char *fmt, ...) { avoidGy_3++; fmt; return 0; }
__declspec(dllexport) int __cdecl PurifyNewInuse(void) { avoidGy_4++; return 0; }
__declspec(dllexport) int __cdecl PurifyAllInuse(void) { avoidGy_5++; return 0; }
__declspec(dllexport) int __cdecl PurifyClearInuse(void) { avoidGy_6++; return 0; }
__declspec(dllexport) int __cdecl PurifyNewLeaks(void) { avoidGy_7++; return 0; }
__declspec(dllexport) int __cdecl PurifyAllLeaks(void) { avoidGy_8++; return 0; }
__declspec(dllexport) int __cdecl PurifyClearLeaks(void) { avoidGy_9++; return 0; }
__declspec(dllexport) int __cdecl PurifyAllHandlesInuse(void) { avoidGy_10++; return 0; }
__declspec(dllexport) int __cdecl PurifyNewHandlesInuse(void) { avoidGy_11++; return 0; }
__declspec(dllexport) int __cdecl PurifyDescribe(void *addr) { avoidGy_12++; addr; return 0; }
__declspec(dllexport) int __cdecl PurifyWhatColors(void *addr, int size) { avoidGy_13++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyAssertIsReadable(const void *addr, int size) { avoidGy_14++; addr; size; return 1; }
__declspec(dllexport) int __cdecl PurifyAssertIsWritable(const void *addr, int size) { avoidGy_15++; addr; size; return 1; }
__declspec(dllexport) int __cdecl PurifyIsReadable(const void *addr, int size) { avoidGy_16++; addr; size; return 1; }
__declspec(dllexport) int __cdecl PurifyIsWritable(const void *addr, int size) { avoidGy_17++; addr; size; return 1; }
__declspec(dllexport) int __cdecl PurifyIsInitialized(const void *addr, int size) { avoidGy_18++; addr; size; return 1; }
__declspec(dllexport) int __cdecl PurifyRed(void *addr, int size) { avoidGy_19++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyGreen(void *addr, int size) { avoidGy_20++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyYellow(void *addr, int size) { avoidGy_21++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyBlue(void *addr, int size) { avoidGy_22++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyMarkAsInitialized(void *addr, int size) { avoidGy_23++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyMarkAsUninitialized(void *addr, int size) { avoidGy_24++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyMarkForTrap(void *addr, int size) { avoidGy_25++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyMarkForNoTrap(void *addr, int size) { avoidGy_26++; addr; size; return 0; }
__declspec(dllexport) int __cdecl PurifyHeapValidate(unsigned int hHeap, unsigned int dwFlags, const void *addr) 
 { avoidGy_27++; hHeap; dwFlags; addr; return 1; }
__declspec(dllexport) int __cdecl PurifySetLateDetectScanCounter(int counter) { avoidGy_28++; counter; return 0; };
__declspec(dllexport) int __cdecl PurifySetLateDetectScanInterval(int seconds) { avoidGy_29++; seconds; return 0; };
__declspec(dllexport) int __cdecl CoverageIsRunning(void) { avoidGy_30++; return 0; }
__declspec(dllexport) int __cdecl CoverageDisableRecordingData(void) { avoidGy_31++; return 0; }
__declspec(dllexport) int __cdecl CoverageStartRecordingData(void) { avoidGy_32++; return 0; }
__declspec(dllexport) int __cdecl CoverageStopRecordingData(void) { avoidGy_33++; return 0; }
__declspec(dllexport) int __cdecl CoverageClearData(void) { avoidGy_34++; return 0; }
__declspec(dllexport) int __cdecl CoverageIsRecordingData(void) { avoidGy_35++; return 0; }
__declspec(dllexport) int __cdecl CoverageAddAnnotation(char *str) { avoidGy_36++; str; return 0; }
__declspec(dllexport) int __cdecl CoverageSaveData(void) { avoidGy_37++; return 0; }
__declspec(dllexport) int __cdecl QuantifyIsRunning(void) { avoidGy_42++; return 0; }
__declspec(dllexport) int __cdecl QuantifyDisableRecordingData(void) { avoidGy_43++; return 0; }
__declspec(dllexport) int __cdecl QuantifyStartRecordingData(void) { avoidGy_44++; return 0; }
__declspec(dllexport) int __cdecl QuantifyStopRecordingData(void) { avoidGy_45++; return 0; }
__declspec(dllexport) int __cdecl QuantifyClearData(void) { avoidGy_46++; return 0; }
__declspec(dllexport) int __cdecl QuantifyIsRecordingData(void) { avoidGy_47++; return 0; }
__declspec(dllexport) int __cdecl QuantifyAddAnnotation(char *str) { avoidGy_48++; str; return 0; }
__declspec(dllexport) int __cdecl QuantifySaveData(void) { avoidGy_49++; return 0; }
__declspec(dllexport) int __cdecl PurelockIsRunning(void) { avoidGy_PL_01++; return 0; }
