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
#  include "nsWaylandDisplay.h"
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
static void clipboard_get_cb(GtkClipboard* aGtkClipboard,
                             GtkSelectionData* aSelectionData, guint info,
                             gpointer user_data);

// Callback when someone asks us to clear a clipboard
static void clipboard_clear_cb(GtkClipboard* aGtkClipboard, gpointer user_data);

// Callback when owner of clipboard data is changed
static void clipboard_owner_change_cb(GtkClipboard* aGtkClipboard,
                                      GdkEventOwnerChange* aEvent,
                                      gpointer aUserData);

static bool GetHTMLCharset(Span<const char> aData, nsCString& str);

static void SetTransferableData(nsITransferable* aTransferable,
                                const nsACString& aFlavor,
                                const char* aClipboardData,
                                uint32_t aClipboardDataLength) {
  LOGCLIP("SetTransferableData MIME %s\n", PromiseFlatCString(aFlavor).get());
  nsCOMPtr<nsISupports> wrapper;
  nsPrimitiveHelpers::CreatePrimitiveForData(
      aFlavor, aClipboardData, aClipboardDataLength, getter_AddRefs(wrapper));
  aTransferable->SetTransferData(PromiseFlatCString(aFlavor).get(), wrapper);
}

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

nsRetrievalContext::~nsRetrievalContext() {
  sClipboardTargets.Clear();
  sPrimaryTargets.Clear();
}

nsClipboard::nsClipboard()
    : nsBaseClipboard(mozilla::dom::ClipboardCapabilities(
#ifdef MOZ_WAYLAND
          widget::GdkIsWaylandDisplay()
              ? widget::WaylandDisplayGet()->IsPrimarySelectionEnabled()
              : true,
#else
          true, /* supportsSelectionClipboard */
#endif
          false /* supportsFindClipboard */,
          false /* supportsSelectionCache */)) {
  g_signal_connect(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), "owner-change",
                   G_CALLBACK(clipboard_owner_change_cb), this);
  g_signal_connect(gtk_clipboard_get(GDK_SELECTION_PRIMARY), "owner-change",
                   G_CALLBACK(clipboard_owner_change_cb), this);
}

nsClipboard::~nsClipboard() {
  g_signal_handlers_disconnect_by_func(
      gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
      FuncToGpointer(clipboard_owner_change_cb), this);
  g_signal_handlers_disconnect_by_func(
      gtk_clipboard_get(GDK_SELECTION_PRIMARY),
      FuncToGpointer(clipboard_owner_change_cb), this);
}

NS_IMPL_ISUPPORTS_INHERITED(nsClipboard, nsBaseClipboard, nsIObserver)

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
      NS_NewRunnableFunction("gtk_clipboard_store()", []() {
        LOGCLIP("nsClipboard storing clipboard content\n");
        gtk_clipboard_store(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
      }));
}

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  // See if we can short cut
  if ((aWhichClipboard == kGlobalClipboard &&
       aTransferable == mGlobalTransferable.get()) ||
      (aWhichClipboard == kSelectionClipboard &&
       aTransferable == mSelectionTransferable.get())) {
    return NS_OK;
  }

  LOGCLIP("nsClipboard::SetNativeClipboardData (%s)\n",
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

    // Special case text/plain since we can handle all of the string types.
    if (flavorStr.EqualsLiteral(kTextMime)) {
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

    if (flavorStr.EqualsLiteral(kFileMime)) {
      LOGCLIP("    adding text/uri-list target\n");
      GdkAtom atom = gdk_atom_intern(kURIListMime, FALSE);
      gtk_target_list_add(list, atom, 0, 0);
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
    EmptyNativeClipboardData(aWhichClipboard);
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
      mSelectionSequenceNumber++;
      mSelectionTransferable = aTransferable;
    } else {
      mGlobalSequenceNumber++;
      mGlobalTransferable = aTransferable;
      gtk_clipboard_set_can_store(gtkClipboard, gtkTargets, numTargets);
    }

    rv = NS_OK;
  } else {
    LOGCLIP("    gtk_clipboard_set_with_data() failed!\n");
    EmptyNativeClipboardData(aWhichClipboard);
    rv = NS_ERROR_FAILURE;
  }

  gtk_target_table_free(gtkTargets, numTargets);
  gtk_target_list_unref(list);

  return rv;
}

mozilla::Result<int32_t, nsresult>
nsClipboard::GetNativeClipboardSequenceNumber(int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));
  return aWhichClipboard == kSelectionClipboard ? mSelectionSequenceNumber
                                                : mGlobalSequenceNumber;
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

static nsresult GetTransferableFlavors(nsITransferable* aTransferable,
                                       nsTArray<nsCString>& aFlavors) {
  if (!aTransferable) {
    return NS_ERROR_FAILURE;
  }
  // Get a list of flavors this transferable can import
  nsresult rv = aTransferable->FlavorsTransferableCanImport(aFlavors);
  if (NS_FAILED(rv)) {
    LOGCLIP("  FlavorsTransferableCanImport falied!\n");
    return rv;
  }
#ifdef MOZ_LOGGING
  LOGCLIP("  Flavors which can be imported:");
  for (const auto& flavor : aFlavors) {
    LOGCLIP("    %s", flavor.get());
  }
#endif
  return NS_OK;
}

static bool TransferableSetFile(nsITransferable* aTransferable,
                                const nsACString& aURIList) {
  nsresult rv;
  nsTArray<nsCString> uris = mozilla::widget::ParseTextURIList(aURIList);
  if (!uris.IsEmpty()) {
    nsCOMPtr<nsIURI> fileURI;
    NS_NewURI(getter_AddRefs(fileURI), uris[0]);
    if (nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(fileURI, &rv)) {
      nsCOMPtr<nsIFile> file;
      rv = fileURL->GetFile(getter_AddRefs(file));
      if (NS_SUCCEEDED(rv)) {
        aTransferable->SetTransferData(kFileMime, file);
        LOGCLIP("  successfully set file to clipboard\n");
        return true;
      }
    }
  }
  return false;
}

static bool TransferableSetHTML(nsITransferable* aTransferable,
                                Span<const char> aData) {
  nsLiteralCString mimeType(kHTMLMime);

  // Convert text/html into our text format
  nsAutoCString charset;
  if (!GetHTMLCharset(aData, charset)) {
    // Fall back to utf-8 in case html/data is missing kHTMLMarkupPrefix.
    LOGCLIP("Failed to get html/text encoding, fall back to utf-8.\n");
    charset.AssignLiteral("utf-8");
  }

  LOGCLIP("TransferableSetHTML: HTML detected charset %s", charset.get());
  // app which use "text/html" to copy&paste
  // get the decoder
  auto encoding = Encoding::ForLabelNoReplacement(charset);
  if (!encoding) {
    LOGCLIP("TransferableSetHTML: get unicode decoder error (charset: %s)",
            charset.get());
    return false;
  }

  // According to spec html UTF-16BE/LE should be switched to UTF-8
  // https://html.spec.whatwg.org/#determining-the-character-encoding:utf-16-encoding-2
  if (encoding == UTF_16LE_ENCODING || encoding == UTF_16BE_ENCODING) {
    encoding = UTF_8_ENCODING;
  }

  // Remove kHTMLMarkupPrefix again, it won't necessarily cause any
  // issues, but might confuse other users.
  const size_t prefixLen = ArrayLength(kHTMLMarkupPrefix) - 1;
  if (aData.Length() >= prefixLen && nsDependentCSubstring(aData.To(prefixLen))
                                         .EqualsLiteral(kHTMLMarkupPrefix)) {
    aData = aData.From(prefixLen);
  }

  nsAutoString unicodeData;
  auto [rv, enc] = encoding->Decode(AsBytes(aData), unicodeData);
#if MOZ_LOGGING
  if (enc != UTF_8_ENCODING &&
      MOZ_LOG_TEST(gClipboardLog, mozilla::LogLevel::Debug)) {
    nsCString decoderName;
    enc->Name(decoderName);
    LOGCLIP("TransferableSetHTML: expected UTF-8 decoder but got %s",
            decoderName.get());
  }
#endif
  if (NS_FAILED(rv)) {
    LOGCLIP("TransferableSetHTML: failed to decode HTML");
    return false;
  }
  SetTransferableData(aTransferable, mimeType,
                      (const char*)unicodeData.BeginReading(),
                      unicodeData.Length() * sizeof(char16_t));
  return true;
}

NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  LOGCLIP("nsClipboard::GetNativeClipboardData (%s)\n",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  // TODO: Ensure we don't re-enter here.
  if (!mContext) {
    return NS_ERROR_FAILURE;
  }

  nsTArray<nsCString> flavors;
  nsresult rv = GetTransferableFlavors(aTransferable, flavors);
  NS_ENSURE_SUCCESS(rv, rv);

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

    // Special case text/plain since we can convert any
    // string into text/plain
    if (flavorStr.EqualsLiteral(kTextMime)) {
      LOGCLIP("    Getting text %s MIME clipboard data\n", flavorStr.get());

      auto clipboardData = mContext->GetClipboardText(aWhichClipboard);
      if (!clipboardData) {
        LOGCLIP("    failed to get text data\n");
        // If the type was text/plain and we couldn't get
        // text off the clipboard, run the next loop
        // iteration.
        continue;
      }

      // Convert utf-8 into our text format.
      NS_ConvertUTF8toUTF16 ucs2string(clipboardData.get());
      SetTransferableData(aTransferable, flavorStr,
                          (const char*)ucs2string.BeginReading(),
                          ucs2string.Length() * 2);

      LOGCLIP("    got text data, length %zd\n", ucs2string.Length());
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

      nsDependentCSubstring fileName(clipboardData.AsSpan());
      if (!TransferableSetFile(aTransferable, fileName)) {
        continue;
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
        if (!TransferableSetHTML(aTransferable, clipboardData.AsSpan())) {
          continue;
        }
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

enum DataType {
  DATATYPE_IMAGE,
  DATATYPE_FILE,
  DATATYPE_HTML,
  DATATYPE_RAW,
};

struct DataCallbackHandler {
  RefPtr<nsITransferable> mTransferable;
  nsBaseClipboard::GetDataCallback mDataCallback;
  nsCString mMimeType;
  DataType mDataType;

  explicit DataCallbackHandler(RefPtr<nsITransferable> aTransferable,
                               nsBaseClipboard::GetDataCallback&& aDataCallback,
                               const char* aMimeType,
                               DataType aDataType = DATATYPE_RAW)
      : mTransferable(std::move(aTransferable)),
        mDataCallback(std::move(aDataCallback)),
        mMimeType(aMimeType),
        mDataType(aDataType) {
    MOZ_COUNT_CTOR(DataCallbackHandler);
    LOGCLIP("DataCallbackHandler created [%p] MIME %s type %d", this,
            mMimeType.get(), mDataType);
  }
  ~DataCallbackHandler() {
    LOGCLIP("DataCallbackHandler deleted [%p]", this);
    MOZ_COUNT_DTOR(DataCallbackHandler);
  }
};

static void AsyncGetTextImpl(nsITransferable* aTransferable,
                             int32_t aWhichClipboard,
                             nsBaseClipboard::GetDataCallback&& aCallback) {
  LOGCLIP("AsyncGetText() type '%s'",
          aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard");

  gtk_clipboard_request_text(
      gtk_clipboard_get(GetSelectionAtom(aWhichClipboard)),
      [](GtkClipboard* aClipboard, const gchar* aText, gpointer aData) -> void {
        UniquePtr<DataCallbackHandler> ref(
            static_cast<DataCallbackHandler*>(aData));
        LOGCLIP("AsyncGetText async handler of [%p]", aData);

        size_t dataLength = aText ? strlen(aText) : 0;
        if (dataLength <= 0) {
          LOGCLIP("  quit, text is not available");
          ref->mDataCallback(NS_OK);
          return;
        }

        // Convert utf-8 into our unicode format.
        NS_ConvertUTF8toUTF16 utf16string(aText, dataLength);
        nsLiteralCString flavor(kTextMime);
        SetTransferableData(ref->mTransferable, flavor,
                            (const char*)utf16string.BeginReading(),
                            utf16string.Length() * 2);
        LOGCLIP("  text is set, length = %d", (int)dataLength);
        ref->mDataCallback(NS_OK);
      },
      new DataCallbackHandler(aTransferable, std::move(aCallback), kTextMime));
}

static void AsyncGetDataImpl(nsITransferable* aTransferable,
                             int32_t aWhichClipboard, const char* aMimeType,
                             DataType aDataType,
                             nsBaseClipboard::GetDataCallback&& aCallback) {
  LOGCLIP("AsyncGetData() type '%s'",
          aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard");

  const char* gtkMIMEType = nullptr;
  switch (aDataType) {
    case DATATYPE_FILE:
      // Don't ask Gtk for application/x-moz-file
      gtkMIMEType = kURIListMime;
      break;
    case DATATYPE_IMAGE:
    case DATATYPE_HTML:
    case DATATYPE_RAW:
      gtkMIMEType = aMimeType;
      break;
  }

  gtk_clipboard_request_contents(
      gtk_clipboard_get(GetSelectionAtom(aWhichClipboard)),
      gdk_atom_intern(gtkMIMEType, FALSE),
      [](GtkClipboard* aClipboard, GtkSelectionData* aSelection,
         gpointer aData) -> void {
        UniquePtr<DataCallbackHandler> ref(
            static_cast<DataCallbackHandler*>(aData));
        LOGCLIP("AsyncGetData async handler [%p] MIME %s type %d", aData,
                ref->mMimeType.get(), ref->mDataType);

        int dataLength = gtk_selection_data_get_length(aSelection);
        if (dataLength <= 0) {
          ref->mDataCallback(NS_OK);
          return;
        }
        const char* data = (const char*)gtk_selection_data_get_data(aSelection);
        if (!data) {
          ref->mDataCallback(NS_OK);
          return;
        }
        switch (ref->mDataType) {
          case DATATYPE_IMAGE: {
            LOGCLIP("  set image clipboard data");
            nsCOMPtr<nsIInputStream> byteStream;
            NS_NewByteInputStream(getter_AddRefs(byteStream),
                                  Span(data, dataLength), NS_ASSIGNMENT_COPY);
            ref->mTransferable->SetTransferData(ref->mMimeType.get(),
                                                byteStream);
            break;
          }
          case DATATYPE_FILE: {
            LOGCLIP("  set file clipboard data");
            nsDependentCSubstring file(data, dataLength);
            TransferableSetFile(ref->mTransferable, file);
            break;
          }
          case DATATYPE_HTML: {
            LOGCLIP("  html clipboard data");
            Span dataSpan(data, dataLength);
            TransferableSetHTML(ref->mTransferable, dataSpan);
            break;
          }
          case DATATYPE_RAW: {
            LOGCLIP("  raw clipboard data %s", ref->mMimeType.get());
            SetTransferableData(ref->mTransferable, ref->mMimeType, data,
                                dataLength);
            break;
          }
        }
        ref->mDataCallback(NS_OK);
      },
      new DataCallbackHandler(aTransferable, std::move(aCallback), aMimeType,
                              aDataType));
}

static void AsyncGetDataFlavor(nsITransferable* aTransferable,
                               int32_t aWhichClipboard, nsCString& aFlavorStr,
                               nsBaseClipboard::GetDataCallback&& aCallback) {
  if (aFlavorStr.EqualsLiteral(kJPEGImageMime) ||
      aFlavorStr.EqualsLiteral(kJPGImageMime) ||
      aFlavorStr.EqualsLiteral(kPNGImageMime) ||
      aFlavorStr.EqualsLiteral(kGIFImageMime)) {
    // Emulate support for image/jpg
    if (aFlavorStr.EqualsLiteral(kJPGImageMime)) {
      aFlavorStr.Assign(kJPEGImageMime);
    }
    LOGCLIP("  Getting image %s MIME clipboard data", aFlavorStr.get());
    AsyncGetDataImpl(aTransferable, aWhichClipboard, aFlavorStr.get(),
                     DATATYPE_IMAGE, std::move(aCallback));
    return;
  }
  // Special case text/plain since we can convert any
  // string into text/plain
  if (aFlavorStr.EqualsLiteral(kTextMime)) {
    LOGCLIP("  Getting unicode clipboard data");
    AsyncGetTextImpl(aTransferable, aWhichClipboard, std::move(aCallback));
    return;
  }
  if (aFlavorStr.EqualsLiteral(kFileMime)) {
    LOGCLIP("  Getting file clipboard data\n");
    AsyncGetDataImpl(aTransferable, aWhichClipboard, aFlavorStr.get(),
                     DATATYPE_FILE, std::move(aCallback));
    return;
  }
  if (aFlavorStr.EqualsLiteral(kHTMLMime)) {
    LOGCLIP("  Getting HTML clipboard data");
    AsyncGetDataImpl(aTransferable, aWhichClipboard, aFlavorStr.get(),
                     DATATYPE_HTML, std::move(aCallback));
    return;
  }
  LOGCLIP("  Getting raw %s MIME clipboard data\n", aFlavorStr.get());
  AsyncGetDataImpl(aTransferable, aWhichClipboard, aFlavorStr.get(),
                   DATATYPE_RAW, std::move(aCallback));
}

void nsClipboard::AsyncGetNativeClipboardData(nsITransferable* aTransferable,
                                              int32_t aWhichClipboard,
                                              GetDataCallback&& aCallback) {
  MOZ_DIAGNOSTIC_ASSERT(aTransferable);
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  LOGCLIP("nsClipboard::AsyncGetNativeClipboardData (%s)",
          aWhichClipboard == nsClipboard::kSelectionClipboard ? "primary"
                                                              : "clipboard");
  nsTArray<nsCString> importedFlavors;
  nsresult rv = GetTransferableFlavors(aTransferable, importedFlavors);
  if (NS_FAILED(rv)) {
    aCallback(rv);
    return;
  }

  auto flavorsNum = importedFlavors.Length();
  if (!flavorsNum) {
    aCallback(NS_OK);
    return;
  }
#ifdef MOZ_LOGGING
  if (flavorsNum > 1) {
    LOGCLIP("  Only first MIME type (%s) will be imported from clipboard!",
            importedFlavors[0].get());
  }
#endif

  // Filter out MIME types on X11 to prevent unwanted conversions,
  // see Bug 1611407
  if (widget::GdkIsX11Display()) {
    AsyncHasNativeClipboardDataMatchingFlavors(
        importedFlavors, aWhichClipboard,
        [aWhichClipboard, transferable = nsCOMPtr{aTransferable},
         callback = std::move(aCallback)](auto aResultOrError) mutable {
          if (aResultOrError.isErr()) {
            callback(aResultOrError.unwrapErr());
            return;
          }

          nsTArray<nsCString> clipboardFlavors =
              std::move(aResultOrError.unwrap());
          if (!clipboardFlavors.Length()) {
            LOGCLIP("  no flavors in clipboard, quit.");
            callback(NS_OK);
            return;
          }

          AsyncGetDataFlavor(transferable, aWhichClipboard, clipboardFlavors[0],
                             std::move(callback));
        });
    return;
  }

  // Read clipboard directly on Wayland
  AsyncGetDataFlavor(aTransferable, aWhichClipboard, importedFlavors[0],
                     std::move(aCallback));
}

nsresult nsClipboard::EmptyNativeClipboardData(int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  LOGCLIP("nsClipboard::EmptyNativeClipboardData (%s)\n",
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
    mSelectionSequenceNumber++;
    mSelectionTransferable = nullptr;
  } else {
    mGlobalSequenceNumber++;
    mGlobalTransferable = nullptr;
  }
}

static bool FlavorMatchesTarget(const nsACString& aFlavor, GdkAtom aTarget) {
  GUniquePtr<gchar> atom_name(gdk_atom_name(aTarget));
  if (!atom_name) {
    return false;
  }
  if (aFlavor.Equals(atom_name.get())) {
    LOGCLIP("    has %s\n", atom_name.get());
    return true;
  }
  // X clipboard supports image/jpeg, but we want to emulate support
  // for image/jpg as well
  if (aFlavor.EqualsLiteral(kJPGImageMime) &&
      !strcmp(atom_name.get(), kJPEGImageMime)) {
    LOGCLIP("    has image/jpg\n");
    return true;
  }
  // application/x-moz-file should be treated like text/uri-list
  if (aFlavor.EqualsLiteral(kFileMime) &&
      !strcmp(atom_name.get(), kURIListMime)) {
    LOGCLIP("    has text/uri-list treating as application/x-moz-file");
    return true;
  }
  return false;
}

mozilla::Result<bool, nsresult>
nsClipboard::HasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  LOGCLIP("nsClipboard::HasNativeClipboardDataMatchingFlavors (%s)\n",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  if (!mContext) {
    return Err(NS_ERROR_FAILURE);
  }

  auto targets = mContext->GetTargets(aWhichClipboard);
  if (!targets) {
    LOGCLIP("    no targes at clipboard (null)\n");
    return false;
  }

#ifdef MOZ_LOGGING
  if (LOGCLIP_ENABLED()) {
    LOGCLIP("    Asking for content:\n");
    for (auto& flavor : aFlavorList) {
      LOGCLIP("        MIME %s\n", flavor.get());
    }
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
  }
#endif

  // Walk through the provided types and try to match it to a
  // provided type.
  for (auto& flavor : aFlavorList) {
    // We special case text/plain here.
    if (flavor.EqualsLiteral(kTextMime) &&
        gtk_targets_include_text(targets.AsSpan().data(),
                                 targets.AsSpan().Length())) {
      LOGCLIP("    has kTextMime\n");
      return true;
    }
    for (const auto& target : targets.AsSpan()) {
      if (FlavorMatchesTarget(flavor, target)) {
        return true;
      }
    }
  }

  LOGCLIP("    no targes at clipboard (bad match)\n");
  return false;
}

struct TragetCallbackHandler {
  TragetCallbackHandler(const nsTArray<nsCString>& aAcceptedFlavorList,
                        nsBaseClipboard::HasMatchingFlavorsCallback&& aCallback)
      : mAcceptedFlavorList(aAcceptedFlavorList.Clone()),
        mCallback(std::move(aCallback)) {
    LOGCLIP("TragetCallbackHandler(%p) created", this);
  }
  ~TragetCallbackHandler() {
    LOGCLIP("TragetCallbackHandler(%p) deleted", this);
  }
  nsTArray<nsCString> mAcceptedFlavorList;
  nsBaseClipboard::HasMatchingFlavorsCallback mCallback;
};

void nsClipboard::AsyncHasNativeClipboardDataMatchingFlavors(
    const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard,
    nsBaseClipboard::HasMatchingFlavorsCallback&& aCallback) {
  MOZ_DIAGNOSTIC_ASSERT(
      nsIClipboard::IsClipboardTypeSupported(aWhichClipboard));

  LOGCLIP("nsClipboard::AsyncHasNativeClipboardDataMatchingFlavors (%s)",
          aWhichClipboard == kSelectionClipboard ? "primary" : "clipboard");

  gtk_clipboard_request_contents(
      gtk_clipboard_get(GetSelectionAtom(aWhichClipboard)),
      gdk_atom_intern("TARGETS", FALSE),
      [](GtkClipboard* aClipboard, GtkSelectionData* aSelection,
         gpointer aData) -> void {
        LOGCLIP("gtk_clipboard_request_contents async handler (%p)", aData);
        UniquePtr<TragetCallbackHandler> handler(
            static_cast<TragetCallbackHandler*>(aData));

        GdkAtom* targets = nullptr;
        gint targetsNum = 0;
        if (gtk_selection_data_get_length(aSelection) > 0) {
          gtk_selection_data_get_targets(aSelection, &targets, &targetsNum);
        }
        nsTArray<nsCString> results;
        if (targetsNum) {
          for (auto& flavor : handler->mAcceptedFlavorList) {
            LOGCLIP("  looking for %s", flavor.get());
            if (flavor.EqualsLiteral(kTextMime) &&
                gtk_targets_include_text(targets, targetsNum)) {
              results.AppendElement(flavor);
              LOGCLIP("    has kTextMime\n");
              continue;
            }
            for (int i = 0; i < targetsNum; i++) {
              if (FlavorMatchesTarget(flavor, targets[i])) {
                results.AppendElement(flavor);
              }
            }
          }
        }
        handler->mCallback(std::move(results));
      },
      new TragetCallbackHandler(aFlavorList, std::move(aCallback)));
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
  // it does, try to give it text/plain after converting it to
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
    LOGCLIP("  providing text/plain data\n");
    // Try to convert our internal type into a text string.  Get
    // the transferable for this clipboard and try to get the
    // text/plain type for it.
    rv = trans->GetTransferData("text/plain", getter_AddRefs(item));
    if (NS_FAILED(rv) || !item) {
      LOGCLIP("  GetTransferData() failed to get text/plain!\n");
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

    LOGCLIP("  Setting %zd bytes of %s data\n", html.Length(),
            GUniquePtr<gchar>(gdk_atom_name(selectionTarget)).get());
    gtk_selection_data_set(aSelectionData, selectionTarget, 8,
                           (const guchar*)html.get(), html.Length());
    return;
  }

  // We put kFileMime onto the clipboard as kURIListMime.
  if (selectionTarget == gdk_atom_intern(kURIListMime, FALSE)) {
    LOGCLIP("  providing %s data\n", kURIListMime);
    rv = trans->GetTransferData(kFileMime, getter_AddRefs(item));
    if (NS_FAILED(rv) || !item) {
      LOGCLIP("  failed to get %s data by GetTransferData()!\n", kFileMime);
      return;
    }

    nsCOMPtr<nsIFile> file = do_QueryInterface(item);
    if (!file) {
      LOGCLIP("  failed to get nsIFile interface!");
      return;
    }

    nsCOMPtr<nsIURI> fileURI;
    rv = NS_NewFileURI(getter_AddRefs(fileURI), file);
    if (NS_FAILED(rv)) {
      LOGCLIP("  failed to get fileURI\n");
      return;
    }

    nsAutoCString uri;
    if (NS_FAILED(fileURI->GetSpec(uri))) {
      LOGCLIP("  failed to get fileURI spec\n");
      return;
    }

    LOGCLIP("  Setting %zd bytes of data\n", uri.Length());
    gtk_selection_data_set(aSelectionData, selectionTarget, 8,
                           (const guchar*)uri.get(), uri.Length());
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
  ClearClipboardCache(whichClipboard);
}

void nsClipboard::OwnerChangedEvent(GtkClipboard* aGtkClipboard,
                                    GdkEventOwnerChange* aEvent) {
  int32_t whichClipboard = GetGeckoClipboardType(aGtkClipboard);
  if (whichClipboard < 0) {
    return;
  }
  LOGCLIP("nsClipboard::OwnerChangedEvent (%s)\n",
          whichClipboard == kSelectionClipboard ? "primary" : "clipboard");
  GtkWidget* gtkWidget = [aEvent]() -> GtkWidget* {
    if (!aEvent->owner) {
      return nullptr;
    }
    gpointer user_data = nullptr;
    gdk_window_get_user_data(aEvent->owner, &user_data);
    return GTK_WIDGET(user_data);
  }();
  // If we can get GtkWidget from the current clipboard owner, this
  // owner-changed event must be triggered by ourself via calling
  // gtk_clipboard_set_with_data, the sequence number should already be handled.
  if (!gtkWidget) {
    if (whichClipboard == kSelectionClipboard) {
      mSelectionSequenceNumber++;
    } else {
      mGlobalSequenceNumber++;
    }
  }

  ClearCachedTargets(whichClipboard);
}

void clipboard_get_cb(GtkClipboard* aGtkClipboard,
                      GtkSelectionData* aSelectionData, guint info,
                      gpointer user_data) {
  LOGCLIP("clipboard_get_cb() callback\n");
  nsClipboard* clipboard = static_cast<nsClipboard*>(user_data);
  clipboard->SelectionGetEvent(aGtkClipboard, aSelectionData);
}

void clipboard_clear_cb(GtkClipboard* aGtkClipboard, gpointer user_data) {
  LOGCLIP("clipboard_clear_cb() callback\n");
  nsClipboard* clipboard = static_cast<nsClipboard*>(user_data);
  clipboard->SelectionClearEvent(aGtkClipboard);
}

void clipboard_owner_change_cb(GtkClipboard* aGtkClipboard,
                               GdkEventOwnerChange* aEvent,
                               gpointer aUserData) {
  LOGCLIP("clipboard_owner_change_cb() callback\n");
  nsClipboard* clipboard = static_cast<nsClipboard*>(aUserData);
  clipboard->OwnerChangedEvent(aGtkClipboard, aEvent);
}

/*
 * This function extracts the encoding label from the subset of HTML internal
 * encoding declaration syntax that uses the old long form with double quotes
 * and without spaces around the equals sign between the "content" attribute
 * name and the attribute value.
 *
 * This was added for the sake of an ancient version of StarOffice
 * in the pre-UTF-8 era in bug 123389. It is unclear if supporting
 * non-UTF-8 encodings is still necessary and if this function
 * still needs to exist.
 *
 * As of December 2022, both Gecko and LibreOffice emit an UTF-8
 * declaration that this function successfully extracts "UTF-8" from,
 * but that's also the default that we fall back on if this function
 * fails to extract a label.
 */
bool GetHTMLCharset(Span<const char> aData, nsCString& aFoundCharset) {
  // Assume ASCII first to find "charset" info
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
    aFoundCharset = Substring(valueStart, valueEnd);
    ToUpperCase(aFoundCharset);
    return true;
  }
  return false;
}
