/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsArrayUtils.h"
#include "nsClipboard.h"
#if defined(MOZ_X11)
#  include "nsClipboardX11.h"
#endif
#if defined(MOZ_WAYLAND)
#  include "nsClipboardWayland.h"
#endif
#include "nsGtkUtils.h"
#include "nsIURI.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsContentUtils.h"
#include "HeadlessClipboard.h"
#include "nsSupportsPrimitives.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"
#include "nsImageToPixbuf.h"
#include "nsStringStream.h"
#include "nsIFileURL.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/RefPtr.h"
#include "mozilla/GRefPtr.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/TimeStamp.h"
#include "GRefPtr.h"
#include "WidgetUtilsGtk.h"

#include "imgIContainer.h"

#include <gtk/gtk.h>
#if defined(MOZ_X11)
#  include <gtk/gtkx.h>
#endif

#include "mozilla/Encoding.h"

using namespace mozilla;

// Idle timeout for receiving selection and property notify events (microsec)
// Right now it's set to 1 sec.
const int kClipboardTimeout = 1000000;

// Defines how many event loop iterations will be done without sleep.
// We ususally get data in first 2-3 iterations unless some large object
// (an image for instance) is transferred through clipboard.
const int kClipboardFastIterationNum = 3;

// We add this prefix to HTML markup, so that GetHTMLCharset can correctly
// detect the HTML as UTF-8 encoded.
static const char kHTMLMarkupPrefix[] =
    R"(<meta http-equiv="content-type" content="text/html; charset=utf-8">)";

static const char kURIListMime[] = "text/uri-list";

ClipboardTargets nsRetrievalContext::sClipboardTargets;
ClipboardTargets nsRetrievalContext::sPrimaryTargets;

// Callback when someone asks us for the data
void clipboard_get_cb(GtkClipboard* aGtkClipboard,
                      GtkSelectionData* aSelectionData, guint info,
                      gpointer user_data);

// Callback when someone asks us to clear a clipboard
void clipboard_clear_cb(GtkClipboard* aGtkClipboard, gpointer user_data);

static bool ConvertHTMLtoUCS2(Span<const char> aData, nsCString& charset,
                              char16_t** unicodeData, int32_t& outUnicodeLen);

static bool GetHTMLCharset(Span<const char> aData, nsCString& str);

ClipboardTargets ClipboardTargets::Clone() {
  ClipboardTargets ret;
  ret.mCount = mCount;
  if (mCount) {
    ret.mTargets.reset(
        reinterpret_cast<GdkAtom*>(g_malloc(sizeof(GdkAtom) * mCount)));
    memcpy(ret.mTargets.get(), mTargets.get(), sizeof(GdkAtom) * mCount);
  }
  return ret;
}

void ClipboardTargets::Set(ClipboardTargets aTargets) {
  mCount = aTargets.mCount;
  mTargets = std::move(aTargets.mTargets);
}

void ClipboardData::SetData(Span<const uint8_t> aData) {
  mData = nullptr;
  mLength = aData.Length();
  if (mLength) {
    mData.reset(reinterpret_cast<char*>(g_malloc(sizeof(char) * mLength)));
    memcpy(mData.get(), aData.data(), sizeof(char) * mLength);
  }
}

void ClipboardData::SetText(Span<const char> aData) {
  mData = nullptr;
  mLength = aData.Length();
  if (mLength) {
    mData.reset(
        reinterpret_cast<char*>(g_malloc(sizeof(char) * (mLength + 1))));
    memcpy(mData.get(), aData.data(), sizeof(char) * mLength);
    mData.get()[mLength] = '\0';
  }
}

void ClipboardData::SetTargets(ClipboardTargets aTargets) {
  mLength = aTargets.mCount;
  mData.reset(reinterpret_cast<char*>(aTargets.mTargets.release()));
}

ClipboardTargets ClipboardData::ExtractTargets() {
  GUniquePtr<GdkAtom> targets(reinterpret_cast<GdkAtom*>(mData.release()));
  uint32_t length = std::exchange(mLength, 0);
  return ClipboardTargets{std::move(targets), length};
}

GdkAtom GetSelectionAtom(int32_t aWhichClipboard) {
  if (aWhichClipboard == nsIClipboard::kGlobalClipboard)
    return GDK_SELECTION_CLIPBOARD;

  return GDK_SELECTION_PRIMARY;
}

int GetGeckoClipboardType(GtkClipboard* aGtkClipboard) {
  if (aGtkClipboard == gtk_clipboard_get(GDK_SELECTION_PRIMARY))
    return nsClipboard::kSelectionClipboard;
  else if (aGtkClipboard == gtk_clipboard_get(GDK_SELECTION_CLIPBOARD))
    return nsClipboard::kGlobalClipboard;

  return -1;  // THAT AIN'T NO CLIPBOARD I EVER HEARD OF
}

void nsRetrievalContext::ClearCachedTargetsClipboard(GtkClipboard* aClipboard,
                                                     GdkEvent* aEvent,
                                                     gpointer data) {
  LOGCLIP("nsRetrievalContext::ClearCachedTargetsClipboard()");
  sClipboardTargets.Clear();
}

void nsRetrievalContext::ClearCachedTargetsPrimary(GtkClipboard* aClipboard,
                                                   GdkEvent* aEvent,
                                                   gpointer data) {
  LOGCLIP("nsRetrievalContext::ClearCachedTargetsPrimary()");
  sPrimaryTargets.Clear();
}

ClipboardTargets nsRetrievalContext::GetTargets(int32_t aWhichClipboard) {
  LOGCLIP("nsRetrievalContext::GetTargets(%s)\n",
          aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard");
  ClipboardTargets& storedTargets =
      (aWhichClipboard == nsClipboard::kSelectionClipboard) ? sPrimaryTargets
                                                            : sClipboardTargets;
  if (!storedTargets) {
    LOGCLIP("  getting targets from system");
    storedTargets.Set(GetTargetsImpl(aWhichClipboard));
  } else {
    LOGCLIP("  using cached targets");
  }
  return storedTargets.Clone();
}

nsRetrievalContext::nsRetrievalContext() {
  g_signal_connect(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), "owner-change",
                   G_CALLBACK(ClearCachedTargetsClipboard), this);
  g_signal_connect(gtk_clipboard_get(GDK_SELECTION_PRIMARY), "owner-change",
                   G_CALLBACK(ClearCachedTargetsPrimary), this);
}

nsRetrievalContext::~nsRetrievalContext() {
  g_signal_handlers_disconnect_by_func(
      gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
      FuncToGpointer(ClearCachedTargetsClipboard), this);
  g_signal_handlers_disconnect_by_func(
      gtk_clipboard_get(GDK_SELECTION_PRIMARY),
      FuncToGpointer(ClearCachedTargetsPrimary), this);
  sClipboardTargets.Clear();
  sPrimaryTargets.Clear();
}

nsClipboard::nsClipboard() = default;

nsClipboard::~nsClipboard() {
  // We have to clear clipboard before gdk_display_close() call.
  // See bug 531580 for details.
  if (mGlobalTransferable) {
    gtk_clipboard_clear(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
  }
  if (mSelectionTransferable) {
    gtk_clipboard_clear(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
  }
}

NS_IMPL_ISUPPORTS(nsClipboard, nsIClipboard, nsIObserver)

nsresult nsClipboard::Init(void) {
#if defined(MOZ_X11)
  if (widget::GdkIsX11Display()) {
    mContext = new nsRetrievalContextX11();
  }
#endif
#if defined(MOZ_WAYLAND)
  if (widget::GdkIsWaylandDisplay()) {
    mContext = new nsRetrievalContextWayland();
  }
#endif

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(this, "xpcom-shutdown", false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  // Save global clipboard content to CLIPBOARD_MANAGER.
  // gtk_clipboard_store() can run an event loop, so call from a dedicated
  // runnable.
  return SchedulerGroup::Dispatch(
      TaskCategory::Other,
      NS_NewRunnableFunction("gtk_clipboard_store()", []() {
        LOGCLIP("nsClipboard storing clipboard content\n");
        gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
      }));
}

NS_IMETHODIMP
nsClipboard::SetData(nsITransferable* aTransferable, nsIClipboardOwner* aOwner,
                     int32_t aWhichClipboard) {
  // See if we can short cut
  if ((aWhichClipboard == kGlobalClipboard &&
       aTransferable == mGlobalTransferable.get() &&
       aOwner == mGlobalOwner.get()) ||
      (aWhichClipboard == kSelectionClipboard &&
       aTransferable == mSelectionTransferable.get() &&
       aOwner == mSelectionOwner.get())) {
    return NS_OK;
  }

  LOGCLIP("nsClipboard::SetData (%s)\n",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  // List of suported targets
  GtkTargetList* list = gtk_target_list_new(nullptr, 0);

  // Get the types of supported flavors
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanExport(flavors);
  if (NS_FAILED(rv)) {
    LOGCLIP("    FlavorsTransferableCanExport failed!\n");
    // Fall through.  |gtkTargets| will be null below.
  }

  // Add all the flavors to this widget's supported type.
  bool imagesAdded = false;
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];
    LOGCLIP("    processing target %s\n", flavorStr.get());

    // Special case text/unicode since we can handle all of the string types.
    if (flavorStr.EqualsLiteral(kUnicodeMime)) {
      LOGCLIP("    adding TEXT targets\n");
      gtk_target_list_add_text_targets(list, 0);
      continue;
    }

    if (nsContentUtils::IsFlavorImage(flavorStr)) {
      // Don't bother adding image targets twice
      if (!imagesAdded) {
        // accept any writable image type
        LOGCLIP("    adding IMAGE targets\n");
        gtk_target_list_add_image_targets(list, 0, TRUE);
        imagesAdded = true;
      }
      continue;
    }

    // Add this to our list of valid targets
    LOGCLIP("    adding OTHER target %s\n", flavorStr.get());
    GdkAtom atom = gdk_atom_intern(flavorStr.get(), FALSE);
    gtk_target_list_add(list, atom, 0, 0);
  }

  // Get GTK clipboard (CLIPBOARD or PRIMARY)
  GtkClipboard* gtkClipboard =
      gtk_clipboard_get(GetSelectionAtom(aWhichClipboard));

  gint numTargets = 0;
  GtkTargetEntry* gtkTargets =
      gtk_target_table_new_from_list(list, &numTargets);
  if (!gtkTargets || numTargets == 0) {
    LOGCLIP(
        "    gtk_target_table_new_from_list() failed or empty list of "
        "targets!\n");
    // Clear references to the any old data and let GTK know that it is no
    // longer available.
    EmptyClipboard(aWhichClipboard);
    return NS_ERROR_FAILURE;
  }

  ClearCachedTargets(aWhichClipboard);

  // Set getcallback and request to store data after an application exit
  if (gtk_clipboard_set_with_data(gtkClipboard, gtkTargets, numTargets,
                                  clipboard_get_cb, clipboard_clear_cb, this)) {
    // We managed to set-up the clipboard so update internal state
    // We have to set it now because gtk_clipboard_set_with_data() calls
    // clipboard_clear_cb() which reset our internal state
    if (aWhichClipboard == kSelectionClipboard) {
      mSelectionOwner = aOwner;
      mSelectionTransferable = aTransferable;
    } else {
      mGlobalOwner = aOwner;
      mGlobalTransferable = aTransferable;
      gtk_clipboard_set_can_store(gtkClipboard, gtkTargets, numTargets);
    }

    rv = NS_OK;
  } else {
    LOGCLIP("    gtk_clipboard_set_with_data() failed!\n");
    EmptyClipboard(aWhichClipboard);
    rv = NS_ERROR_FAILURE;
  }

  gtk_target_table_free(gtkTargets, numTargets);
  gtk_target_list_unref(list);

  return rv;
}

void nsClipboard::SetTransferableData(nsITransferable* aTransferable,
                                      nsCString& aFlavor,
                                      const char* aClipboardData,
                                      uint32_t aClipboardDataLength) {
  LOGCLIP("nsClipboard::SetTransferableData MIME %s\n", aFlavor.get());

  nsCOMPtr<nsISupports> wrapper;
  nsPrimitiveHelpers::CreatePrimitiveForData(
      aFlavor, aClipboardData, aClipboardDataLength, getter_AddRefs(wrapper));
  aTransferable->SetTransferData(aFlavor.get(), wrapper);
}

static bool IsMIMEAtFlavourList(const nsTArray<nsCString>& aFlavourList,
                                const char* aMime) {
  for (const auto& flavorStr : aFlavourList) {
    if (flavorStr.Equals(aMime)) {
      return true;
    }
  }
  return false;
}

// When clipboard contains only images, X11/Gtk tries to convert them
// to text when we request text instead of just fail to provide the data.
// So if clipboard contains images only remove text MIME offer.
bool nsClipboard::FilterImportedFlavors(int32_t aWhichClipboard,
                                        nsTArray<nsCString>& aFlavors) {
  LOGCLIP("nsClipboard::FilterImportedFlavors");

  auto targets = mContext->GetTargets(aWhichClipboard);
  if (!targets) {
    LOGCLIP("    X11: no targes at clipboard (null), quit.\n");
    return true;
  }

  for (const auto& atom : targets.AsSpan()) {
    GUniquePtr<gchar> atom_name(gdk_atom_name(atom));
    if (!atom_name) {
      continue;
    }
    // Filter out system MIME types.
    if (strcmp(atom_name.get(), "TARGETS") == 0 ||
        strcmp(atom_name.get(), "TIMESTAMP") == 0 ||
        strcmp(atom_name.get(), "SAVE_TARGETS") == 0 ||
        strcmp(atom_name.get(), "MULTIPLE") == 0) {
      continue;
    }
    // Filter out types which can't be converted to text.
    if (strncmp(atom_name.get(), "image/", 6) == 0 ||
        strncmp(atom_name.get(), "application/", 12) == 0 ||
        strncmp(atom_name.get(), "audio/", 6) == 0 ||
        strncmp(atom_name.get(), "video/", 6) == 0) {
      continue;
    }
    // We have some other MIME type on clipboard which can be hopefully
    // converted to text without any problem.
    LOGCLIP("    X11: text types in clipboard, no need to filter them.\n");
    return true;
  }

  // So make sure we offer only types we have at clipboard.
  nsTArray<nsCString> clipboardFlavors;
  for (const auto& atom : targets.AsSpan()) {
    GUniquePtr<gchar> atom_name(gdk_atom_name(atom));
    if (!atom_name) {
      continue;
    }
    if (IsMIMEAtFlavourList(aFlavors, atom_name.get())) {
      clipboardFlavors.AppendElement(nsCString(atom_name.get()));
    }
  }
  aFlavors.SwapElements(clipboardFlavors);
#ifdef MOZ_LOGGING
  LOGCLIP("    X11: Flavors which match clipboard content:\n");
  for (uint32_t i = 0; i < aFlavors.Length(); i++) {
    LOGCLIP("    %s\n", aFlavors[i].get());
  }
#endif
  return true;
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable* aTransferable, int32_t aWhichClipboard) {
  LOGCLIP("nsClipboard::GetData (%s)\n",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  // TODO: Ensure we don't re-enter here.
  if (!aTransferable || !mContext) {
    return NS_ERROR_FAILURE;
  }

  // Get a list of flavors this transferable can import
  nsTArray<nsCString> flavors;
  nsresult rv = aTransferable->FlavorsTransferableCanImport(flavors);
  if (NS_FAILED(rv)) {
    LOGCLIP("    FlavorsTransferableCanImport falied!\n");
    return rv;
  }
#ifdef MOZ_LOGGING
  LOGCLIP("    Flavors which can be imported:");
  for (uint32_t i = 0; i < flavors.Length(); i++) {
    LOGCLIP("    %s\n", flavors[i].get());
  }
#endif

  // Filter out MIME types on X11 to prevent unwanted conversions,
  // see Bug 1611407
  if (widget::GdkIsX11Display() &&
      !FilterImportedFlavors(aWhichClipboard, flavors)) {
    LOGCLIP("    Missing suitable clipboard data, quit.");
    return NS_OK;
  }

  for (uint32_t i = 0; i < flavors.Length(); i++) {
    nsCString& flavorStr = flavors[i];

    if (flavorStr.EqualsLiteral(kJPEGImageMime) ||
        flavorStr.EqualsLiteral(kJPGImageMime) ||
        flavorStr.EqualsLiteral(kPNGImageMime) ||
        flavorStr.EqualsLiteral(kGIFImageMime)) {
      // Emulate support for image/jpg
      if (flavorStr.EqualsLiteral(kJPGImageMime)) {
        flavorStr.Assign(kJPEGImageMime);
      }

      LOGCLIP("    Getting image %s MIME clipboard data\n", flavorStr.get());

      auto clipboardData =
          mContext->GetClipboardData(flavorStr.get(), aWhichClipboard);
      if (!clipboardData) {
        LOGCLIP("    %s type is missing\n", flavorStr.get());
        continue;
      }

      nsCOMPtr<nsIInputStream> byteStream;
      NS_NewByteInputStream(getter_AddRefs(byteStream), clipboardData.AsSpan(),
                            NS_ASSIGNMENT_COPY);
      aTransferable->SetTransferData(flavorStr.get(), byteStream);
      LOGCLIP("    got %s MIME data\n", flavorStr.get());
      return NS_OK;
    }

    // Special case text/unicode since we can convert any
    // string into text/unicode
    if (flavorStr.EqualsLiteral(kUnicodeMime)) {
      LOGCLIP("    Getting unicode %s MIME clipboard data\n", flavorStr.get());

      auto clipboardData = mContext->GetClipboardText(aWhichClipboard);
      if (!clipboardData) {
        LOGCLIP("    failed to get unicode data\n");
        // If the type was text/unicode and we couldn't get
        // text off the clipboard, run the next loop
        // iteration.
        continue;
      }

      // Convert utf-8 into our unicode format.
      NS_ConvertUTF8toUTF16 ucs2string(clipboardData.get());
      SetTransferableData(aTransferable, flavorStr,
                          (const char*)ucs2string.BeginReading(),
                          ucs2string.Length() * 2);

      LOGCLIP("    got unicode data, length %zd\n", ucs2string.Length());
      return NS_OK;
    }

    if (flavorStr.EqualsLiteral(kFileMime)) {
      LOGCLIP("    Getting %s file clipboard data\n", flavorStr.get());

      auto clipboardData =
          mContext->GetClipboardData(kURIListMime, aWhichClipboard);
      if (!clipboardData) {
        LOGCLIP("    text/uri-list type is missing\n");
        continue;
      }

      nsDependentCSubstring data(clipboardData.AsSpan());
      nsTArray<nsCString> uris = mozilla::widget::ParseTextURIList(data);
      if (!uris.IsEmpty()) {
        nsCOMPtr<nsIURI> fileURI;
        NS_NewURI(getter_AddRefs(fileURI), uris[0]);
        if (nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv)) {
          nsCOMPtr<nsIFile> file;
          rv = fileURL->GetFile(getter_AddRefs(file));
          if (NS_SUCCEEDED(rv)) {
            aTransferable->SetTransferData(flavorStr.get(), file);
            LOGCLIP("    successfully set file to clipboard\n");
          }
        }
      }
      return NS_OK;
    }

    LOGCLIP("    Getting %s MIME clipboard data\n", flavorStr.get());

    auto clipboardData =
        mContext->GetClipboardData(flavorStr.get(), aWhichClipboard);

#ifdef MOZ_LOGGING
    if (!clipboardData) {
      LOGCLIP("    %s type is missing\n", flavorStr.get());
    }
#endif

    if (clipboardData) {
      LOGCLIP("    got %s mime type data.\n", flavorStr.get());

      // Special case text/html since we can convert into UCS2
      if (flavorStr.EqualsLiteral(kHTMLMime)) {
        char16_t* htmlBody = nullptr;
        int32_t htmlBodyLen = 0;
        // Convert text/html into our unicode format
        nsAutoCString charset;
        if (!GetHTMLCharset(clipboardData.AsSpan(), charset)) {
          // Fall back to utf-8 in case html/data is missing kHTMLMarkupPrefix.
          LOGCLIP("Failed to get html/text encoding, fall back to utf-8.\n");
          charset.AssignLiteral("utf-8");
        }
        if (!ConvertHTMLtoUCS2(clipboardData.AsSpan(), charset, &htmlBody,
                               htmlBodyLen)) {
          LOGCLIP("    failed to convert text/html to UCS2.\n");
          continue;
        }

        SetTransferableData(aTransferable, flavorStr, (const char*)htmlBody,
                            htmlBodyLen * 2);
        free(htmlBody);
      } else {
        auto span = clipboardData.AsSpan();
        SetTransferableData(aTransferable, flavorStr, span.data(),
                            span.Length());
      }
      return NS_OK;
    }
  }

  LOGCLIP("    failed to get clipboard content.\n");
  return NS_OK;
}

RefPtr<GenericPromise> nsClipboard::AsyncGetData(nsITransferable* aTransferable,
                                                 int32_t aWhichClipboard) {
  // XXX we should read the clipboard data asynchronously instead, bug 1778201.
  nsresult rv = GetData(aTransferable, aWhichClipboard);
  if (NS_FAILED(rv)) {
    return GenericPromise::CreateAndReject(rv, __func__);
  }

  return GenericPromise::CreateAndResolve(true, __func__);
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(int32_t aWhichClipboard) {
  LOGCLIP("nsClipboard::EmptyClipboard (%s)\n",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");
  if (aWhichClipboard == kSelectionClipboard) {
    if (mSelectionTransferable) {
      gtk_clipboard_clear(gtk_clipboard_get(GDK_SELECTION_PRIMARY));
      MOZ_ASSERT(!mSelectionTransferable);
    }
  } else {
    if (mGlobalTransferable) {
      gtk_clipboard_clear(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
      MOZ_ASSERT(!mGlobalTransferable);
    }
  }
  ClearCachedTargets(aWhichClipboard);
  return NS_OK;
}

void nsClipboard::ClearTransferable(int32_t aWhichClipboard) {
  if (aWhichClipboard == kSelectionClipboard) {
    if (mSelectionOwner) {
      mSelectionOwner->LosingOwnership(mSelectionTransferable);
      mSelectionOwner = nullptr;
    }
    mSelectionTransferable = nullptr;
  } else {
    if (mGlobalOwner) {
      mGlobalOwner->LosingOwnership(mGlobalTransferable);
      mGlobalOwner = nullptr;
    }
    mGlobalTransferable = nullptr;
  }
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                    int32_t aWhichClipboard, bool* _retval) {
  if (!_retval) {
    return NS_ERROR_NULL_POINTER;
  }

  LOGCLIP("nsClipboard::HasDataMatchingFlavors (%s)\n",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  *_retval = false;

  if (!mContext) {
    return NS_ERROR_FAILURE;
  }

  auto targets = mContext->GetTargets(aWhichClipboard);
  if (!targets) {
    LOGCLIP("    no targes at clipboard (null)\n");
    return NS_OK;
  }

#ifdef MOZ_LOGGING
  if (LOGCLIP_ENABLED()) {
    LOGCLIP("    Clipboard content (target nums %zu):\n",
            targets.AsSpan().Length());
    for (const auto& target : targets.AsSpan()) {
      GUniquePtr<gchar> atom_name(gdk_atom_name(target));
      if (!atom_name) {
        LOGCLIP("        failed to get MIME\n");
        continue;
      }
      LOGCLIP("        MIME %s\n", atom_name.get());
    }
    LOGCLIP("    Asking for content:\n");
    for (auto& flavor : aFlavorList) {
      LOGCLIP("        MIME %s\n", flavor.get());
    }
  }
#endif

  // Walk through the provided types and try to match it to a
  // provided type.
  for (auto& flavor : aFlavorList) {
    // We special case text/unicode here.
    if (flavor.EqualsLiteral(kUnicodeMime) &&
        gtk_targets_include_text(targets.AsSpan().data(),
                                 targets.AsSpan().Length())) {
      *_retval = true;
      LOGCLIP("    has kUnicodeMime\n");
      break;
    }

    for (const auto& target : targets.AsSpan()) {
      GUniquePtr<gchar> atom_name(gdk_atom_name(target));
      if (!atom_name) {
        continue;
      }

      if (flavor.Equals(atom_name.get())) {
        LOGCLIP("    has %s\n", atom_name.get());
        *_retval = true;
        break;
      }
      // X clipboard supports image/jpeg, but we want to emulate support
      // for image/jpg as well
      if (flavor.EqualsLiteral(kJPGImageMime) &&
          !strcmp(atom_name.get(), kJPEGImageMime)) {
        LOGCLIP("    has image/jpg\n");
        *_retval = true;
        break;
      }
      // application/x-moz-file should be treated like text/uri-list
      if (flavor.EqualsLiteral(kFileMime) &&
          !strcmp(atom_name.get(), kURIListMime)) {
        LOGCLIP("    has text/uri-list treating as application/x-moz-file");
        *_retval = true;
        break;
      }
    }

    if (*_retval) {
      break;
    }
  }

#ifdef MOZ_LOGGING
  if (!(*_retval)) {
    LOGCLIP("    no targes at clipboard (bad match)\n");
  }
#endif

  return NS_OK;
}

RefPtr<DataFlavorsPromise> nsClipboard::AsyncHasDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  LOGCLIP("nsClipboard::AsyncHasDataMatchingFlavors (%s)\n",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  nsTArray<nsCString> results;
  for (const auto& flavor : aFlavorList) {
    bool hasMatchingFlavor = false;
    nsresult rv = HasDataMatchingFlavors(AutoTArray<nsCString, 1>{flavor},
                                         aWhichClipboard, &hasMatchingFlavor);
    if (NS_SUCCEEDED(rv) && hasMatchingFlavor) {
      results.AppendElement(flavor);
    }
  }

  return DataFlavorsPromise::CreateAndResolve(std::move(results), __func__);
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool* _retval) {
  *_retval = true;
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsFindClipboard(bool* _retval) {
  *_retval = false;
  return NS_OK;
}

nsITransferable* nsClipboard::GetTransferable(int32_t aWhichClipboard) {
  nsITransferable* retval;

  if (aWhichClipboard == kSelectionClipboard)
    retval = mSelectionTransferable.get();
  else
    retval = mGlobalTransferable.get();

  return retval;
}

void nsClipboard::SelectionGetEvent(GtkClipboard* aClipboard,
                                    GtkSelectionData* aSelectionData) {
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
    return;  // THAT AIN'T NO CLIPBOARD I EVER HEARD OF

  LOGCLIP("nsClipboard::SelectionGetEvent (%s)\n",
          whichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  nsCOMPtr<nsITransferable> trans = GetTransferable(whichClipboard);
  if (!trans) {
    // We have nothing to serve
    LOGCLIP("nsClipboard::SelectionGetEvent() - %s clipboard is empty!\n",
            whichClipboard == kSelectionClipboard ? "Primary" : "Clipboard");
    return;
  }

  nsresult rv;
  nsCOMPtr<nsISupports> item;

  GdkAtom selectionTarget = gtk_selection_data_get_target(aSelectionData);
  LOGCLIP("  selection target %s\n",
          GUniquePtr<gchar>(gdk_atom_name(selectionTarget)).get());

  // Check to see if the selection data is some text type.
  if (gtk_targets_include_text(&selectionTarget, 1)) {
    LOGCLIP("  providing text/unicode data\n");
    // Try to convert our internal type into a text string.  Get
    // the transferable for this clipboard and try to get the
    // text/unicode type for it.
    rv = trans->GetTransferData("text/unicode", getter_AddRefs(item));
    if (NS_FAILED(rv) || !item) {
      LOGCLIP("  GetTransferData() failed to get text/unicode!\n");
      return;
    }

    nsCOMPtr<nsISupportsString> wideString;
    wideString = do_QueryInterface(item);
    if (!wideString) return;

    nsAutoString ucs2string;
    wideString->GetData(ucs2string);
    NS_ConvertUTF16toUTF8 utf8string(ucs2string);

    LOGCLIP("  sent %zd bytes of utf-8 data\n", utf8string.Length());
    if (selectionTarget == gdk_atom_intern("text/plain;charset=utf-8", FALSE)) {
      LOGCLIP(
          "  using gtk_selection_data_set for 'text/plain;charset=utf-8'\n");
      // Bypass gtk_selection_data_set_text, which will convert \n to \r\n
      // in some versions of GTK.
      gtk_selection_data_set(aSelectionData, selectionTarget, 8,
                             reinterpret_cast<const guchar*>(utf8string.get()),
                             utf8string.Length());
    } else {
      gtk_selection_data_set_text(aSelectionData, utf8string.get(),
                                  utf8string.Length());
    }
    return;
  }

  // Check to see if the selection data is an image type
  if (gtk_targets_include_image(&selectionTarget, 1, TRUE)) {
    LOGCLIP("  providing image data\n");
    // Look through our transfer data for the image
    static const char* const imageMimeTypes[] = {kNativeImageMime,
                                                 kPNGImageMime, kJPEGImageMime,
                                                 kJPGImageMime, kGIFImageMime};
    nsCOMPtr<nsISupports> imageItem;
    nsCOMPtr<imgIContainer> image;
    for (uint32_t i = 0; i < ArrayLength(imageMimeTypes); i++) {
      rv = trans->GetTransferData(imageMimeTypes[i], getter_AddRefs(imageItem));
      if (NS_FAILED(rv)) {
        LOGCLIP("    %s is missing at GetTransferData()\n", imageMimeTypes[i]);
        continue;
      }

      image = do_QueryInterface(imageItem);
      if (image) {
        LOGCLIP("    %s is available at GetTransferData()\n",
                imageMimeTypes[i]);
        break;
      }
    }

    if (!image) {  // Not getting an image for an image mime type!?
      LOGCLIP("    Failed to get any image mime from GetTransferData()!\n");
      return;
    }

    RefPtr<GdkPixbuf> pixbuf = nsImageToPixbuf::ImageToPixbuf(image);
    if (!pixbuf) {
      LOGCLIP("    nsImageToPixbuf::ImageToPixbuf() failed!\n");
      return;
    }

    LOGCLIP("    Setting pixbuf image data as %s\n",
            GUniquePtr<gchar>(gdk_atom_name(selectionTarget)).get());
    gtk_selection_data_set_pixbuf(aSelectionData, pixbuf);
    return;
  }

  if (selectionTarget == gdk_atom_intern(kHTMLMime, FALSE)) {
    LOGCLIP("  providing %s data\n", kHTMLMime);
    rv = trans->GetTransferData(kHTMLMime, getter_AddRefs(item));
    if (NS_FAILED(rv) || !item) {
      LOGCLIP("  failed to get %s data by GetTransferData()!\n", kHTMLMime);
      return;
    }

    nsCOMPtr<nsISupportsString> wideString;
    wideString = do_QueryInterface(item);
    if (!wideString) {
      LOGCLIP("  failed to get wideString interface!");
      return;
    }

    nsAutoString ucs2string;
    wideString->GetData(ucs2string);

    nsAutoCString html;
    // Add the prefix so the encoding is correctly detected.
    html.AppendLiteral(kHTMLMarkupPrefix);
    AppendUTF16toUTF8(ucs2string, html);

    LOGCLIP("  Setting %zd bytest of %s data\n", html.Length(),
            GUniquePtr<gchar>(gdk_atom_name(selectionTarget)).get());
    gtk_selection_data_set(aSelectionData, selectionTarget, 8,
                           (const guchar*)html.get(), html.Length());
    return;
  }

  LOGCLIP("  Try if we have anything at GetTransferData() for %s\n",
          GUniquePtr<gchar>(gdk_atom_name(selectionTarget)).get());

  // Try to match up the selection data target to something our
  // transferable provides.
  GUniquePtr<gchar> target_name(gdk_atom_name(selectionTarget));
  if (!target_name) {
    LOGCLIP("  Failed to get target name!\n");
    return;
  }

  rv = trans->GetTransferData(target_name.get(), getter_AddRefs(item));
  // nothing found?
  if (NS_FAILED(rv) || !item) {
    LOGCLIP("  Failed to get anything from GetTransferData()!\n");
    return;
  }

  void* primitive_data = nullptr;
  uint32_t dataLen = 0;
  nsPrimitiveHelpers::CreateDataFromPrimitive(
      nsDependentCString(target_name.get()), item, &primitive_data, &dataLen);
  if (!primitive_data) {
    LOGCLIP("  Failed to get primitive data!\n");
    return;
  }

  LOGCLIP("  Setting %s as a primitive data type, %d bytes\n",
          target_name.get(), dataLen);
  gtk_selection_data_set(aSelectionData, selectionTarget,
                         8, /* 8 bits in a unit */
                         (const guchar*)primitive_data, dataLen);
  free(primitive_data);
}

void nsClipboard::ClearCachedTargets(int32_t aWhichClipboard) {
  if (aWhichClipboard == kSelectionClipboard) {
    nsRetrievalContext::ClearCachedTargetsPrimary(nullptr, nullptr, nullptr);
  } else {
    nsRetrievalContext::ClearCachedTargetsClipboard(nullptr, nullptr, nullptr);
  }
}

void nsClipboard::SelectionClearEvent(GtkClipboard* aGtkClipboard) {
  int32_t whichClipboard = GetGeckoClipboardType(aGtkClipboard);
  if (whichClipboard < 0) {
    return;
  }
  LOGCLIP("nsClipboard::SelectionClearEvent (%s)\n",
          whichClipboard == kSelectionClipboard ? "primary" : "clipboard");
  ClearCachedTargets(whichClipboard);
  ClearTransferable(whichClipboard);
}

void clipboard_get_cb(GtkClipboard* aGtkClipboard,
                      GtkSelectionData* aSelectionData, guint info,
                      gpointer user_data) {
  LOGCLIP("clipboard_get_cb() callback\n");
  nsClipboard* aClipboard = static_cast<nsClipboard*>(user_data);
  aClipboard->SelectionGetEvent(aGtkClipboard, aSelectionData);
}

void clipboard_clear_cb(GtkClipboard* aGtkClipboard, gpointer user_data) {
  LOGCLIP("clipboard_clear_cb() callback\n");
  nsClipboard* aClipboard = static_cast<nsClipboard*>(user_data);
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
bool ConvertHTMLtoUCS2(Span<const char> aData, nsCString& charset,
                       char16_t** unicodeData, int32_t& outUnicodeLen) {
  if (charset.EqualsLiteral("UTF-16")) {  // current mozilla
    outUnicodeLen = (aData.Length() / 2) - 1;
    *unicodeData = reinterpret_cast<char16_t*>(
        moz_xmalloc((outUnicodeLen + sizeof('\0')) * sizeof(char16_t)));
    memcpy(*unicodeData, aData.data() + sizeof(char16_t),
           outUnicodeLen * sizeof(char16_t));
    (*unicodeData)[outUnicodeLen] = '\0';
    return true;
  }
  if (charset.EqualsLiteral("UNKNOWN")) {
    outUnicodeLen = 0;
    return false;
  }
  // app which use "text/html" to copy&paste
  // get the decoder
  auto encoding = Encoding::ForLabelNoReplacement(charset);
  if (!encoding) {
    LOGCLIP("ConvertHTMLtoUCS2: get unicode decoder error\n");
    outUnicodeLen = 0;
    return false;
  }

  // Remove kHTMLMarkupPrefix again, it won't necessarily cause any
  // issues, but might confuse other users.
  const size_t prefixLen = ArrayLength(kHTMLMarkupPrefix) - 1;
  if (aData.Length() >= prefixLen && nsDependentCSubstring(aData.To(prefixLen))
                                         .EqualsLiteral(kHTMLMarkupPrefix)) {
    aData = aData.From(prefixLen);
  }

  auto decoder = encoding->NewDecoder();
  CheckedInt<size_t> needed = decoder->MaxUTF16BufferLength(aData.Length());
  if (!needed.isValid() || needed.value() > INT32_MAX) {
    outUnicodeLen = 0;
    return false;
  }

  outUnicodeLen = 0;
  if (needed.value()) {
    *unicodeData = reinterpret_cast<char16_t*>(
        moz_xmalloc((needed.value() + 1) * sizeof(char16_t)));
    uint32_t result;
    size_t read;
    size_t written;
    std::tie(result, read, written, std::ignore) = decoder->DecodeToUTF16(
        AsBytes(aData), Span(*unicodeData, needed.value()), true);
    MOZ_ASSERT(result == kInputEmpty);
    MOZ_ASSERT(read == size_t(aData.Length()));
    MOZ_ASSERT(written <= needed.value());
    outUnicodeLen = written;
    // null terminate.
    (*unicodeData)[outUnicodeLen] = '\0';
    return true;
  }  // if valid length
  return false;
}

/*
 * get "charset" information from clipboard data
 * return value can be:
 *  1. "UTF-16":      mozilla or "text/html" with "charset=utf-16"
 *  2. "UNKNOWN":     mozilla can't detect what encode it use
 *  3. other:         "text/html" with other charset than utf-16
 */
bool GetHTMLCharset(Span<const char> aData, nsCString& str) {
  // if detect "FFFE" or "FEFF", assume UTF-16
  {
    char16_t* beginChar = (char16_t*)aData.data();
    if ((beginChar[0] == 0xFFFE) || (beginChar[0] == 0xFEFF)) {
      str.AssignLiteral("UTF-16");
      LOGCLIP("GetHTMLCharset: Charset of HTML is UTF-16\n");
      return true;
    }
  }

  // no "FFFE" and "FEFF", assume ASCII first to find "charset" info
  const nsDependentCSubstring htmlStr(aData);
  nsACString::const_iterator start, end;
  htmlStr.BeginReading(start);
  htmlStr.EndReading(end);
  nsACString::const_iterator valueStart(start), valueEnd(start);

  if (CaseInsensitiveFindInReadable("CONTENT=\"text/html;"_ns, start, end)) {
    start = end;
    htmlStr.EndReading(end);

    if (CaseInsensitiveFindInReadable("charset="_ns, start, end)) {
      valueStart = end;
      start = end;
      htmlStr.EndReading(end);

      if (FindCharInReadable('"', start, end)) valueEnd = start;
    }
  }
  // find "charset" in HTML
  if (valueStart != valueEnd) {
    str = Substring(valueStart, valueEnd);
    ToUpperCase(str);
    LOGCLIP("GetHTMLCharset: Charset of HTML = %s\n", str.get());
    return true;
  }
  str.AssignLiteral("UNKNOWN");
  LOGCLIP("GetHTMLCharset: Failed to get HTML Charset!\n");
  return false;
}
