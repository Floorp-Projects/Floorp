/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *  Patrick C. Beard <beard@netscape.com>
 */

#include "nsThemeHandler.h"
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsIComponentManager.h"
#include "nsNetUtil.h"
#include "nsIByteArrayInputStream.h"

#include <Appearance.h>
#include <ImageCompression.h>

#include <string>
#include <map>

/**
 * Represents the CGI argument list as an STL map<string, string>.
 */
typedef map<string, string> Arguments;

static nsresult parseArguments(const string& path, Arguments& args)
{
    // str should be of the form:  widget?name1=value1&...&nameN=valueN
    string::size_type questionMark = path.find('?');
    if (questionMark == string::npos) return NS_OK;
    
    string::size_type first = questionMark + 1;
    string::size_type equals = path.find('=', first);
    while (equals != string::npos) {
        string name(path.begin() + first, path.begin() + equals);
        string::size_type last = path.find('&', equals + 1);
        if (last == string::npos) last = path.length();
        string value(path.begin() + equals + 1, path.begin() + last);
        args[name] = value;
        first = last + 1;
        equals = path.find('=', first);
    }
    
    return NS_OK;
}

static const char* getArgument(const Arguments& args, const char* name, const char* defaultValue)
{
    Arguments::const_iterator i = args.find(name);
    if (i != args.end())
        return i->second.c_str();
    return defaultValue;
}

static int getIntArgument(const Arguments& args, const char* name, int defaultValue)
{
    const char* value = getArgument(args, name, NULL);
    if (value != NULL)
        return ::atoi(value);
    return defaultValue;
}

static bool getBoolArgument(const Arguments& args, const char* name, bool defaultValue)
{
    const char* value = getArgument(args, name, NULL);
    if (value != NULL)
        return (nsCRT::strcasecmp(value, "true") == 0);
    return defaultValue;
}

nsThemeHandler::nsThemeHandler()
{
    NS_INIT_ISUPPORTS();
}

NS_IMPL_ISUPPORTS1(nsThemeHandler, nsIProtocolHandler);

NS_METHOD
nsThemeHandler::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsCOMPtr<nsThemeHandler> themeHandler(new nsThemeHandler());
    return (themeHandler ? themeHandler->QueryInterface(aIID, aResult) : NS_ERROR_OUT_OF_MEMORY);
}

NS_IMETHODIMP
nsThemeHandler::GetScheme(char* *result)
{
    *result = nsCRT::strdup("theme");
    if (!*result) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsThemeHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;
    return NS_OK;
}

NS_IMETHODIMP
nsThemeHandler::NewURI(const char *aSpec, nsIURI *aBaseURI, nsIURI **result)
{
    nsresult rv;
    nsCOMPtr<nsIURI> url(do_CreateInstance("@mozilla.org/network/simple-uri;1", &rv));
    if (NS_FAILED(rv)) return rv;

    rv = url->SetSpec(aSpec);
    if (NS_FAILED(rv)) return rv;

    *result = url;
    NS_ADDREF(*result);
    
    return NS_OK;
}

/**
 * Quick & dirty little 32-bit deep GWorld wrapper, meant to be used within a single
 * block of code. After construction, the GWorld will be made the current port, and
 * all drawing will occur offscreen until the object is destructed.
 */
class TempGWorld {
public:
    TempGWorld(const Rect& bounds);
    ~TempGWorld();

    bool valid() { return mWorld != NULL; }
    operator GWorldPtr() { return mWorld; }

    long* begin() { return mPixels;  }
    long* end() { return mLimit; }

    void fill(long value)
    {
        for (long *pixels = mPixels, *limit = mLimit; pixels < limit; ++pixels)
            *pixels = value;
    }

    void xorFill(long value)
    {
        for (long *pixels = mPixels, *limit = mLimit; pixels < limit; ++pixels)
            *pixels ^= value;
    }

    void copy(TempGWorld& srcWorld, Rect& srcRect, const Rect& destRect, RgnHandle maskRgn = NULL);

private:
    GWorldPtr mWorld;
    PixMapHandle mPixmap;
    long* mPixels;
    long* mLimit;
    CGrafPtr mOldWorld;
    GDHandle mOldDevice;
};

TempGWorld::TempGWorld(const Rect& bounds)
    :   mWorld(0), mPixmap(0), mPixels(0), mLimit(0),
        mOldWorld(0), mOldDevice(0)
{
    OSStatus status;
    status = ::NewGWorld(&mWorld, 32, &bounds, NULL, NULL, useTempMem);
    if (status == noErr) {
        ::GetGWorld(&mOldWorld, &mOldDevice);
        mPixmap = ::GetGWorldPixMap(mWorld);
        ::LockPixels(mPixmap);
        mPixels = (long*) ::GetPixBaseAddr(mPixmap);
        mLimit = mPixels + (::GetPixRowBytes(mPixmap) / sizeof(long)) * (bounds.bottom - bounds.top);
        ::SetGWorld(mWorld, NULL);
    }
}

TempGWorld::~TempGWorld()
{
    if (mWorld) {
        ::SetGWorld(mOldWorld, mOldDevice);
        ::UnlockPixels(mPixmap);
        ::DisposeGWorld(mWorld);
    }
}

void TempGWorld::copy(TempGWorld& srcWorld, Rect& srcRect, const Rect& destRect, RgnHandle maskRgn)
{
    ::CopyBits((BitMap*)*srcWorld.mPixmap, (BitMap*)*mPixmap,
                &srcRect, &destRect, srcCopy, maskRgn);
}

class GraphicsExporter {
    GraphicsExportComponent mExporter;
public:
    GraphicsExporter(OSType type);
    ~GraphicsExporter();

    bool valid() { return mExporter != NULL; }
    
    OSStatus setInputGWorld(GWorldPtr gworld) { return ::GraphicsExportSetInputGWorld(mExporter, gworld); }
    OSStatus setOutputHandle(Handle output) { return ::GraphicsExportSetOutputHandle(mExporter, output); }
    OSStatus doExport() { return ::GraphicsExportDoExport(mExporter, NULL); }
};

GraphicsExporter::GraphicsExporter(OSType type)
{
    mExporter = ::OpenDefaultComponent(GraphicsExporterComponentType, type);
}

GraphicsExporter::~GraphicsExporter()
{
    if (mExporter) ::CloseComponent(mExporter);
}

static nsresult encodeGWorld(GWorldPtr world, OSType type, nsIInputStream **result, PRInt32 *length)
{
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    GraphicsExporter exporter(type);
    if (exporter.valid()) {
        Handle imageHandle = ::NewHandle(0);
        if (imageHandle != NULL) {
            if (exporter.setInputGWorld(world) == noErr &&
                exporter.setOutputHandle(imageHandle) == noErr &&
                exporter.doExport() == noErr) {
                UInt32 imageLength = ::GetHandleSize(imageHandle);
                char* buffer = (char*) nsMemory::Alloc(imageLength);
                if (buffer != NULL) {
                    nsCRT::memcpy(buffer, *imageHandle, imageLength);
                    nsCOMPtr<nsIByteArrayInputStream> stream;
                    rv = ::NS_NewByteArrayInputStream(getter_AddRefs(stream), buffer, imageLength);
                    if (NS_SUCCEEDED(rv)) {
                        *result = stream;
                        NS_ADDREF(*result);
                        *length = imageLength;
                    } else {
                        nsMemory::Free(buffer);
                    }
                }
            }
            ::DisposeHandle(imageHandle);
        }
    }
    return rv;
}

struct ButtonInfo {
    const char* title;
    Rect& bounds;
    ThemeButtonDrawInfo& drawInfo;
};

static void drawTitle(const Rect *bounds, ThemeButtonKind kind,
                      const ThemeButtonDrawInfo *info, UInt32 userData,
                      SInt16 depth, Boolean isColorDev)
{
    ButtonInfo* buttonInfo = reinterpret_cast<ButtonInfo*>(userData);
    const char* title = buttonInfo->title;
    if (title != NULL) {
        short titleLength = nsCRT::strlen(title);
        Rect textBounds;
        QDTextBounds(titleLength, title, &textBounds);
        FontInfo fontInfo;
        GetFontInfo(&fontInfo);

        // want to center the bounding box of the text
        // within the button's content bounding box.
        short textWidth = textBounds.right - textBounds.left;
        short textHeight = textBounds.bottom - textBounds.top;
        Rect contentBounds;
        GetThemeButtonContentBounds(&buttonInfo->bounds, kThemePushButton,
                                    &buttonInfo->drawInfo, &contentBounds);
        short contentWidth = contentBounds.right - contentBounds.left;
        short contentHeight = contentBounds.bottom - contentBounds.top;
        short x = contentBounds.left + (contentWidth - textWidth) / 2;
        short y = contentBounds.bottom - (contentHeight - textHeight) / 2;
        MoveTo(x, (y - fontInfo.leading));
        DrawText(title, 0, titleLength);
    }
}

static RoutineDescriptor drawTitleDescriptor = BUILD_ROUTINE_DESCRIPTOR(uppThemeButtonDrawProcInfo, &drawTitle);
static ThemeButtonDrawUPP drawTitleUPP = ThemeButtonDrawUPP(&drawTitleDescriptor);

static nsresult drawThemeButton(Arguments& args, nsIInputStream **result, PRInt32 *length)
{
    int width = getIntArgument(args, "width", 40);
    int height = getIntArgument(args, "height", 20);
    bool isActive = getBoolArgument(args, "active", true);
    bool isPressed = getBoolArgument(args, "pressed", false);
    bool isDefault = getBoolArgument(args, "default", false);
    const char* title = getArgument(args, "title", NULL);

    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    OSStatus status;
    
    ThemeButtonDrawInfo drawInfo = {
        (isActive ? (isPressed ? kThemeStatePressed : kThemeStateActive) : kThemeStateInactive),
        kThemeButtonOn,
        (isDefault ? kThemeAdornmentDefault : 0)
    };

    Rect buttonBounds = { 0, 0, height, width }, backgroundBounds;
    status = ::GetThemeButtonBackgroundBounds(&buttonBounds, kThemePushButton, &drawInfo, &backgroundBounds);
    if (status == noErr) {
        width = backgroundBounds.right - backgroundBounds.left;
        height = backgroundBounds.bottom - backgroundBounds.top;
        short dx = (buttonBounds.left - backgroundBounds.left);
        short dy = (buttonBounds.top - backgroundBounds.top);
        OffsetRect(&buttonBounds, dx, dy);
        SetRect(&backgroundBounds, 0, 0, width, height);
    } else {
        return rv;
    }
    
    TempGWorld world(backgroundBounds);
    if (world.valid()) {
        // initialize the GWorld with all black, alpha=0xFF.
        world.fill(0xFF000000);
        
        ButtonInfo buttonInfo = {
            title, buttonBounds, drawInfo
        };
        
        status = ::DrawThemeButton(&buttonBounds, kThemePushButton, &drawInfo,
                                   NULL, NULL, drawTitleUPP,
                                   reinterpret_cast<UInt32>(&buttonInfo));

        // now, for all pixels that aren't 0xFF000000, turn on the alpha channel,
        // otherwise turn it off on the pixels that weren't touched.
        world.xorFill(0xFF000000);
        
        // now, encode the image as a 'PNGf' image, and return the encoded image
        // as an nsIInputStream.
        rv = encodeGWorld(world, 'PNGf', result, length);
    }
    return rv;
}

static nsresult drawThemeScrollbar(Arguments& args, nsIInputStream **result, PRInt32 *length)
{
    bool isHorizontal = getBoolArgument(args, "horizontal", true);
    int width = getIntArgument(args, "width", (isHorizontal ? 200 : 16));
    int height = getIntArgument(args, "height", (isHorizontal ? 16 : 200));
    int min = getIntArgument(args, "min", 0);
    int max = getIntArgument(args, "max", 255);
    int value = getIntArgument(args, "value", 127);
    int viewSize = getIntArgument(args, "viewSize", 0);
    bool isActive = getBoolArgument(args, "active", true);
    bool isThumbPressed = getBoolArgument(args, "thumb", false);
    bool isLeftArrowPressed = getBoolArgument(args, "left", false);
    bool isRightArrowPressed = getBoolArgument(args, "right", false);

    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    OSStatus status;
    
    ThemeTrackEnableState enableState = (isActive ? kThemeTrackActive : kThemeTrackInactive);
    ThemeTrackPressState pressState = ((isThumbPressed ? kThemeThumbPressed : 0) |
                                       (isRightArrowPressed ? (kThemeRightOutsideArrowPressed | kThemeLeftInsideArrowPressed) : 0) |
                                       (isLeftArrowPressed ? (kThemeLeftOutsideArrowPressed | kThemeRightInsideArrowPressed) : 0));
    ThemeTrackAttributes trackState = ((isHorizontal ? kThemeTrackHorizontal : 0) | kThemeTrackShowThumb);
    Rect scrollbarBounds = { 0, 0, (short) height , (short) width };

    TempGWorld world(scrollbarBounds);
    if (world.valid()) {
        // initialize the GWorld with all black, alpha=0xFF.
        world.fill(0xFF000000);

        ThemeTrackDrawInfo drawInfo;
        status = ::DrawThemeScrollBarArrows(&scrollbarBounds, enableState, pressState, isHorizontal, &drawInfo.bounds);
        drawInfo.kind = kThemeScrollBar;
        drawInfo.min = min, drawInfo.max = max, drawInfo.value = value;
        drawInfo.reserved = 0;
        drawInfo.attributes = trackState;
        drawInfo.enableState = enableState;
        drawInfo.trackInfo.scrollbar.viewsize = viewSize;
        drawInfo.trackInfo.scrollbar.pressState = pressState;
        status = ::DrawThemeTrack(&drawInfo, NULL, NULL, 0);

        // now, encode the image as a 'PNGf' image, and return the encoded image
        // as an nsIInputStream.
        const char* part = getArgument(args, "part", NULL);
        if (part != NULL && nsCRT::strcasecmp(part, "thumb") == 0) {
            RgnHandle thumbRgn = ::NewRgn();
            if (thumbRgn != NULL) {
                status = ::GetThemeTrackThumbRgn(&drawInfo, thumbRgn);
                Rect srcBounds = (**thumbRgn).rgnBBox;
                Rect thumbBounds = { 0, 0, srcBounds.bottom - srcBounds.top, srcBounds.right - srcBounds.left };
                ::OffsetRgn(thumbRgn, -srcBounds.left, -srcBounds.top);
                TempGWorld thumbWorld(thumbBounds);
                if (thumbWorld.valid()) {
                    thumbWorld.fill(0xFF000000);
                    thumbWorld.copy(world, srcBounds, thumbBounds, thumbRgn);
                    thumbWorld.xorFill(0xFF000000);
                    rv = encodeGWorld(thumbWorld, 'PNGf', result, length);
                }
                ::DisposeRgn(thumbRgn);
            }
        } else {
            // now, for all pixels that aren't 0xFF000000, turn on the alpha channel,
            // otherwise turn it off on the pixels that weren't touched.
            world.xorFill(0xFF000000);        
            rv = encodeGWorld(world, 'PNGf', result, length);
        }
    }
    
    return rv;
}

/*
    1.  Parse the URL path, which will be of the form:
        theme:widget?width=40&height=20&title=OK
    2.  Generate an image of a widget in an offscreen GWorld, as specified by the URL.
    3.  Encode the image as a PNG file, and return that via nsIInputStream/nsIChannel.
 */
NS_IMETHODIMP
nsThemeHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
    nsresult rv;
    Arguments args;
    string action;
    {
        nsXPIDLCString buffer;
        rv = url->GetPath(getter_Copies(buffer));
        if (NS_FAILED(rv)) return rv;
        string path(buffer.get());
        rv = parseArguments(path, args);
        if (NS_FAILED(rv)) return rv;
        string::size_type questionMark = path.find('?');
        if (questionMark != string::npos) {
            action.resize(questionMark);
            copy(path.begin(), path.begin() + questionMark, action.begin());
        } else {
            action = path;
        }
    }
    
    PRInt32 contentLength = 0;
    nsCOMPtr<nsIInputStream> input;

    if (action == "button") {
        rv = drawThemeButton(args, getter_AddRefs(input), &contentLength);
    } else if (action == "scrollbar") {
        rv = drawThemeScrollbar(args, getter_AddRefs(input), &contentLength);
    } else {
        rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    if (NS_FAILED(rv)) return rv;
    
    return NS_NewInputStreamChannel(result, url, input, "image/png", contentLength);
}
