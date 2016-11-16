// Copyright © 2015, Peter Atashian
// Licensed under the MIT License <LICENSE.md>
//! FFI bindings to gdi32.
#![cfg(windows)]
extern crate winapi;
use winapi::*;
extern "system" {
    pub fn AbortDoc(hdc: HDC) -> c_int;
    pub fn AbortPath(hdc: HDC) -> BOOL;
    pub fn AddFontMemResourceEx(
        pbFont: PVOID, cbSize: DWORD, pdv: PVOID, pcFonts: *mut DWORD,
    ) -> HANDLE;
    pub fn AddFontResourceA(lpszFilename: LPCSTR) -> c_int;
    pub fn AddFontResourceExA(lpszFilename: LPCSTR, fl: DWORD, pdv: PVOID) -> c_int;
    pub fn AddFontResourceExW(lpszFilename: LPCWSTR, fl: DWORD, pdv: PVOID) -> c_int;
    pub fn AddFontResourceW(lpszFilename: LPCWSTR) -> c_int;
    pub fn AngleArc(
        hdc: HDC, X: c_int, Y: c_int, dwRadius: DWORD, eStartAngle: FLOAT, eSweepAngle: FLOAT,
    ) -> BOOL;
    pub fn AnimatePalette(
        hpal: HPALETTE, iStartIndex: UINT, cEntries: UINT, ppe: *const PALETTEENTRY,
    ) -> BOOL;
    pub fn Arc(
        hdc: HDC, nLeftRect: c_int, nTopRect: c_int, nRightRect: c_int, nBottomRect: c_int,
        nXStartArc: c_int, nYStartArc: c_int, nXEndArc: c_int, nYEndArc: c_int,
    ) -> BOOL;
    pub fn ArcTo(
        hdc: HDC, nLeftRect: c_int, nTopRect: c_int, nRightRect: c_int, nBottomRect: c_int,
        nXRadial1: c_int, nYRadial1: c_int, nXRadial2: c_int, nYRadial2: c_int,
    ) -> BOOL;
    pub fn BeginPath(hdc: HDC) -> BOOL;
    pub fn BitBlt(
        hdc: HDC, x: c_int, y: c_int, cx: c_int, cy: c_int, hdcSrc: HDC, x1: c_int, y1: c_int,
        rop: DWORD,
    ) -> BOOL;
    pub fn CancelDC(hdc: HDC) -> BOOL;
    pub fn CheckColorsInGamut(
        hDC: HDC, lpRGBTriples: LPVOID, lpBuffer: LPVOID, nCount: UINT,
    ) -> BOOL;
    pub fn ChoosePixelFormat(hdc: HDC, ppfd: *const PIXELFORMATDESCRIPTOR) -> c_int;
    pub fn Chord(
        hdc: HDC, nLeftRect: c_int, nTopRect: c_int, nRightRect: c_int, nBottomRect: c_int,
        nXRadial1: c_int, nYRadial1: c_int, nXRadial2: c_int, nYRadial2: c_int,
    ) -> BOOL;
    pub fn CloseEnhMetaFile(hdc: HDC) -> HENHMETAFILE;
    pub fn CloseFigure(hdc: HDC) -> BOOL;
    pub fn CloseMetaFile(hdc: HDC) -> HMETAFILE;
    pub fn ColorCorrectPalette(
        hDC: HDC, hPalette: HPALETTE, dwFirstEntry: DWORD, dwNumOfEntries: DWORD,
    ) -> BOOL;
    pub fn ColorMatchToTarget(hDC: HDC, hdcTarget: HDC, uiAction: UINT) -> BOOL;
    pub fn CombineRgn(
        hrgnDst: HRGN, hrgnSrc1: HRGN, hrgnSrc2: HRGN, fnCombineMode: c_int,
    ) -> c_int;
    pub fn CombineTransform(
        lpxformResult: LPXFORM, lpxform1: *const XFORM, lpxform2: *const XFORM,
    ) -> BOOL;
    pub fn CopyEnhMetaFileA(hemfSrc: HENHMETAFILE, lpszFile: LPCSTR) -> HENHMETAFILE;
    pub fn CopyEnhMetaFileW(hemfSrc: HENHMETAFILE, lpszFile: LPCWSTR) -> HENHMETAFILE;
    pub fn CopyMetaFileA(hmfSrc: HMETAFILE, lpszFile: LPCSTR) -> HMETAFILE;
    pub fn CopyMetaFileW(hmfSrc: HMETAFILE, lpszFile: LPCWSTR) -> HMETAFILE;
    pub fn CreateBitmap(
        nWidth: c_int, nHeight: c_int, nPlanes: UINT, nBitCount: UINT, lpBits: *const c_void,
    ) -> HBITMAP;
    pub fn CreateBitmapIndirect(pbm: *const BITMAP) -> HBITMAP;
    pub fn CreateBrushIndirect(lplb: *const LOGBRUSH) -> HBRUSH;
    pub fn CreateColorSpaceA(lpLogColorSpace: LPLOGCOLORSPACEA) -> HCOLORSPACE;
    pub fn CreateColorSpaceW(lpLogColorSpace: LPLOGCOLORSPACEW) -> HCOLORSPACE;
    pub fn CreateCompatibleBitmap(hdc: HDC, cx: c_int, cy: c_int) -> HBITMAP;
    pub fn CreateCompatibleDC(hdc: HDC) -> HDC;
    pub fn CreateDCA(
        lpszDriver: LPCSTR, lpszDevice: LPCSTR, lpszOutput: LPCSTR, lpInitData: *const DEVMODEA,
    ) -> HDC;
    pub fn CreateDCW(
        lpszDriver: LPCWSTR, lpszDevice: LPCWSTR, lpszOutput: LPCWSTR, lpInitData: *const DEVMODEW,
    ) -> HDC;
    pub fn CreateDIBPatternBrush(hglbDIBPacked: HGLOBAL, fuColorSpec: UINT) -> HBRUSH;
    pub fn CreateDIBPatternBrushPt(lpPackedDIB: *const VOID, iUsage: UINT) -> HBRUSH;
    pub fn CreateDIBSection(
        hdc: HDC, lpbmi: *const BITMAPINFO, usage: UINT, ppvBits: *mut *mut c_void,
        hSection: HANDLE, offset: DWORD,
    ) -> HBITMAP;
    pub fn CreateDIBitmap(
        hdc: HDC, pbmih: *const BITMAPINFOHEADER, flInit: DWORD, pjBits: *const c_void,
        pbmi: *const BITMAPINFO, iUsage: UINT,
    ) -> HBITMAP;
    pub fn CreateDiscardableBitmap(hdc: HDC, nWidth: c_int, nHeight: c_int) -> HBITMAP;
    pub fn CreateEllipticRgn(
        nLeftRect: c_int, nTopRect: c_int, nRightRect: c_int, nBottomRect: c_int,
    ) -> HRGN;
    pub fn CreateEllipticRgnIndirect(lprc: *const RECT) -> HRGN;
    pub fn CreateEnhMetaFileA(
        hdcRef: HDC, lpFilename: LPCSTR, lpRect: *const RECT, lpDescription: LPCSTR,
    ) -> HDC;
    pub fn CreateEnhMetaFileW(
        hdcRef: HDC, lpFilename: LPCWSTR, lpRect: *const RECT, lpDescription: LPCWSTR,
    ) -> HDC;
    pub fn CreateFontA(
        cHeight: c_int, cWidth: c_int, cEscapement: c_int, cOrientation: c_int, cWeight: c_int,
        bItalic: DWORD, bUnderline: DWORD, bStrikeOut: DWORD, iCharSet: DWORD,
        iOutPrecision: DWORD, iClipPrecision: DWORD, iQuality: DWORD, iPitchAndFamily: DWORD,
        pszFaceName: LPCSTR,
    ) -> HFONT;
    pub fn CreateFontIndirectA(lplf: *const LOGFONTA) -> HFONT;
    pub fn CreateFontIndirectExA(penumlfex: *const ENUMLOGFONTEXDVA) -> HFONT;
    pub fn CreateFontIndirectExW(penumlfex: *const ENUMLOGFONTEXDVW) -> HFONT;
    pub fn CreateFontIndirectW(lplf: *const LOGFONTW) -> HFONT;
    pub fn CreateFontW(
        cHeight: c_int, cWidth: c_int, cEscapement: c_int, cOrientation: c_int, cWeight: c_int,
        bItalic: DWORD, bUnderline: DWORD, bStrikeOut: DWORD, iCharSet: DWORD,
        iOutPrecision: DWORD, iClipPrecision: DWORD, iQuality: DWORD, iPitchAndFamily: DWORD,
        pszFaceName: LPCWSTR,
    ) -> HFONT;
    pub fn CreateHalftonePalette(hdc: HDC) -> HPALETTE;
    pub fn CreateHatchBrush(fnStyle: c_int, clrref: COLORREF) -> HBRUSH;
    pub fn CreateICA(
        lpszDriver: LPCSTR, lpszDevice: LPCSTR, lpszOutput: LPCSTR, lpdvmInit: *const DEVMODEA,
    ) -> HDC;
    pub fn CreateICW(
        lpszDriver: LPCWSTR, lpszDevice: LPCWSTR, lpszOutput: LPCWSTR, lpdvmInit: *const DEVMODEW,
    ) -> HDC;
    pub fn CreateMetaFileA(lpszFile: LPCSTR) -> HDC;
    pub fn CreateMetaFileW(lpszFile: LPCWSTR) -> HDC;
    pub fn CreatePalette(lplgpl: *const LOGPALETTE) -> HPALETTE;
    pub fn CreatePatternBrush(hbmp: HBITMAP) -> HBRUSH;
    pub fn CreatePen(fnPenStyle: c_int, nWidth: c_int, crColor: COLORREF) -> HPEN;
    pub fn CreatePenIndirect(lplgpn: *const LOGPEN) -> HPEN;
    pub fn CreatePolyPolygonRgn(
        lppt: *const POINT, lpPolyCounts: *const INT, nCount: c_int, fnPolyFillMode: c_int,
    ) -> HRGN;
    pub fn CreatePolygonRgn(lppt: *const POINT, cPoints: c_int, fnPolyFillMode: c_int) -> HRGN;
    pub fn CreateRectRgn(
        nLeftRect: c_int, nTopRect: c_int, nRightRect: c_int, nBottomRect: c_int,
    ) -> HRGN;
    pub fn CreateRectRgnIndirect(lprect: *const RECT) -> HRGN;
    pub fn CreateRoundRectRgn(
		x1: c_int, y1: c_int, x2: c_int, y2: c_int, w: c_int, h: c_int
	) -> HRGN;
    pub fn CreateScalableFontResourceA(
		fdwHidden: DWORD, lpszFont: LPCSTR, lpszFile: LPCSTR, lpszPath: LPCSTR
	) -> BOOL;
    pub fn CreateScalableFontResourceW(
		fdwHidden: DWORD, lpszFont: LPCWSTR, lpszFile: LPCWSTR, lpszPath: LPCWSTR
	) -> BOOL;
    pub fn CreateSolidBrush(color: COLORREF) -> HBRUSH;
    // pub fn D3DKMTAcquireKeyedMutex();
    // pub fn D3DKMTAcquireKeyedMutex2();
    // pub fn D3DKMTCacheHybridQueryValue();
    // pub fn D3DKMTCheckExclusiveOwnership();
    // pub fn D3DKMTCheckMonitorPowerState();
    // pub fn D3DKMTCheckMultiPlaneOverlaySupport();
    // pub fn D3DKMTCheckOcclusion();
    // pub fn D3DKMTCheckSharedResourceAccess();
    // pub fn D3DKMTCheckVidPnExclusiveOwnership();
    // pub fn D3DKMTCloseAdapter();
    // pub fn D3DKMTConfigureSharedResource();
    // pub fn D3DKMTCreateAllocation();
    // pub fn D3DKMTCreateAllocation2();
    // pub fn D3DKMTCreateContext();
    // pub fn D3DKMTCreateDCFromMemory();
    // pub fn D3DKMTCreateDevice();
    // pub fn D3DKMTCreateKeyedMutex();
    // pub fn D3DKMTCreateKeyedMutex2();
    // pub fn D3DKMTCreateOutputDupl();
    // pub fn D3DKMTCreateOverlay();
    // pub fn D3DKMTCreateSynchronizationObject();
    // pub fn D3DKMTCreateSynchronizationObject2();
    // pub fn D3DKMTDestroyAllocation();
    // pub fn D3DKMTDestroyContext();
    // pub fn D3DKMTDestroyDCFromMemory();
    // pub fn D3DKMTDestroyDevice();
    // pub fn D3DKMTDestroyKeyedMutex();
    // pub fn D3DKMTDestroyOutputDupl();
    // pub fn D3DKMTDestroyOverlay();
    // pub fn D3DKMTDestroySynchronizationObject();
    // pub fn D3DKMTEnumAdapters();
    // pub fn D3DKMTEscape();
    // pub fn D3DKMTFlipOverlay();
    // pub fn D3DKMTGetCachedHybridQueryValue();
    // pub fn D3DKMTGetContextInProcessSchedulingPriority();
    // pub fn D3DKMTGetContextSchedulingPriority();
    // pub fn D3DKMTGetDeviceState();
    // pub fn D3DKMTGetDisplayModeList();
    // pub fn D3DKMTGetMultisampleMethodList();
    // pub fn D3DKMTGetOverlayState();
    // pub fn D3DKMTGetPresentHistory();
    // pub fn D3DKMTGetPresentQueueEvent();
    // pub fn D3DKMTGetProcessSchedulingPriorityClass();
    // pub fn D3DKMTGetRuntimeData();
    // pub fn D3DKMTGetScanLine();
    // pub fn D3DKMTGetSharedPrimaryHandle();
    // pub fn D3DKMTGetSharedResourceAdapterLuid();
    // pub fn D3DKMTInvalidateActiveVidPn();
    // pub fn D3DKMTLock();
    // pub fn D3DKMTNetDispGetNextChunkInfo();
    // pub fn D3DKMTNetDispQueryMiracastDisplayDeviceStatus();
    // pub fn D3DKMTNetDispQueryMiracastDisplayDeviceSupport();
    // pub fn D3DKMTNetDispStartMiracastDisplayDevice2();
    // pub fn D3DKMTNetDispStopMiracastDisplayDevice();
    // pub fn D3DKMTOfferAllocations();
    // pub fn D3DKMTOpenAdapterFromDeviceName();
    // pub fn D3DKMTOpenAdapterFromGdiDisplayName();
    // pub fn D3DKMTOpenAdapterFromHdc();
    // pub fn D3DKMTOpenAdapterFromLuid();
    // pub fn D3DKMTOpenKeyedMutex();
    // pub fn D3DKMTOpenKeyedMutex2();
    // pub fn D3DKMTOpenNtHandleFromName();
    // pub fn D3DKMTOpenResource();
    // pub fn D3DKMTOpenResource2();
    // pub fn D3DKMTOpenResourceFromNtHandle();
    // pub fn D3DKMTOpenSyncObjectFromNtHandle();
    // pub fn D3DKMTOpenSynchronizationObject();
    // pub fn D3DKMTOutputDuplGetFrameInfo();
    // pub fn D3DKMTOutputDuplGetMetaData();
    // pub fn D3DKMTOutputDuplGetPointerShapeData();
    // pub fn D3DKMTOutputDuplPresent();
    // pub fn D3DKMTOutputDuplReleaseFrame();
    // pub fn D3DKMTPinDirectFlipResources();
    // pub fn D3DKMTPollDisplayChildren();
    // pub fn D3DKMTPresent();
    // pub fn D3DKMTPresentMultiPlaneOverlay();
    // pub fn D3DKMTQueryAdapterInfo();
    // pub fn D3DKMTQueryAllocationResidency();
    // pub fn D3DKMTQueryRemoteVidPnSourceFromGdiDisplayName();
    // pub fn D3DKMTQueryResourceInfo();
    // pub fn D3DKMTQueryResourceInfoFromNtHandle();
    // pub fn D3DKMTQueryStatistics();
    // pub fn D3DKMTReclaimAllocations();
    // pub fn D3DKMTReleaseKeyedMutex();
    // pub fn D3DKMTReleaseKeyedMutex2();
    // pub fn D3DKMTReleaseProcessVidPnSourceOwners();
    // pub fn D3DKMTRender();
    // pub fn D3DKMTSetAllocationPriority();
    // pub fn D3DKMTSetContextInProcessSchedulingPriority();
    // pub fn D3DKMTSetContextSchedulingPriority();
    // pub fn D3DKMTSetDisplayMode();
    // pub fn D3DKMTSetDisplayPrivateDriverFormat();
    // pub fn D3DKMTSetGammaRamp();
    // pub fn D3DKMTSetProcessSchedulingPriorityClass();
    // pub fn D3DKMTSetQueuedLimit();
    // pub fn D3DKMTSetStereoEnabled();
    // pub fn D3DKMTSetVidPnSourceOwner();
    // pub fn D3DKMTSetVidPnSourceOwner1();
    // pub fn D3DKMTShareObjects();
    // pub fn D3DKMTSharedPrimaryLockNotification();
    // pub fn D3DKMTSharedPrimaryUnLockNotification();
    // pub fn D3DKMTSignalSynchronizationObject();
    // pub fn D3DKMTSignalSynchronizationObject2();
    // pub fn D3DKMTUnlock();
    // pub fn D3DKMTUnpinDirectFlipResources();
    // pub fn D3DKMTUpdateOverlay();
    // pub fn D3DKMTWaitForIdle();
    // pub fn D3DKMTWaitForSynchronizationObject();
    // pub fn D3DKMTWaitForSynchronizationObject2();
    // pub fn D3DKMTWaitForVerticalBlankEvent();
    // pub fn D3DKMTWaitForVerticalBlankEvent2();
    pub fn DPtoLP(hdc: HDC, lppt: *mut POINT, c: c_int) -> BOOL;
    pub fn DeleteColorSpace(hcs: HCOLORSPACE) -> BOOL;
    pub fn DeleteDC(hdc: HDC) -> BOOL;
    pub fn DeleteEnhMetaFile(hmf: HENHMETAFILE) -> BOOL;
    pub fn DeleteMetaFile(hmf: HMETAFILE) -> BOOL;
    pub fn DeleteObject(ho: HGDIOBJ) -> BOOL;
    pub fn DescribePixelFormat(
        hdc: HDC, iPixelFormat: c_int, nBytes: UINT, ppfd: LPPIXELFORMATDESCRIPTOR,
    ) -> c_int;
    // pub fn DeviceCapabilitiesExA();
    // pub fn DeviceCapabilitiesExW();
    pub fn DrawEscape(hdc: HDC, iEscape: c_int, cjIn: c_int, lpIn: LPCSTR) -> c_int;
    pub fn Ellipse(hdc: HDC, left: c_int, top: c_int, right: c_int, bottom: c_int) -> BOOL;
    // pub fn EnableEUDC();
    pub fn EndDoc(hdc: HDC) -> c_int;
    // pub fn EndFormPage();
    pub fn EndPage(hdc: HDC) -> c_int;
    pub fn EndPath(hdc: HDC) -> BOOL;
    pub fn EnumEnhMetaFile(
        hdc: HDC, hmf: HENHMETAFILE, lpProc: ENHMFENUMPROC, param: LPVOID, lpRect: *const RECT
    ) -> BOOL;
    pub fn EnumFontFamiliesA(
		hdc: HDC, lpLogfont: LPCSTR, lpProc: FONTENUMPROCA, lParam: LPARAM
	) -> c_int;
    pub fn EnumFontFamiliesExA(
		hdc: HDC, lpLogfont: LPLOGFONTA, lpProc: FONTENUMPROCA, lParam: LPARAM, dwFlags: DWORD
	) -> c_int;
    pub fn EnumFontFamiliesExW(
		hdc: HDC, lpLogfont: LPLOGFONTW, lpProc: FONTENUMPROCW, lParam: LPARAM, dwFlags: DWORD
	) -> c_int;
    pub fn EnumFontFamiliesW(
		hdc: HDC, lpLogfont: LPCWSTR, lpProc: FONTENUMPROCW, lParam: LPARAM
	) -> c_int;
    pub fn EnumFontsA(
		hdc: HDC, lpLogfont: LPCSTR, lpProc: FONTENUMPROCA, lParam: LPARAM
	) -> c_int;
    pub fn EnumFontsW(
		hdc: HDC, lpLogfont: LPCWSTR, lpProc: FONTENUMPROCW, lParam: LPARAM
	) -> c_int;
    pub fn EnumICMProfilesA(
		hdc: HDC, iproc: ICMENUMPROCA, param: LPARAM
	) -> c_int;
    pub fn EnumICMProfilesW(
		hdc: HDC, iproc: ICMENUMPROCW, param: LPARAM
	) -> c_int;
    pub fn EnumMetaFile(
		hdc: HDC, hmf: HMETAFILE, mproc: MFENUMPROC, param: LPARAM
	) -> BOOL;
    pub fn EnumObjects(
		hdc: HDC, nType: c_int, lpFunc: GOBJENUMPROC, lParam: LPARAM
	) -> c_int;
    pub fn EqualRgn(hrgn1: HRGN, hrgn2: HRGN) -> BOOL;
    pub fn Escape(hdc: HDC, iEscape: c_int, cjIn: c_int, pvIn: LPCSTR, pvOut: LPVOID) -> c_int;
    // pub fn EudcLoadLinkW();
    // pub fn EudcUnloadLinkW();
    pub fn ExcludeClipRect(hdc: HDC, left: c_int, top: c_int, right: c_int, bottom: c_int) -> c_int;
    pub fn ExtCreatePen(
        iPenStyle: DWORD, cWidth: DWORD, plbrush: *const LOGBRUSH, cStyle: DWORD, pstyle: *const DWORD
    ) -> HPEN;
    pub fn ExtCreateRegion(lpx: *const XFORM, nCount: DWORD, lpData: *const RGNDATA) -> HRGN;
    pub fn ExtEscape(
        hdc: HDC, iEscape: c_int, cjInput: c_int, lpInData: LPCSTR, cjOutput: c_int, lpOutData: LPSTR
    ) -> c_int;
    pub fn ExtFloodFill(hdc: HDC, x: c_int, y: c_int, color: COLORREF, utype: UINT) -> BOOL;
    pub fn ExtSelectClipRgn(hdc: HDC, hrgn: HRGN, mode: c_int) -> c_int;
    pub fn ExtTextOutA(
		hdc: HDC, x: c_int, y: c_int, options: UINT, lprect: *const RECT, lpString: LPCSTR, c: UINT,
		lpDx: *const INT
	) -> BOOL;
    pub fn ExtTextOutW(
		hdc: HDC, x: c_int, y: c_int, options: UINT, lprect: *const RECT, lpString: LPCWSTR, c: UINT,
		lpDx: *const INT
	) -> BOOL;
    pub fn FillPath(hdc: HDC) -> BOOL;
    pub fn FillRgn(hdc: HDC, hrgn: HRGN, hbr: HBRUSH) -> BOOL;
    pub fn FixBrushOrgEx(hdc: HDC, x: c_int, y: c_int, ptl: LPPOINT) -> BOOL;
    pub fn FlattenPath(hdc: HDC) -> BOOL;
    pub fn FloodFill(hdc: HDC, x: c_int, y: c_int, color: COLORREF) -> BOOL;
    pub fn FrameRgn(hdc: HDC, hrgn: HRGN, hbr: HBRUSH, w: c_int, h: c_int) -> BOOL;
    pub fn GdiAlphaBlend(
		hdcDest: HDC, xoriginDest: c_int, yoriginDest: c_int, wDest: c_int, hDest: c_int,
        hdcSrc: HDC, xoriginSrc: c_int, yoriginSrc: c_int, wSrc: c_int, hSrc: c_int,
        ftn: BLENDFUNCTION
	) -> BOOL;
    // pub fn GdiArtificialDecrementDriver();
    pub fn GdiComment(hdc: HDC, nSize: UINT, lpData: *const BYTE) -> BOOL;
    // pub fn GdiDeleteSpoolFileHandle();
    // pub fn GdiEndDocEMF();
    // pub fn GdiEndPageEMF();
    pub fn GdiFlush() -> BOOL;
    pub fn GdiGetBatchLimit() -> DWORD;
    // pub fn GdiGetDC();
    // pub fn GdiGetDevmodeForPage();
    // pub fn GdiGetPageCount();
    // pub fn GdiGetPageHandle();
    // pub fn GdiGetSpoolFileHandle();
    pub fn GdiGradientFill(
        hdc: HDC, pVertex: PTRIVERTEX, nVertex: ULONG, pMesh: PVOID, nCount: ULONG, ulMode: ULONG
    ) -> BOOL;
    // pub fn GdiPlayDCScript();
    // pub fn GdiPlayEMF();
    // pub fn GdiPlayJournal();
    // pub fn GdiPlayPageEMF();
    // pub fn GdiPlayPrivatePageEMF();
    // pub fn GdiPlayScript();
    // pub fn GdiResetDCEMF();
    pub fn GdiSetBatchLimit(dw: DWORD) -> DWORD;
    // pub fn GdiStartDocEMF();
    // pub fn GdiStartPageEMF();
    pub fn GdiTransparentBlt(
        hdcDest: HDC, xoriginDest: c_int, yoriginDest: c_int, wDest: c_int, hDest: c_int, hdcSrc: HDC,
        xoriginSrc: c_int, yoriginSrc: c_int, wSrc: c_int, hSrc: c_int, crTransparent: UINT
    ) -> BOOL;
    pub fn GetArcDirection(hdc: HDC) -> c_int;
    pub fn GetAspectRatioFilterEx(hdc: HDC, lpsize: LPSIZE) -> BOOL;
    pub fn GetBitmapBits(hbit: HBITMAP, cb: LONG, lpvBits: LPVOID) -> LONG;
    pub fn GetBitmapDimensionEx(hbit: HBITMAP, lpsize: LPSIZE) -> BOOL;
    pub fn GetBkColor(hdc: HDC) -> COLORREF;
    pub fn GetBkMode(hdc: HDC) -> c_int;
    pub fn GetBoundsRect(hdc: HDC, lprect: LPRECT, flags: UINT) -> UINT;
    pub fn GetBrushOrgEx(hdc: HDC, lppt: LPPOINT) -> BOOL;
    pub fn GetCharABCWidthsA( hdc: HDC, wFirst: UINT, wLast: UINT, lpABC: LPABC) -> BOOL;
    pub fn GetCharABCWidthsFloatA(
		hdc: HDC, iFirst: UINT, iLast: UINT, lpABC: LPABCFLOAT
	) -> BOOL;
    pub fn GetCharABCWidthsFloatW(
		hdc: HDC, iFirst: UINT, iLast: UINT, lpABC: LPABCFLOAT
	) -> BOOL;
    pub fn GetCharABCWidthsI(hdc: HDC, giFirst: UINT, cgi: UINT, pgi: LPWORD, pabc: LPABC) -> BOOL;
    pub fn GetCharABCWidthsW(hdc: HDC, wFirst: UINT, wLast: UINT, lpABC: LPABC) -> BOOL;
    pub fn GetCharWidth32A(hdc: HDC, iFirst: UINT, iLast: UINT, lpBuffer: LPINT) -> BOOL;
    pub fn GetCharWidth32W(hdc: HDC, iFirst: UINT, iLast: UINT, lpBuffer: LPINT) -> BOOL;
    pub fn GetCharWidthA(hdc: HDC, iFirst: UINT, iLast: UINT, lpBuffer: LPINT) -> BOOL;
    pub fn GetCharWidthFloatA(hdc: HDC, iFirst: UINT, iLast: UINT, lpBuffer: PFLOAT) -> BOOL;
    pub fn GetCharWidthFloatW(hdc: HDC, iFirst: UINT, iLast: UINT, lpBuffer: PFLOAT) -> BOOL;
    pub fn GetCharWidthI(hdc: HDC, giFirst: UINT, cgi: UINT, pgi: LPWORD, piWidths: LPINT) -> BOOL;
    pub fn GetCharWidthW(hdc: HDC, iFirst: UINT, iLast: UINT, lpBuffer: LPINT) -> BOOL;
    pub fn GetCharacterPlacementA(
		hdc: HDC, lpString: LPCSTR, nCount: c_int, nMexExtent: c_int, lpResults: LPGCP_RESULTSA,
		dwFlags: DWORD
	) -> DWORD;
    pub fn GetCharacterPlacementW(
		hdc: HDC, lpString: LPCWSTR, nCount: c_int, nMexExtent: c_int, lpResults: LPGCP_RESULTSW,
		dwFlags: DWORD
	) -> DWORD;
    pub fn GetClipBox(hdc: HDC, lprect: LPRECT) -> c_int;
    pub fn GetClipRgn(hdc: HDC, hrgn: HRGN) -> c_int;
    pub fn GetColorAdjustment(hdc: HDC, lpca: LPCOLORADJUSTMENT) -> BOOL;
    pub fn GetColorSpace(hdc: HDC) -> HCOLORSPACE;
    pub fn GetCurrentObject(hdc: HDC, tp: UINT) -> HGDIOBJ;
    pub fn GetCurrentPositionEx(hdc: HDC, lppt: LPPOINT) -> BOOL;
    pub fn GetDCBrushColor(hdc: HDC) -> COLORREF;
    pub fn GetDCOrgEx(hdc: HDC, lppt: LPPOINT) -> BOOL;
    pub fn GetDCPenColor(hdc: HDC) -> COLORREF;
    pub fn GetDIBColorTable(hdc: HDC, iStart: UINT, cEntries: UINT, prgbq: *mut RGBQUAD) -> UINT;
    pub fn GetDIBits(
        hdc: HDC, hbm: HBITMAP, start: UINT, cLines: UINT, lpvBits: LPVOID, lpbmi: LPBITMAPINFO,
        usage: UINT
    ) -> c_int;
    pub fn GetDeviceCaps(hdc: HDC, nIndex: c_int) -> c_int;
    pub fn GetDeviceGammaRamp(hdc: HDC, lpRamp: LPVOID) -> BOOL;
    pub fn GetEnhMetaFileA(lpName: LPCSTR) -> HENHMETAFILE;
    pub fn GetEnhMetaFileBits(hEMF: HENHMETAFILE, nSize: UINT, lpData: LPBYTE) -> UINT;
    pub fn GetEnhMetaFileDescriptionA(hemf: HENHMETAFILE, cchBuffer: UINT, lpDescription: LPSTR) -> UINT;
    pub fn GetEnhMetaFileDescriptionW(hemf: HENHMETAFILE, cchBuffer: UINT, lpDescription: LPWSTR) -> UINT;
    pub fn GetEnhMetaFileHeader(
        hemf: HENHMETAFILE, nSize: UINT, lpEnhMetaHeader: LPENHMETAHEADER
    ) -> UINT;
    pub fn GetEnhMetaFilePaletteEntries(
        hemf: HENHMETAFILE, nNumEntries: UINT, lpPaletteEntries: LPPALETTEENTRY
    ) -> UINT;
    pub fn GetEnhMetaFilePixelFormat(
        hemf: HENHMETAFILE, cbBuffer: UINT, ppfd: *mut PIXELFORMATDESCRIPTOR
    ) -> UINT;
    pub fn GetEnhMetaFileW(lpName: LPCWSTR) -> HENHMETAFILE;
    // pub fn GetFontAssocStatus();
    pub fn GetFontData(
        hdc: HDC, dwTable: DWORD, dwOffset: DWORD, pvBuffer: PVOID, cjBuffer: DWORD
    ) -> DWORD;
    pub fn GetFontLanguageInfo(hdc: HDC) -> DWORD;
    // pub fn GetFontResourceInfoW();
    pub fn GetFontUnicodeRanges(hdc: HDC, lpgs: LPGLYPHSET) -> DWORD;
    pub fn GetGlyphIndicesA(hdc: HDC, lpstr: LPCSTR, c: c_int, pgi: LPWORD, fl: DWORD) -> DWORD;
    pub fn GetGlyphIndicesW(hdc: HDC, lpstr: LPCWSTR, c: c_int, pgi: LPWORD, fl: DWORD) -> DWORD;
    // pub fn GetGlyphOutline();
    pub fn GetGlyphOutlineA(
        hdc: HDC, uChar: UINT, fuFormat: UINT, lpgm: LPGLYPHMETRICS, cjBuffer: DWORD,
        pvBuffer: LPVOID, lpmat2: *const MAT2
    ) -> DWORD;
    pub fn GetGlyphOutlineW(
        hdc: HDC, uChar: UINT, fuFormat: UINT, lpgm: LPGLYPHMETRICS, cjBuffer: DWORD,
        pvBuffer: LPVOID, lpmat2: *const MAT2
    ) -> DWORD;
    pub fn GetGraphicsMode(hdc: HDC) -> c_int;
    pub fn GetICMProfileA(hdc: HDC, pBufSize: LPDWORD, pszFilename: LPSTR) -> BOOL;
    pub fn GetICMProfileW(hdc: HDC, pBufSize: LPDWORD, pszFilename: LPWSTR) -> BOOL;
    // pub fn GetKerningPairs();
    pub fn GetKerningPairsA(hdc: HDC, nPairs: DWORD, lpKernPair: LPKERNINGPAIR) -> DWORD;
    pub fn GetKerningPairsW(hdc: HDC, nPairs: DWORD, lpKernPair: LPKERNINGPAIR) -> DWORD;
    pub fn GetLayout(hdc: HDC) -> DWORD;
    pub fn GetLogColorSpaceA(
        hColorSpace: HCOLORSPACE, lpBuffer: LPLOGCOLORSPACEA, nSize: DWORD
    ) -> BOOL;
    pub fn GetLogColorSpaceW(
        hColorSpace: HCOLORSPACE, lpBuffer: LPLOGCOLORSPACEW, nSize: DWORD
    ) -> BOOL;
    pub fn GetMapMode(hdc: HDC) -> c_int;
    pub fn GetMetaFileA(lpName: LPCSTR) -> HMETAFILE;
    pub fn GetMetaFileBitsEx(hMF: HMETAFILE, cbBuffer: UINT, lpData: LPVOID) -> UINT;
    pub fn GetMetaFileW(lpName: LPCWSTR) -> HMETAFILE;
    pub fn GetMetaRgn(hdc: HDC, hrgn: HRGN) -> c_int;
    pub fn GetMiterLimit(hdc: HDC, plimit: PFLOAT) -> BOOL;
    pub fn GetNearestColor(hdc: HDC, color: COLORREF) -> COLORREF;
    pub fn GetNearestPaletteIndex(h: HPALETTE, color: COLORREF) -> UINT;
    pub fn GetObjectA(h: HANDLE, c: c_int, pv: LPVOID) -> c_int;
    pub fn GetObjectType(h: HGDIOBJ) -> DWORD;
    pub fn GetObjectW(h: HANDLE, c: c_int, pv: LPVOID) -> c_int;
    pub fn GetOutlineTextMetricsA(hdc: HDC, cjCopy: UINT, potm: LPOUTLINETEXTMETRICA) -> UINT;
    pub fn GetOutlineTextMetricsW(hdc: HDC, cjCopy: UINT, potm: LPOUTLINETEXTMETRICW) -> UINT;
    pub fn GetPaletteEntries(
        hpal: HPALETTE, iStart: UINT, cEntries: UINT, pPalEntries: LPPALETTEENTRY
    ) -> UINT;
    pub fn GetPath(hdc: HDC, apt: LPPOINT, aj: LPBYTE, cpt: c_int) -> c_int;
    pub fn GetPixel(hdc: HDC, x: c_int, y: c_int) -> COLORREF;
    pub fn GetPixelFormat(hdc: HDC) -> c_int;
    pub fn GetPolyFillMode(hdc: HDC) -> c_int;
    pub fn GetROP2(hdc: HDC) -> c_int;
    pub fn GetRandomRgn (hdc: HDC, hrgn: HRGN, i: INT) -> c_int;
    pub fn GetRasterizerCaps(lpraststat: LPRASTERIZER_STATUS, cjBytes: UINT) -> BOOL;
    pub fn GetRegionData(hrgn: HRGN, nCount: DWORD, lpRgnData: LPRGNDATA) -> DWORD;
    // pub fn GetRelAbs();
    pub fn GetRgnBox(hrgn: HRGN, lprc: LPRECT) -> c_int;
    pub fn GetStockObject(i: c_int) -> HGDIOBJ;
    pub fn GetStretchBltMode(hdc: HDC) -> c_int;
    pub fn GetSystemPaletteEntries(hdc: HDC, iStart: UINT, cEntries: UINT, pPalEntries: LPPALETTEENTRY) -> UINT;
    pub fn GetSystemPaletteUse(hdc: HDC) -> UINT;
    pub fn GetTextAlign(hdc: HDC) -> UINT;
    pub fn GetTextCharacterExtra(hdc: HDC) -> c_int;
    pub fn GetTextCharset(hdc: HDC) -> c_int;
    pub fn GetTextCharsetInfo(hdc: HDC, lpSig: LPFONTSIGNATURE, dwFlags: DWORD) -> c_int;
    pub fn GetTextColor(hdc: HDC) -> COLORREF;
    pub fn GetTextExtentExPointA(
        hdc: HDC, lpszString: LPCSTR, cchString: c_int, nMaxExtent: c_int, lpnFit: LPINT,
        lpnDx: LPINT, lpSize: LPSIZE
    ) -> BOOL;
    pub fn GetTextExtentExPointI(hdc: HDC, lpwszString: LPWORD, cwchString: c_int, nMaxExtent: c_int,
        lpnFit: LPINT, lpnDx: LPINT, lpSize: LPSIZE
    ) -> BOOL;
    pub fn GetTextExtentExPointW(
        hdc: HDC, lpszString: LPCWSTR, cchString: c_int, nMaxExtent: c_int, lpnFit: LPINT,
        lpnDx: LPINT, lpSize: LPSIZE
    ) -> BOOL;
    pub fn GetTextExtentPoint32A(hdc: HDC, lpString: LPCSTR, c: c_int, psizl: LPSIZE) -> BOOL;
    pub fn GetTextExtentPoint32W(hdc: HDC, lpString: LPCWSTR, c: c_int, psizl: LPSIZE) -> BOOL;
    pub fn GetTextExtentPointA(hdc: HDC, lpString: LPCSTR, c: c_int, lpsz: LPSIZE) -> BOOL;
    pub fn GetTextExtentPointI(hdc: HDC, pgiIn: LPWORD, cgi: c_int, psize: LPSIZE) -> BOOL;
    pub fn GetTextExtentPointW(hdc: HDC, lpString: LPCWSTR, c: c_int, lpsz: LPSIZE) -> BOOL;
    pub fn GetTextFaceA(hdc: HDC, c: c_int, lpName: LPSTR) -> c_int;
    pub fn GetTextFaceW(hdc: HDC, c: c_int, lpName: LPWSTR) -> c_int;
    pub fn GetTextMetricsA(hdc: HDC, lptm: LPTEXTMETRICA) -> BOOL;
    pub fn GetTextMetricsW(hdc: HDC, lptm: *mut TEXTMETRICW) -> BOOL;
    pub fn GetViewportExtEx(hdc: HDC, lpsize: LPSIZE) -> BOOL;
    pub fn GetViewportOrgEx(hdc: HDC, lppoint: LPPOINT) -> BOOL;
    pub fn GetWinMetaFileBits(
        hemf: HENHMETAFILE, cbData16: UINT, pData16: LPBYTE, iMapMode: INT, hdcRef: HDC
    ) -> UINT;
    pub fn GetWindowExtEx(hdc: HDC, lpsize: LPSIZE) -> BOOL;
    pub fn GetWindowOrgEx(hdc: HDC, lppoint: LPPOINT) -> BOOL;
    pub fn GetWorldTransform(hdc: HDC, lpxf: LPXFORM) -> BOOL;
    pub fn IntersectClipRect(
		hdc: HDC, left: c_int, top: c_int, right: c_int, bottom: c_int
	) -> c_int;
    pub fn InvertRgn(hdc: HDC, hrgn: HRGN) -> BOOL;
    pub fn LPtoDP(hdc: HDC, lppt: LPPOINT, c: c_int) -> BOOL;
    pub fn LineDDA(
        nXStart: c_int, nYStart: c_int, nXEnd: c_int, nYEnd: c_int, lpLineFunc: LINEDDAPROC,
        lpData: LPARAM,
    ) -> BOOL;
    pub fn LineTo(hdc: HDC, nXEnd: c_int, nYEnd: c_int) -> BOOL;
    pub fn MaskBlt(
        hdcDest: HDC, xDest: c_int, yDest: c_int, width: c_int, height: c_int, hdcSrc: HDC,
        xSrc: c_int, ySrc: c_int, hbmMask: HBITMAP, xMask: c_int, yMask: c_int, rop: DWORD
    ) -> BOOL;
    pub fn ModifyWorldTransform(hdc: HDC, lpxf: *const XFORM, mode: DWORD) -> BOOL;
    pub fn MoveToEx(hdc: HDC, X: c_int, Y: c_int, lpPoint:LPPOINT) -> BOOL;
    pub fn OffsetClipRgn(hdc: HDC, x: c_int, y: c_int) -> c_int;
    pub fn OffsetRgn(hrgn: HRGN, x: c_int, y: c_int) -> c_int;
    pub fn OffsetViewportOrgEx(hdc: HDC, x: c_int, y: c_int, lppt: LPPOINT) -> BOOL;
    pub fn OffsetWindowOrgEx(hdc: HDC, x: c_int, y: c_int, lppt: LPPOINT) -> BOOL;
    pub fn PaintRgn(hdc: HDC, hrgn: HRGN) -> BOOL;
    pub fn PatBlt(
        hdc: HDC, nXLeft: c_int, nYLeft: c_int, nWidth: c_int, nHeight: c_int, dwRop: DWORD,
    ) -> BOOL;
    pub fn PathToRegion(hdc: HDC) -> HRGN;
    pub fn Pie(
        hdc: HDC, nLeftRect: c_int, nTopRect: c_int, nRightRect: c_int, nBottomRect: c_int, nXRadial1: c_int,
        nYRadial1: c_int, nXRadial2: c_int, nYRadial2: c_int,
    ) -> BOOL;
    pub fn PlayEnhMetaFile(hdc: HDC, hmf: HENHMETAFILE, lprect: *const RECT) -> BOOL;
    pub fn PlayEnhMetaFileRecord(
        hdc: HDC, pht: LPHANDLETABLE, pmr: *const ENHMETARECORD,cht: UINT
    ) -> BOOL;
    pub fn PlayMetaFile(hdc: HDC, hmf: HMETAFILE) -> BOOL;
    pub fn PlayMetaFileRecord(hdc: HDC, lpHandleTable: LPHANDLETABLE, lpMR: LPMETARECORD, noObjs: UINT) -> BOOL;
    pub fn PlgBlt(hdcDest: HDC, lpPoint: *const POINT, hdcSrc: HDC, xSrc: c_int, ySrc: c_int,
        width: c_int, height: c_int, hbmMask: HBITMAP, xMask: c_int, yMask: c_int
    ) -> BOOL;
    pub fn PolyBezier(hdc: HDC, lppt: *const POINT, cPoints: DWORD) -> BOOL;
    pub fn PolyBezierTo(hdc: HDC, lppt: *const POINT, cPoints: DWORD) -> BOOL;
    pub fn PolyDraw(hdc: HDC, lppt: *const POINT, lpbTypes: *const BYTE, cCount: c_int) -> BOOL;
    pub fn PolyPolygon(
        hdc: HDC, lpPoints: *const POINT, lpPolyCounts: *const INT, cCount: DWORD,
    ) -> BOOL;
    pub fn PolyPolyline(
        hdc: HDC, lppt: *const POINT, lpdwPolyPoints: *const DWORD, cCount: DWORD,
    ) -> BOOL;
    pub fn PolyTextOutA(hdc: HDC, ppt: *const POLYTEXTA, nstrings: c_int) -> BOOL;
    pub fn PolyTextOutW(hdc: HDC, ppt: *const POLYTEXTW, nstrings: c_int) -> BOOL;
    pub fn Polygon(hdc: HDC, lpPoints: *const POINT, nCount: c_int) -> BOOL;
    pub fn Polyline(hdc: HDC, lppt: *const POINT, cCount: c_int) -> BOOL;
    pub fn PolylineTo(hdc: HDC, lppt: *const POINT, cCount: DWORD) -> BOOL;
    pub fn PtInRegion(hrgn: HRGN, x: c_int, y: c_int) -> BOOL;
    pub fn PtVisible(hdc: HDC, x: c_int, y: c_int) -> BOOL;
    pub fn RealizePalette(hdc: HDC) -> UINT;
    pub fn RectInRegion(hrgn: HRGN, lprect: *const RECT) -> BOOL;
    pub fn RectVisible(hdc: HDC, lprect: *const RECT) -> BOOL;
    pub fn Rectangle(hdc: HDC, left: c_int, top: c_int, right: c_int, bottom: c_int) -> BOOL;
    pub fn RemoveFontMemResourceEx(h: HANDLE) -> BOOL;
    pub fn RemoveFontResourceA(lpFileName: LPCSTR) -> BOOL;
    pub fn RemoveFontResourceExA(name: LPCSTR, fl: DWORD, pdv: PVOID) -> BOOL;
    pub fn RemoveFontResourceExW(name: LPCWSTR, fl: DWORD, pdv: PVOID) -> BOOL;
    pub fn RemoveFontResourceW(lpFileName: LPCWSTR) -> BOOL;
    pub fn ResetDCA(hdc: HDC, lpdm: *const DEVMODEA) -> HDC;
    pub fn ResetDCW(hdc: HDC, lpdm: *const DEVMODEW) -> HDC;
    pub fn ResizePalette(hpal: HPALETTE, n: UINT) -> BOOL;
    pub fn RestoreDC(hdc: HDC, nSavedDC: c_int) -> BOOL;
    pub fn RoundRect(
        hdc: HDC, nLeftRect: c_int, nTopRect: c_int, nRightRect: c_int, nBottomRect: c_int,
        nWidth: c_int, nHeight: c_int,
    ) -> BOOL;
    pub fn SaveDC(hdc: HDC) -> c_int;
    pub fn ScaleViewportExtEx(
		hdc: HDC,xn: c_int, dx: c_int, yn: c_int, yd: c_int, lpsz: LPSIZE
	) -> BOOL;
    pub fn ScaleWindowExtEx(
		hdc: HDC, xn: c_int, xd: c_int, yn: c_int, yd: c_int, lpsz: LPSIZE
	) -> BOOL;
    // pub fn SelectBrushLocal();
    pub fn SelectClipPath(hdc: HDC, mode: c_int) -> BOOL;
    pub fn SelectClipRgn(hdc: HDC, hrgn: HRGN) -> c_int;
    // pub fn SelectFontLocal();
    pub fn SelectObject(hdc: HDC, h: HGDIOBJ) -> HGDIOBJ;
    pub fn SelectPalette(hdc: HDC, hPal: HPALETTE, bForceBkgd: BOOL) -> HPALETTE;
    pub fn SetAbortProc(hdc: HDC, aproc: ABORTPROC) -> c_int;
    pub fn SetArcDirection(hdc: HDC, ArcDirection: c_int) -> c_int;
    pub fn SetBitmapBits(hbm: HBITMAP, cb: DWORD, pvBits: *const VOID) -> LONG;
    pub fn SetBitmapDimensionEx(hbm: HBITMAP, w: c_int, h: c_int, lpsz: LPSIZE) -> BOOL;
    pub fn SetBkColor(hdc: HDC, color: COLORREF) -> COLORREF;
    pub fn SetBkMode(hdc: HDC, mode: c_int) -> c_int;
    pub fn SetBoundsRect(hdc: HDC, lprect: *const RECT, flags: UINT) -> UINT;
    pub fn SetBrushOrgEx(hdc: HDC, x: c_int, y: c_int, lppt: LPPOINT) -> BOOL;
    pub fn SetColorAdjustment(hdc: HDC, lpca: *const COLORADJUSTMENT) -> BOOL;
    pub fn SetColorSpace(hdc: HDC, hcs: HCOLORSPACE) -> HCOLORSPACE;
    pub fn SetDCBrushColor(hdc: HDC, color: COLORREF) -> COLORREF;
    pub fn SetDCPenColor(hdc: HDC, color: COLORREF) -> COLORREF;
    pub fn SetDIBColorTable(hdc: HDC, iStart: UINT, cEntries: UINT, prgbq: *const RGBQUAD) -> UINT;
    pub fn SetDIBits(
		hdc: HDC, hbm: HBITMAP, start: UINT, cLines: UINT, lpBits: *const VOID,
		lpbmi: *const BITMAPINFO, ColorUse: UINT
	) -> c_int;
    pub fn SetDIBitsToDevice(
        hdc: HDC, xDest: c_int, yDest: c_int, w: DWORD, h: DWORD, xSrc: c_int, ySrc: c_int,
        StartScan: UINT, cLines: UINT, lpvBits: *const VOID, lpbmi: *const BITMAPINFO, ColorUse: UINT
    ) -> c_int;
    pub fn SetDeviceGammaRamp(hdc: HDC, lpRamp: LPVOID) -> BOOL;
    pub fn SetEnhMetaFileBits(nSize: UINT, pb: *const BYTE) -> HENHMETAFILE;
    // pub fn SetFontEnumeration();
    pub fn SetGraphicsMode(hdc: HDC, iMode: c_int) -> c_int;
    pub fn SetICMMode(hdc: HDC, mode: c_int) -> c_int;
    pub fn SetICMProfileA(hdc: HDC, lpFileName: LPSTR) -> BOOL;
    pub fn SetICMProfileW(hdc: HDC, lpFileName: LPWSTR) -> BOOL;
    pub fn SetLayout(hdc: HDC, l: DWORD) -> DWORD;
    // pub fn SetMagicColors();
    pub fn SetMapMode(hdc: HDC, mode: c_int) -> c_int;
    pub fn SetMapperFlags(hdc: HDC, flags: DWORD) -> DWORD;
    pub fn SetMetaFileBitsEx(cbBuffer: UINT, lpData: *const BYTE) -> HMETAFILE;
    pub fn SetMetaRgn(hdc: HDC) -> c_int;
    pub fn SetMiterLimit(hdc: HDC, limit: FLOAT, old: PFLOAT) -> BOOL;
    pub fn SetPaletteEntries(
        hpal: HPALETTE, iStart: UINT, cEntries: UINT, pPalEntries: *const PALETTEENTRY
    ) -> UINT;
    pub fn SetPixel(hdc: HDC, x: c_int, y: c_int, color: COLORREF) -> COLORREF;
    pub fn SetPixelFormat(
        hdc: HDC, iPixelFormat: c_int, ppfd: *const PIXELFORMATDESCRIPTOR,
    ) -> BOOL;
    pub fn SetPixelV(hdc: HDC, x: c_int, y: c_int, color: COLORREF) -> BOOL;
    pub fn SetPolyFillMode(hdc: HDC, iPolyFillMode: c_int) -> c_int;
    pub fn SetROP2(hdc: HDC, rop2: c_int) -> c_int;
    pub fn SetRectRgn(hrgn: HRGN, left: c_int, top: c_int, right: c_int, bottom: c_int) -> BOOL;
    // pub fn SetRelAbs();
    pub fn SetStretchBltMode(hdc: HDC, mode: c_int) -> c_int;
    pub fn SetSystemPaletteUse(hdc: HDC, uuse: UINT) -> UINT;
    pub fn SetTextAlign(hdc: HDC, align: UINT) -> UINT;
    pub fn SetTextCharacterExtra(hdc: HDC, extra: c_int) -> c_int;
    pub fn SetTextColor(hdc: HDC, color: COLORREF) -> COLORREF;
    pub fn SetTextJustification(hdc: HDC, extra: c_int, count: c_int) -> BOOL;
    pub fn SetViewportExtEx(hdc: HDC, x: c_int, y: c_int, lpsz: *mut SIZE) -> BOOL;
    pub fn SetViewportOrgEx(hdc: HDC, x: c_int, y: c_int, lppt: *mut POINT) -> BOOL;
    pub fn SetWinMetaFileBits(
        nSize: UINT, lpMeta16Data: *const BYTE, hdcRef: HDC, lpMFP: *const METAFILEPICT
    ) -> HENHMETAFILE;
    pub fn SetWindowExtEx(hdc: HDC, x: c_int, y: c_int, lppt: *mut SIZE) -> BOOL;
    pub fn SetWindowOrgEx(hdc: HDC, x: c_int, y: c_int, lppt: LPPOINT) -> BOOL;
    pub fn SetWorldTransform(hdc: HDC, lpxf: *const XFORM) -> BOOL;
    pub fn StartDocA(hdc: HDC, lpdi: *const DOCINFOA) -> c_int;
    pub fn StartDocW(hdc: HDC, lpdi: *const DOCINFOW) -> c_int;
    // pub fn StartFormPage();
    pub fn StartPage(hdc: HDC) -> c_int;
    pub fn StretchBlt(
		hdcDest: HDC, xDest: c_int, yDest: c_int, wDest: c_int, hDest: c_int, hdcSrc: HDC,
		xSrc: c_int, ySrc: c_int, wSrc: c_int, hSrc: c_int, rop: DWORD
	) -> BOOL;
    pub fn StretchDIBits(
        hdc: HDC, XDest: c_int, YDest: c_int, nDestWidth: c_int,
        nDestHeight: c_int, XSrc: c_int, YSrc: c_int, nSrcWidth: c_int,
        nSrcHeight: c_int, lpBits: *const VOID, lpBitsInfo: *const BITMAPINFO,
        iUsage: UINT, dwRop: DWORD,
    ) -> c_int;
    pub fn StrokeAndFillPath(hdc: HDC) -> BOOL;
    pub fn StrokePath(hdc: HDC) -> BOOL;
    pub fn SwapBuffers(hdc: HDC) -> BOOL;
    pub fn TextOutA(hdc: HDC, x: c_int, y: c_int, lpString: LPCSTR, c: c_int) -> BOOL;
    pub fn TextOutW(hdc: HDC, x: c_int, y: c_int, lpString: LPCWSTR, c: c_int) -> BOOL;
    pub fn TranslateCharsetInfo(
		lpSrc: *const DWORD, lpCs: LPCHARSETINFO, dwFlags: DWORD
	) -> BOOL;
    pub fn UnrealizeObject(h: HGDIOBJ) -> BOOL;
    pub fn  UpdateColors(hdc: HDC) -> BOOL;
    pub fn UpdateICMRegKeyA(
		reserved: DWORD, lpszCMID: LPSTR, lpszFileName: LPSTR, command: UINT
	) -> BOOL;
    pub fn UpdateICMRegKeyW(
		reserved: DWORD, lpszCMID: LPWSTR, lpszFileName: LPWSTR, command: UINT
	) -> BOOL;
    pub fn WidenPath(hdc: HDC) -> BOOL;
    // pub fn gdiPlaySpoolStream();
}
