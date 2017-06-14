/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "HeadlessClipboard.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsIServiceManager.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/TimeStamp.h"

#include "imgIContainer.h"

#include <gtk/gtk.h>

// For manipulation of the X event queue
#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include "X11UndefineNone.h"

#include "mozilla/Encoding.h"

#include "gfxPlatform.h"

using namespace mozilla;

// Callback when someone asks us for the data
void
clipboard_get_cb(GtkClipboard *aGtkClipboard,
                 GtkSelectionData *aSelectionData,
                 guint info,
                 gpointer user_data);

// Callback when someone asks us to clear a clipboard
void
clipboard_clear_cb(GtkClipboard *aGtkClipboard,
                   gpointer user_data);
                   
static void
ConvertHTMLtoUCS2          (guchar             *data,
                            int32_t             dataLength,
                            char16_t         **unicodeData,
                            int32_t            &outUnicodeLen);

static void
GetHTMLCharset             (guchar * data, int32_t dataLength, nsCString& str);


// Our own versions of gtk_clipboard_wait_for_contents and
// gtk_clipboard_wait_for_text, which don't run the event loop while
// waiting for the data.  This prevents a lot of problems related to
// dispatching events at unexpected times.

static GtkSelectionData *
wait_for_contents          (GtkClipboard *clipboard, GdkAtom target);

static gchar *
wait_for_text              (GtkClipboard *clipboard);

static GdkFilterReturn
selection_request_filter   (GdkXEvent *gdk_xevent,
                            GdkEvent *event,
                            gpointer data);

namespace mozilla {
namespace clipboard {
StaticRefPtr<nsIClipboard> sInstance;
}
}
/* static */ already_AddRefed<nsIClipboard>
nsClipboard::GetInstance()
{
    using namespace mozilla::clipboard;

    if (!sInstance) {
        if (gfxPlatform::IsHeadless()) {
            sInstance = new widget::HeadlessClipboard();
        } else {
            RefPtr<nsClipboard> clipboard = new nsClipboard();
            nsresult rv = clipboard->Init();
            if (NS_FAILED(rv)) {
                return nullptr;
            }
            sInstance = clipboard.forget();
        }
        ClearOnShutdown(&sInstance);
    }

    RefPtr<nsIClipboard> service = sInstance.get();
    return service.forget();
}

nsClipboard::nsClipboard()
{
}

nsClipboard::~nsClipboard()
{
    // We have to clear clipboard before gdk_display_close() call.
    // See bug 531580 for details.
    if (mGlobalTransferable) {
        gtk_clipboard_clear(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
    }
    if (mSelectionTransferable) {
        gtk_clipboard_clear(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
    }
}

NS_IMPL_ISUPPORTS(nsClipboard, nsIClipboard)

nsresult
nsClipboard::Init(void)
{
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (!os)
      return NS_ERROR_FAILURE;

    os->AddObserver(this, "quit-application", false);

    // A custom event filter to workaround attempting to dereference a null
    // selection requestor in GTK3 versions before 3.11.3. See bug 1178799.
#if (MOZ_WIDGET_GTK == 3) && defined(MOZ_X11)
    if (gtk_check_version(3, 11, 3))
        gdk_window_add_filter(nullptr, selection_request_filter, nullptr);
#endif

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::Observe(nsISupports *aSubject, const char *aTopic, const char16_t *aData)
{
    if (strcmp(aTopic, "quit-application") == 0) {
        // application is going to quit, save clipboard content
        Store();
        gdk_window_remove_filter(nullptr, selection_request_filter, nullptr);
    }
    return NS_OK;
}

nsresult
nsClipboard::Store(void)
{
    // Ask the clipboard manager to store the current clipboard content
    if (mGlobalTransferable) {
        GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_store(clipboard);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SetData(nsITransferable *aTransferable,
                     nsIClipboardOwner *aOwner, int32_t aWhichClipboard)
{
    // See if we can short cut
    if ((aWhichClipboard == kGlobalClipboard &&
         aTransferable == mGlobalTransferable.get() &&
         aOwner == mGlobalOwner.get()) ||
        (aWhichClipboard == kSelectionClipboard &&
         aTransferable == mSelectionTransferable.get() &&
         aOwner == mSelectionOwner.get())) {
        return NS_OK;
    }

    // Clear out the clipboard in order to set the new data
    EmptyClipboard(aWhichClipboard);

    // List of suported targets
    GtkTargetList *list = gtk_target_list_new(nullptr, 0);

    // Get the types of supported flavors
    nsCOMPtr<nsIArray> flavors;

    nsresult rv =
        aTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavors));
    if (!flavors || NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    // Add all the flavors to this widget's supported type.
    bool imagesAdded = false;
    uint32_t count;
    flavors->GetLength(&count);
    for (uint32_t i=0; i < count; i++) {
        nsCOMPtr<nsISupportsCString> flavor = do_QueryElementAt(flavors, i);

        if (flavor) {
            nsXPIDLCString flavorStr;
            flavor->ToString(getter_Copies(flavorStr));

            // special case text/unicode since we can handle all of
            // the string types
            if (!strcmp(flavorStr, kUnicodeMime)) {
                gtk_target_list_add(list, gdk_atom_intern("UTF8_STRING", FALSE), 0, 0);
                gtk_target_list_add(list, gdk_atom_intern("COMPOUND_TEXT", FALSE), 0, 0);
                gtk_target_list_add(list, gdk_atom_intern("TEXT", FALSE), 0, 0);
                gtk_target_list_add(list, GDK_SELECTION_TYPE_STRING, 0, 0);
                continue;
            }

            if (flavorStr.EqualsLiteral(kNativeImageMime) ||
                flavorStr.EqualsLiteral(kPNGImageMime) ||
                flavorStr.EqualsLiteral(kJPEGImageMime) ||
                flavorStr.EqualsLiteral(kJPGImageMime) ||
                flavorStr.EqualsLiteral(kGIFImageMime)) {
                // don't bother adding image targets twice
                if (!imagesAdded) {
                    // accept any writable image type
                    gtk_target_list_add_image_targets(list, 0, TRUE);
                    imagesAdded = true;
                }
                continue;
            }

            // Add this to our list of valid targets
            GdkAtom atom = gdk_atom_intern(flavorStr, FALSE);
            gtk_target_list_add(list, atom, 0, 0);
        }
    }
    
    // Get GTK clipboard (CLIPBOARD or PRIMARY)
    GtkClipboard *gtkClipboard = gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));
  
    gint numTargets;
    GtkTargetEntry *gtkTargets = gtk_target_table_new_from_list(list, &numTargets);
          
    // Set getcallback and request to store data after an application exit
    if (gtkTargets &&
        gtk_clipboard_set_with_data(gtkClipboard, gtkTargets, numTargets,
                                    clipboard_get_cb, clipboard_clear_cb, this))
    {
        // We managed to set-up the clipboard so update internal state
        // We have to set it now because gtk_clipboard_set_with_data() calls clipboard_clear_cb()
        // which reset our internal state 
        if (aWhichClipboard == kSelectionClipboard) {
            mSelectionOwner = aOwner;
            mSelectionTransferable = aTransferable;
        }
        else {
            mGlobalOwner = aOwner;
            mGlobalTransferable = aTransferable;
            gtk_clipboard_set_can_store(gtkClipboard, gtkTargets, numTargets);
        }

        rv = NS_OK;
    }
    else {  
        rv = NS_ERROR_FAILURE;
    }

    gtk_target_table_free(gtkTargets, numTargets);
    gtk_target_list_unref(list);
  
    return rv;
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable *aTransferable, int32_t aWhichClipboard)
{
    if (!aTransferable)
        return NS_ERROR_FAILURE;

    GtkClipboard *clipboard;
    clipboard = gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

    guchar        *data = nullptr;
    gint           length = 0;
    bool           foundData = false;
    nsAutoCString  foundFlavor;

    // Get a list of flavors this transferable can import
    nsCOMPtr<nsIArray> flavors;
    nsresult rv;
    rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavors));
    if (!flavors || NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    uint32_t count;
    flavors->GetLength(&count);
    for (uint32_t i=0; i < count; i++) {
        nsCOMPtr<nsISupportsCString> currentFlavor;
        currentFlavor = do_QueryElementAt(flavors, i);

        if (currentFlavor) {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString(getter_Copies(flavorStr));

            // Special case text/unicode since we can convert any
            // string into text/unicode
            if (!strcmp(flavorStr, kUnicodeMime)) {
                gchar* new_text = wait_for_text(clipboard);
                if (new_text) {
                    // Convert utf-8 into our unicode format.
                    NS_ConvertUTF8toUTF16 ucs2string(new_text);
                    data = (guchar *)ToNewUnicode(ucs2string);
                    length = ucs2string.Length() * 2;
                    g_free(new_text);
                    foundData = true;
                    foundFlavor = kUnicodeMime;
                    break;
                }
                // If the type was text/unicode and we couldn't get
                // text off the clipboard, run the next loop
                // iteration.
                continue;
            }

            // For images, we must wrap the data in an nsIInputStream then return instead of break,
            // because that code below won't help us.
            if (!strcmp(flavorStr, kJPEGImageMime) ||
                !strcmp(flavorStr, kJPGImageMime) ||
                !strcmp(flavorStr, kPNGImageMime) ||
                !strcmp(flavorStr, kGIFImageMime)) {
                // Emulate support for image/jpg
                if (!strcmp(flavorStr, kJPGImageMime)) {
                    flavorStr.Assign(kJPEGImageMime);
                }

                GdkAtom atom = gdk_atom_intern(flavorStr, FALSE);

                GtkSelectionData *selectionData = wait_for_contents(clipboard, atom);
                if (!selectionData)
                    continue;

                nsCOMPtr<nsIInputStream> byteStream;
                NS_NewByteInputStream(getter_AddRefs(byteStream), 
                                      (const char*)gtk_selection_data_get_data(selectionData),
                                      gtk_selection_data_get_length(selectionData), 
                                      NS_ASSIGNMENT_COPY);
                aTransferable->SetTransferData(flavorStr, byteStream, sizeof(nsIInputStream*));
                gtk_selection_data_free(selectionData);
                return NS_OK;
            }

            // Get the atom for this type and try to request it off
            // the clipboard.
            GdkAtom atom = gdk_atom_intern(flavorStr, FALSE);
            GtkSelectionData *selectionData;
            selectionData = wait_for_contents(clipboard, atom);
            if (selectionData) {
                const guchar *clipboardData = gtk_selection_data_get_data(selectionData);
                length = gtk_selection_data_get_length(selectionData);
                // Special case text/html since we can convert into UCS2
                if (!strcmp(flavorStr, kHTMLMime)) {
                    char16_t* htmlBody= nullptr;
                    int32_t htmlBodyLen = 0;
                    // Convert text/html into our unicode format
                    ConvertHTMLtoUCS2(const_cast<guchar*>(clipboardData), length,
                                      &htmlBody, htmlBodyLen);
                    // Try next data format?
                    if (!htmlBodyLen)
                        continue;
                    data = (guchar *)htmlBody;
                    length = htmlBodyLen * 2;
                } else {
                    data = (guchar *)moz_xmalloc(length);
                    if (!data)
                        break;
                    memcpy(data, clipboardData, length);
                }
                gtk_selection_data_free(selectionData);
                foundData = true;
                foundFlavor = flavorStr;
                break;
            }
        }
    }

    if (foundData) {
        nsCOMPtr<nsISupports> wrapper;
        nsPrimitiveHelpers::CreatePrimitiveForData(foundFlavor.get(),
                                                   data, length,
                                                   getter_AddRefs(wrapper));
        aTransferable->SetTransferData(foundFlavor.get(),
                                       wrapper, length);
    }

    if (data)
        free(data);

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard)
{
    if (aWhichClipboard == kSelectionClipboard) {
        if (mSelectionOwner) {
            mSelectionOwner->LosingOwnership(mSelectionTransferable);
            mSelectionOwner = nullptr;
        }
        mSelectionTransferable = nullptr;
    }
    else {
        if (mGlobalOwner) {
            mGlobalOwner->LosingOwnership(mGlobalTransferable);
            mGlobalOwner = nullptr;
        }
        mGlobalTransferable = nullptr;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char** aFlavorList, uint32_t aLength,
                                    int32_t aWhichClipboard, bool *_retval)
{
    if (!aFlavorList || !_retval)
        return NS_ERROR_NULL_POINTER;

    *_retval = false;

    GtkSelectionData *selection_data =
        GetTargets(GetSelectionAtom(aWhichClipboard));
    if (!selection_data)
        return NS_OK;

    gint n_targets = 0;
    GdkAtom *targets = nullptr;

    if (!gtk_selection_data_get_targets(selection_data, 
                                        &targets, &n_targets) ||
        !n_targets)
        return NS_OK;

    // Walk through the provided types and try to match it to a
    // provided type.
    for (uint32_t i = 0; i < aLength && !*_retval; i++) {
        // We special case text/unicode here.
        if (!strcmp(aFlavorList[i], kUnicodeMime) && 
            gtk_selection_data_targets_include_text(selection_data)) {
            *_retval = true;
            break;
        }

        for (int32_t j = 0; j < n_targets; j++) {
            gchar *atom_name = gdk_atom_name(targets[j]);
            if (!atom_name)
                continue;

            if (!strcmp(atom_name, aFlavorList[i]))
                *_retval = true;

            // X clipboard supports image/jpeg, but we want to emulate support
            // for image/jpg as well
            if (!strcmp(aFlavorList[i], kJPGImageMime) && !strcmp(atom_name, kJPEGImageMime))
                *_retval = true;

            g_free(atom_name);

            if (*_retval)
                break;
        }
    }
    gtk_selection_data_free(selection_data);
    g_free(targets);

    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool *_retval)
{
    *_retval = true; // yeah, unix supports the selection clipboard
    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool* _retval)
{
  *_retval = false;
  return NS_OK;
}

/* static */
GdkAtom
nsClipboard::GetSelectionAtom(int32_t aWhichClipboard)
{
    if (aWhichClipboard == kGlobalClipboard)
        return GDK_SELECTION_CLIPBOARD;

    return GDK_SELECTION_PRIMARY;
}

/* static */
GtkSelectionData *
nsClipboard::GetTargets(GdkAtom aWhichClipboard)
{
    GtkClipboard *clipboard = gtk_clipboard_get(aWhichClipboard);
    return wait_for_contents(clipboard, gdk_atom_intern("TARGETS", FALSE));
}

nsITransferable *
nsClipboard::GetTransferable(int32_t aWhichClipboard)
{
    nsITransferable *retval;

    if (aWhichClipboard == kSelectionClipboard)
        retval = mSelectionTransferable.get();
    else
        retval = mGlobalTransferable.get();
        
    return retval;
}

void
nsClipboard::SelectionGetEvent(GtkClipboard     *aClipboard,
                               GtkSelectionData *aSelectionData)
{
    // Someone has asked us to hand them something.  The first thing
    // that we want to do is see if that something includes text.  If
    // it does, try to give it text/unicode after converting it to
    // utf-8.

    int32_t whichClipboard;

    // which clipboard?
    GdkAtom selection = gtk_selection_data_get_selection(aSelectionData);
    if (selection == GDK_SELECTION_PRIMARY)
        whichClipboard = kSelectionClipboard;
    else if (selection == GDK_SELECTION_CLIPBOARD)
        whichClipboard = kGlobalClipboard;
    else
        return; // THAT AIN'T NO CLIPBOARD I EVER HEARD OF

    nsCOMPtr<nsITransferable> trans = GetTransferable(whichClipboard);
    if (!trans) {
      // We have nothing to serve
#ifdef DEBUG_CLIPBOARD
      printf("nsClipboard::SelectionGetEvent() - %s clipboard is empty!\n",
             whichClipboard == kSelectionClipboard ? "Selection" : "Global");
#endif
      return;
    }

    nsresult rv;
    nsCOMPtr<nsISupports> item;
    uint32_t len;

    GdkAtom selectionTarget = gtk_selection_data_get_target(aSelectionData);

    // Check to see if the selection data includes any of the string
    // types that we support.
    if (selectionTarget == gdk_atom_intern ("STRING", FALSE) ||
        selectionTarget == gdk_atom_intern ("TEXT", FALSE) ||
        selectionTarget == gdk_atom_intern ("COMPOUND_TEXT", FALSE) ||
        selectionTarget == gdk_atom_intern ("UTF8_STRING", FALSE)) {
        // Try to convert our internal type into a text string.  Get
        // the transferable for this clipboard and try to get the
        // text/unicode type for it.
        rv = trans->GetTransferData("text/unicode", getter_AddRefs(item),
                                    &len);
        if (!item || NS_FAILED(rv))
            return;
        
        nsCOMPtr<nsISupportsString> wideString;
        wideString = do_QueryInterface(item);
        if (!wideString)
            return;

        nsAutoString ucs2string;
        wideString->GetData(ucs2string);
        char *utf8string = ToNewUTF8String(ucs2string);
        if (!utf8string)
            return;
        
        gtk_selection_data_set_text (aSelectionData, utf8string,
                                     strlen(utf8string));

        free(utf8string);
        return;
    }

    // Check to see if the selection data is an image type
    if (gtk_targets_include_image(&selectionTarget, 1, TRUE)) {
        // Look through our transfer data for the image
        static const char* const imageMimeTypes[] = {
            kNativeImageMime, kPNGImageMime, kJPEGImageMime, kJPGImageMime, kGIFImageMime };
        nsCOMPtr<nsISupports> imageItem;
        nsCOMPtr<nsISupportsInterfacePointer> ptrPrimitive;
        for (uint32_t i = 0; !ptrPrimitive && i < ArrayLength(imageMimeTypes); i++) {
            rv = trans->GetTransferData(imageMimeTypes[i], getter_AddRefs(imageItem), &len);
            ptrPrimitive = do_QueryInterface(imageItem);
        }
        if (!ptrPrimitive)
            return;

        nsCOMPtr<nsISupports> primitiveData;
        ptrPrimitive->GetData(getter_AddRefs(primitiveData));
        nsCOMPtr<imgIContainer> image(do_QueryInterface(primitiveData));
        if (!image) // Not getting an image for an image mime type!?
            return;

        GdkPixbuf* pixbuf = nsImageToPixbuf::ImageToPixbuf(image);
        if (!pixbuf)
            return;

        gtk_selection_data_set_pixbuf(aSelectionData, pixbuf);
        g_object_unref(pixbuf);
        return;
    }

    // Try to match up the selection data target to something our
    // transferable provides.
    gchar *target_name = gdk_atom_name(selectionTarget);
    if (!target_name)
        return;

    rv = trans->GetTransferData(target_name, getter_AddRefs(item), &len);
    // nothing found?
    if (!item || NS_FAILED(rv)) {
        g_free(target_name);
        return;
    }

    void *primitive_data = nullptr;
    nsPrimitiveHelpers::CreateDataFromPrimitive(target_name, item,
                                                &primitive_data, len);

    if (primitive_data) {
        // Check to see if the selection data is text/html
        if (selectionTarget == gdk_atom_intern (kHTMLMime, FALSE)) {
            /*
             * "text/html" can be encoded UCS2. It is recommended that
             * documents transmitted as UCS2 always begin with a ZERO-WIDTH
             * NON-BREAKING SPACE character (hexadecimal FEFF, also called
             * Byte Order Mark (BOM)). Adding BOM can help other app to
             * detect mozilla use UCS2 encoding when copy-paste.
             */
            guchar *buffer = (guchar *)
                    moz_xmalloc((len * sizeof(guchar)) + sizeof(char16_t));
            if (!buffer)
                return;
            char16_t prefix = 0xFEFF;
            memcpy(buffer, &prefix, sizeof(prefix));
            memcpy(buffer + sizeof(prefix), primitive_data, len);
            free((guchar *)primitive_data);
            primitive_data = (guchar *)buffer;
            len += sizeof(prefix);
        }
  
        gtk_selection_data_set(aSelectionData, selectionTarget,
                               8, /* 8 bits in a unit */
                               (const guchar *)primitive_data, len);
        free(primitive_data);
    }

    g_free(target_name);
                           
}

void
nsClipboard::SelectionClearEvent(GtkClipboard *aGtkClipboard)
{
    int32_t whichClipboard;

    // which clipboard?
    if (aGtkClipboard == gtk_clipboard_get(GDK_SELECTION_PRIMARY))
        whichClipboard = kSelectionClipboard;
    else if (aGtkClipboard == gtk_clipboard_get(GDK_SELECTION_CLIPBOARD))
        whichClipboard = kGlobalClipboard;
    else
        return; // THAT AIN'T NO CLIPBOARD I EVER HEARD OF

    EmptyClipboard(whichClipboard);
}

void
clipboard_get_cb(GtkClipboard *aGtkClipboard,
                 GtkSelectionData *aSelectionData,
                 guint info,
                 gpointer user_data)
{
    nsClipboard *aClipboard = static_cast<nsClipboard *>(user_data);
    aClipboard->SelectionGetEvent(aGtkClipboard, aSelectionData);
}

void
clipboard_clear_cb(GtkClipboard *aGtkClipboard,
                   gpointer user_data)
{
    nsClipboard *aClipboard = static_cast<nsClipboard *>(user_data);
    aClipboard->SelectionClearEvent(aGtkClipboard);
}

/*
 * when copy-paste, mozilla wants data encoded using UCS2,
 * other app such as StarOffice use "text/html"(RFC2854).
 * This function convert data(got from GTK clipboard)
 * to data mozilla wanted.
 *
 * data from GTK clipboard can be 3 forms:
 *  1. From current mozilla
 *     "text/html", charset = utf-16
 *  2. From old version mozilla or mozilla-based app
 *     content("body" only), charset = utf-16
 *  3. From other app who use "text/html" when copy-paste
 *     "text/html", has "charset" info
 *
 * data      : got from GTK clipboard
 * dataLength: got from GTK clipboard
 * body      : pass to Mozilla
 * bodyLength: pass to Mozilla
 */
void ConvertHTMLtoUCS2(guchar * data, int32_t dataLength,
                       char16_t** unicodeData, int32_t& outUnicodeLen)
{
    nsAutoCString charset;
    GetHTMLCharset(data, dataLength, charset);// get charset of HTML
    if (charset.EqualsLiteral("UTF-16")) {//current mozilla
        outUnicodeLen = (dataLength / 2) - 1;
        *unicodeData = reinterpret_cast<char16_t*>
                                       (moz_xmalloc((outUnicodeLen + sizeof('\0')) *
                       sizeof(char16_t)));
        if (*unicodeData) {
            memcpy(*unicodeData, data + sizeof(char16_t),
                   outUnicodeLen * sizeof(char16_t));
            (*unicodeData)[outUnicodeLen] = '\0';
        }
    } else if (charset.EqualsLiteral("UNKNOWN")) {
        outUnicodeLen = 0;
        return;
    } else {
        // app which use "text/html" to copy&paste
        // get the decoder
        auto encoding = Encoding::ForLabelNoReplacement(charset);
        if (!encoding) {
#ifdef DEBUG_CLIPBOARD
            g_print("        get unicode decoder error\n");
#endif
            outUnicodeLen = 0;
            return;
        }
        auto decoder = encoding->NewDecoder();
        CheckedInt<size_t> needed = decoder->MaxUTF16BufferLength(dataLength);
        if (!needed.isValid() || needed.value() > INT32_MAX) {
          outUnicodeLen = 0;
          return;
        }

        outUnicodeLen = 0;
        if (needed.value()) {
          *unicodeData = reinterpret_cast<char16_t*>(
            moz_xmalloc((needed.value() + 1) * sizeof(char16_t)));
          if (*unicodeData) {
            uint32_t result;
            size_t read;
            size_t written;
            bool hadErrors;
            Tie(result, read, written, hadErrors) =
              decoder->DecodeToUTF16(AsBytes(MakeSpan(data, dataLength)),
                                     MakeSpan(*unicodeData, needed.value()),
                                     true);
            MOZ_ASSERT(result == kInputEmpty);
            MOZ_ASSERT(read == size_t(dataLength));
            MOZ_ASSERT(written <= needed.value());
            Unused << hadErrors;
#ifdef DEBUG_CLIPBOARD
            if (read != dataLength)
              printf("didn't consume all the bytes\n");
#endif
            outUnicodeLen = written;
            // null terminate.
            (*unicodeData)[outUnicodeLen] = '\0';
            }
        } // if valid length
    }
}

/*
 * get "charset" information from clipboard data
 * return value can be:
 *  1. "UTF-16":      mozilla or "text/html" with "charset=utf-16"
 *  2. "UNKNOWN":     mozilla can't detect what encode it use
 *  3. other:         "text/html" with other charset than utf-16
 */
void GetHTMLCharset(guchar * data, int32_t dataLength, nsCString& str)
{
    // if detect "FFFE" or "FEFF", assume UTF-16
    char16_t* beginChar =  (char16_t*)data;
    if ((beginChar[0] == 0xFFFE) || (beginChar[0] == 0xFEFF)) {
        str.AssignLiteral("UTF-16");
        return;
    }
    // no "FFFE" and "FEFF", assume ASCII first to find "charset" info
    const nsDependentCString htmlStr((const char *)data, dataLength);
    nsACString::const_iterator start, end;
    htmlStr.BeginReading(start);
    htmlStr.EndReading(end);
    nsACString::const_iterator valueStart(start), valueEnd(start);

    if (CaseInsensitiveFindInReadable(
        NS_LITERAL_CSTRING("CONTENT=\"text/html;"),
        start, end)) {
        start = end;
        htmlStr.EndReading(end);

        if (CaseInsensitiveFindInReadable(
            NS_LITERAL_CSTRING("charset="),
            start, end)) {
            valueStart = end;
            start = end;
            htmlStr.EndReading(end);
          
            if (FindCharInReadable('"', start, end))
                valueEnd = start;
        }
    }
    // find "charset" in HTML
    if (valueStart != valueEnd) {
        str = Substring(valueStart, valueEnd);
        ToUpperCase(str);
#ifdef DEBUG_CLIPBOARD
        printf("Charset of HTML = %s\n", charsetUpperStr.get());
#endif
        return;
    }
    str.AssignLiteral("UNKNOWN");
}

static void
DispatchSelectionNotifyEvent(GtkWidget *widget, XEvent *xevent)
{
    GdkEvent event;
    event.selection.type = GDK_SELECTION_NOTIFY;
    event.selection.window = gtk_widget_get_window(widget);
    event.selection.selection = gdk_x11_xatom_to_atom(xevent->xselection.selection);
    event.selection.target = gdk_x11_xatom_to_atom(xevent->xselection.target);
    event.selection.property = gdk_x11_xatom_to_atom(xevent->xselection.property);
    event.selection.time = xevent->xselection.time;

    gtk_widget_event(widget, &event);
}

static void
DispatchPropertyNotifyEvent(GtkWidget *widget, XEvent *xevent)
{
    GdkWindow *window = gtk_widget_get_window(widget);
    if ((gdk_window_get_events(window)) & GDK_PROPERTY_CHANGE_MASK) {
        GdkEvent event;
        event.property.type = GDK_PROPERTY_NOTIFY;
        event.property.window = window;
        event.property.atom = gdk_x11_xatom_to_atom(xevent->xproperty.atom);
        event.property.time = xevent->xproperty.time;
        event.property.state = xevent->xproperty.state;

        gtk_widget_event(widget, &event);
    }
}

struct checkEventContext
{
    GtkWidget *cbWidget;
    Atom       selAtom;
};

static Bool
checkEventProc(Display *display, XEvent *event, XPointer arg)
{
    checkEventContext *context = (checkEventContext *) arg;

    if (event->xany.type == SelectionNotify ||
        (event->xany.type == PropertyNotify &&
         event->xproperty.atom == context->selAtom)) {

        GdkWindow *cbWindow = 
            gdk_x11_window_lookup_for_display(gdk_x11_lookup_xdisplay(display),
                                              event->xany.window);
        if (cbWindow) {
            GtkWidget *cbWidget = nullptr;
            gdk_window_get_user_data(cbWindow, (gpointer *)&cbWidget);
            if (cbWidget && GTK_IS_WIDGET(cbWidget)) {
                context->cbWidget = cbWidget;
                return True;
            }
        }
    }

    return False;
}

// Idle timeout for receiving selection and property notify events (microsec)
static const int kClipboardTimeout = 500000;

static gchar* CopyRetrievedData(const gchar *aData)
{
    return g_strdup(aData);
}

static GtkSelectionData* CopyRetrievedData(GtkSelectionData *aData)
{
    // A negative length indicates that retrieving the data failed.
    return gtk_selection_data_get_length(aData) >= 0 ?
        gtk_selection_data_copy(aData) : nullptr;
}

class RetrievalContext {
    ~RetrievalContext()
    {
        MOZ_ASSERT(!mData, "Wait() wasn't called");
    }

public:
    NS_INLINE_DECL_REFCOUNTING(RetrievalContext)
    enum State { INITIAL, COMPLETED, TIMED_OUT };

    RetrievalContext() : mState(INITIAL), mData(nullptr) {}

    /**
     * Call this when data has been retrieved.
     */
    template <class T> void Complete(T *aData)
    {
        if (mState == INITIAL) {
            mState = COMPLETED;
            mData = CopyRetrievedData(aData);
        } else {
            // Already timed out
            MOZ_ASSERT(mState == TIMED_OUT);
        }
    }

    /**
     * Spins X event loop until timing out or being completed. Returns
     * null if we time out, otherwise returns the completed data (passing
     * ownership to caller).
     */
    void *Wait();

protected:
    State mState;
    void* mData;
};

void *
RetrievalContext::Wait()
{
    if (mState == COMPLETED) { // the request completed synchronously
        void *data = mData;
        mData = nullptr;
        return data;
    }

    GdkDisplay *gdkDisplay = gdk_display_get_default();
    if (GDK_IS_X11_DISPLAY(gdkDisplay)) {
        Display *xDisplay = GDK_DISPLAY_XDISPLAY(gdkDisplay);
        checkEventContext context;
        context.cbWidget = nullptr;
        context.selAtom = gdk_x11_atom_to_xatom(gdk_atom_intern("GDK_SELECTION",
                                                                FALSE));

        // Send X events which are relevant to the ongoing selection retrieval
        // to the clipboard widget.  Wait until either the operation completes, or
        // we hit our timeout.  All other X events remain queued.

        int select_result;

        int cnumber = ConnectionNumber(xDisplay);
        fd_set select_set;
        FD_ZERO(&select_set);
        FD_SET(cnumber, &select_set);
        ++cnumber;
        TimeStamp start = TimeStamp::Now();

        do {
            XEvent xevent;

            while (XCheckIfEvent(xDisplay, &xevent, checkEventProc,
                                 (XPointer) &context)) {

                if (xevent.xany.type == SelectionNotify)
                    DispatchSelectionNotifyEvent(context.cbWidget, &xevent);
                else
                    DispatchPropertyNotifyEvent(context.cbWidget, &xevent);

                if (mState == COMPLETED) {
                    void *data = mData;
                    mData = nullptr;
                    return data;
                }
            }

            TimeStamp now = TimeStamp::Now();
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = std::max<int32_t>(0,
                kClipboardTimeout - (now - start).ToMicroseconds());
            select_result = select(cnumber, &select_set, nullptr, nullptr, &tv);
        } while (select_result == 1 ||
                 (select_result == -1 && errno == EINTR));
    }
#ifdef DEBUG_CLIPBOARD
    printf("exceeded clipboard timeout\n");
#endif
    mState = TIMED_OUT;
    return nullptr;
}

static void
clipboard_contents_received(GtkClipboard     *clipboard,
                            GtkSelectionData *selection_data,
                            gpointer          data)
{
    RetrievalContext *context = static_cast<RetrievalContext*>(data);
    context->Complete(selection_data);
    context->Release();
}

static GtkSelectionData *
wait_for_contents(GtkClipboard *clipboard, GdkAtom target)
{
    RefPtr<RetrievalContext> context = new RetrievalContext();
    // Balanced by Release in clipboard_contents_received
    context.get()->AddRef();
    gtk_clipboard_request_contents(clipboard, target,
                                   clipboard_contents_received,
                                   context.get());
    return static_cast<GtkSelectionData*>(context->Wait());
}

static void
clipboard_text_received(GtkClipboard *clipboard,
                        const gchar  *text,
                        gpointer      data)
{
    RetrievalContext *context = static_cast<RetrievalContext*>(data);
    context->Complete(text);
    context->Release();
}

static gchar *
wait_for_text(GtkClipboard *clipboard)
{
    RefPtr<RetrievalContext> context = new RetrievalContext();
    // Balanced by Release in clipboard_text_received
    context.get()->AddRef();
    gtk_clipboard_request_text(clipboard, clipboard_text_received, context.get());
    return static_cast<gchar*>(context->Wait());
}

static GdkFilterReturn
selection_request_filter(GdkXEvent *gdk_xevent, GdkEvent *event, gpointer data)
{
    XEvent *xevent = static_cast<XEvent*>(gdk_xevent);
    if (xevent->xany.type == SelectionRequest) {
        if (xevent->xselectionrequest.requestor == X11None)
            return GDK_FILTER_REMOVE;

        GdkDisplay *display = gdk_x11_lookup_xdisplay(
                xevent->xselectionrequest.display);
        if (!display)
            return GDK_FILTER_REMOVE;

        GdkWindow *window = gdk_x11_window_foreign_new_for_display(display,
                xevent->xselectionrequest.requestor);
        if (!window)
            return GDK_FILTER_REMOVE;

        g_object_unref(window);
    }
    return GDK_FILTER_CONTINUE;
}
