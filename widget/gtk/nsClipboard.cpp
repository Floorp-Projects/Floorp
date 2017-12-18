/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#include "nsClipboardX11.h"
#include "HeadlessClipboard.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsIServiceManager.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"

#include "imgIContainer.h"

#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include "mozilla/Encoding.h"


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
ConvertHTMLtoUCS2          (const char*         data,
                            int32_t             dataLength,
                            char16_t         **unicodeData,
                            int32_t            &outUnicodeLen);

static void
GetHTMLCharset             (const char* data, int32_t dataLength, nsCString& str);

GdkAtom
GetSelectionAtom(int32_t aWhichClipboard)
{
    if (aWhichClipboard == nsIClipboard::kGlobalClipboard)
        return GDK_SELECTION_CLIPBOARD;

    return GDK_SELECTION_PRIMARY;
}

// Idle timeout for receiving selection and property notify events (microsec)
const int nsRetrievalContext::kClipboardTimeout = 500000;

nsClipboard::nsClipboard()
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
      os->AddObserver(this, "quit-application", false);
      os->AddObserver(this, "xpcom-shutdown", false);
  }
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
    // create nsRetrievalContext
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
        mContext = new nsRetrievalContextX11();
    }
    return NS_OK;
}


nsresult
nsClipboard::Store(void)
{
    if (mGlobalTransferable) {
        GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        gtk_clipboard_store(clipboard);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::Observe(nsISupports *aSubject, const char *aTopic,
                     const char16_t *aData)
{
    Store();
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
            nsCString flavorStr;
            flavor->ToString(getter_Copies(flavorStr));

            // special case text/unicode since we can handle all of
            // the string types
            if (flavorStr.EqualsLiteral(kUnicodeMime)) {
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
            GdkAtom atom = gdk_atom_intern(flavorStr.get(), FALSE);
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

void
nsClipboard::SetTransferableData(nsITransferable* aTransferable,
                                 nsCString&       aFlavor,
                                 const char*      aClipboardData,
                                 uint32_t         aClipboardDataLength)
{
  nsCOMPtr<nsISupports> wrapper;
  nsPrimitiveHelpers::CreatePrimitiveForData(aFlavor,
                                             aClipboardData,
                                             aClipboardDataLength,
                                             getter_AddRefs(wrapper));
  aTransferable->SetTransferData(aFlavor.get(),
                                 wrapper, aClipboardDataLength);
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable *aTransferable, int32_t aWhichClipboard)
{
    if (!aTransferable)
        return NS_ERROR_FAILURE;

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
        if (!currentFlavor)
            continue;

        nsCString flavorStr;
        currentFlavor->ToString(getter_Copies(flavorStr));

        if (flavorStr.EqualsLiteral(kJPEGImageMime) ||
            flavorStr.EqualsLiteral(kJPGImageMime) ||
            flavorStr.EqualsLiteral(kPNGImageMime) ||
            flavorStr.EqualsLiteral(kGIFImageMime)) {
            // Emulate support for image/jpg
            if (flavorStr.EqualsLiteral(kJPGImageMime)) {
                flavorStr.Assign(kJPEGImageMime);
            }

            uint32_t    clipboardDataLength;
            const char* clipboardData =
                mContext->GetClipboardData(flavorStr.get(),
                                           aWhichClipboard,
                                           &clipboardDataLength);
            if (!clipboardData)
                continue;

            nsCOMPtr<nsIInputStream> byteStream;
            NS_NewByteInputStream(getter_AddRefs(byteStream),
                                  clipboardData,
                                  clipboardDataLength,
                                  NS_ASSIGNMENT_COPY);
            aTransferable->SetTransferData(flavorStr.get(), byteStream,
                                           sizeof(nsIInputStream*));

            mContext->ReleaseClipboardData(clipboardData);
            return NS_OK;
        }

        // Special case text/unicode since we can convert any
        // string into text/unicode
        if (flavorStr.EqualsLiteral(kUnicodeMime)) {
            uint32_t    clipboardDataLength;
            const char* clipboardData =
                mContext->GetClipboardData(GTK_DEFAULT_MIME_TEXT,
                                           aWhichClipboard,
                                           &clipboardDataLength);
            if (!clipboardData) {
                // If the type was text/unicode and we couldn't get
                // text off the clipboard, run the next loop
                // iteration.
                continue;
            }

            // Convert utf-8 into our unicode format.
            NS_ConvertUTF8toUTF16 ucs2string(clipboardData, clipboardDataLength);
            const char* unicodeData = (const char *)ToNewUnicode(ucs2string);
            uint32_t unicodeDataLength = ucs2string.Length() * 2;
            SetTransferableData(aTransferable, flavorStr,
                                unicodeData, unicodeDataLength);
            free((void *)unicodeData);

            mContext->ReleaseClipboardData(clipboardData);
            return NS_OK;
        }


        uint32_t clipboardDataLength;
        const char* clipboardData = mContext->GetClipboardData(
            flavorStr.get(), aWhichClipboard, &clipboardDataLength);

        if (clipboardData) {
            // Special case text/html since we can convert into UCS2
            if (flavorStr.EqualsLiteral(kHTMLMime)) {
                char16_t* htmlBody = nullptr;
                int32_t htmlBodyLen = 0;
                // Convert text/html into our unicode format
                ConvertHTMLtoUCS2(clipboardData, clipboardDataLength,
                                  &htmlBody, htmlBodyLen);

                // Try next data format?
                if (!htmlBodyLen) {
                    mContext->ReleaseClipboardData(clipboardData);
                    continue;
                }

                SetTransferableData(aTransferable, flavorStr,
                                    (const char*)htmlBody, htmlBodyLen * 2);
                free(htmlBody);
            } else {
                SetTransferableData(aTransferable, flavorStr,
                                    clipboardData, clipboardDataLength);
            }

            mContext->ReleaseClipboardData(clipboardData);
            return NS_OK;
        }
    }

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

  int targetNums;
  GdkAtom* targets = mContext->GetTargets(aWhichClipboard, &targetNums);

  // Walk through the provided types and try to match it to a
  // provided type.
  for (uint32_t i = 0; i < aLength && !*_retval; i++) {
      // We special case text/unicode here.
      if (!strcmp(aFlavorList[i], kUnicodeMime) &&
          gtk_targets_include_text(targets, targetNums)) {
          *_retval = true;
          break;
      }

      for (int32_t j = 0; j < targetNums; j++) {
          gchar *atom_name = gdk_atom_name(targets[j]);
          if (!atom_name)
              continue;

          if (!strcmp(atom_name, aFlavorList[i]))
              *_retval = true;

          // X clipboard supports image/jpeg, but we want to emulate support
          // for image/jpg as well
          if (!strcmp(aFlavorList[i], kJPGImageMime) &&
              !strcmp(atom_name, kJPEGImageMime)) {
              *_retval = true;
          }

          g_free(atom_name);

          if (*_retval)
              break;
      }
  }

  g_free(targets);
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool *_retval)
{
    // yeah, unix supports the selection clipboard on X11
    // TODO Wayland
    *_retval = GDK_IS_X11_DISPLAY(gdk_display_get_default());
    return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool* _retval)
{
  *_retval = false;
  return NS_OK;
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
    nsPrimitiveHelpers::CreateDataFromPrimitive(nsDependentCString(target_name),
                                                item, &primitive_data, len);

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
                    g_malloc((len * sizeof(guchar)) + sizeof(char16_t));
            if (!buffer)
                return;
            char16_t prefix = 0xFEFF;
            memcpy(buffer, &prefix, sizeof(prefix));
            memcpy(buffer + sizeof(prefix), primitive_data, len);
            g_free((guchar *)primitive_data);
            primitive_data = (guchar *)buffer;
            len += sizeof(prefix);
        }
  
        gtk_selection_data_set(aSelectionData, selectionTarget,
                               8, /* 8 bits in a unit */
                               (const guchar *)primitive_data, len);
        g_free(primitive_data);
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
void ConvertHTMLtoUCS2(const char* data, int32_t dataLength,
                       char16_t** unicodeData, int32_t& outUnicodeLen)
{
    nsAutoCString charset;
    GetHTMLCharset(data, dataLength, charset);// get charset of HTML
    if (charset.EqualsLiteral("UTF-16")) {//current mozilla
        outUnicodeLen = (dataLength / 2) - 1;
        *unicodeData =
            reinterpret_cast<char16_t*>
            (moz_xmalloc((outUnicodeLen + sizeof('\0')) * sizeof(char16_t)));
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
void GetHTMLCharset(const char* data, int32_t dataLength, nsCString& str)
{
    // if detect "FFFE" or "FEFF", assume UTF-16
    char16_t* beginChar =  (char16_t*)data;
    if ((beginChar[0] == 0xFFFE) || (beginChar[0] == 0xFEFF)) {
        str.AssignLiteral("UTF-16");
        return;
    }
    // no "FFFE" and "FEFF", assume ASCII first to find "charset" info
    const nsDependentCString htmlStr(data, dataLength);
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
