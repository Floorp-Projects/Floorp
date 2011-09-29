/* vim:set ts=2 sw=2 sts=2 cin et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla icon channel for gnome.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Nils Maier <MaierMan@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <unistd.h>

#ifdef MOZ_ENABLE_GNOMEUI
// Older versions of these headers seem to be missing an extern "C"
extern "C" {
#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-icon-theme.h>
#include <libgnomeui/gnome-icon-lookup.h>

#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>
}
#endif
#ifdef MOZ_ENABLE_GIO
#include <gio/gio.h>
#endif

#include <gtk/gtk.h>

#include "nsIMIMEService.h"

#include "nsIStringBundle.h"

#include "nsNetUtil.h"
#include "nsIURL.h"
#include "prlink.h"

#include "mozilla/Util.h" // for DebugOnly

#include "nsIconChannel.h"

NS_IMPL_ISUPPORTS2(nsIconChannel,
                   nsIRequest,
                   nsIChannel)

#ifdef MOZ_ENABLE_GNOMEUI
// These let us have a soft dependency on libgnomeui rather than a hard one. These are just basically the prototypes
// of the functions in the libraries.
typedef char* (*_GnomeIconLookup_fn)(GtkIconTheme *icon_theme, GnomeThumbnailFactory *thumbnail_factory,
                                     const char *file_uri, const char *custom_icon, GnomeVFSFileInfo *file_info,
                                     const char *mime_type, GnomeIconLookupFlags flags, GnomeIconLookupResultFlags *result);
typedef GnomeIconTheme* (*_GnomeIconThemeNew_fn)(void);
typedef int (*_GnomeInit_fn)(const char *app_id, const char *app_version, int argc, char **argv, const struct poptOption *options,
                             int flags, poptContext *return_ctx);
typedef GnomeProgram* (*_GnomeProgramGet_fn)(void);
typedef GnomeVFSResult (*_GnomeVFSGetFileInfo_fn)(const gchar *text_uri, GnomeVFSFileInfo *info, GnomeVFSFileInfoOptions options);
typedef void (*_GnomeVFSFileInfoClear_fn)(GnomeVFSFileInfo *info);

static PRLibrary* gLibGnomeUI = nsnull;
static PRLibrary* gLibGnome = nsnull;
static PRLibrary* gLibGnomeVFS = nsnull;
static bool gTriedToLoadGnomeLibs = false;

static _GnomeIconLookup_fn _gnome_icon_lookup = nsnull;
static _GnomeIconThemeNew_fn _gnome_icon_theme_new = nsnull;
static _GnomeInit_fn _gnome_init = nsnull;
static _GnomeProgramGet_fn _gnome_program_get = nsnull;
static _GnomeVFSGetFileInfo_fn _gnome_vfs_get_file_info = nsnull;
static _GnomeVFSFileInfoClear_fn _gnome_vfs_file_info_clear = nsnull;
#endif //MOZ_ENABLE_GNOMEUI

static nsresult
moz_gdk_pixbuf_to_channel(GdkPixbuf* aPixbuf, nsIURI *aURI,
                          nsIChannel **aChannel)
{
  int width = gdk_pixbuf_get_width(aPixbuf);
  int height = gdk_pixbuf_get_height(aPixbuf);
  NS_ENSURE_TRUE(height < 256 && width < 256 && height > 0 && width > 0 &&
                 gdk_pixbuf_get_colorspace(aPixbuf) == GDK_COLORSPACE_RGB &&
                 gdk_pixbuf_get_bits_per_sample(aPixbuf) == 8 &&
                 gdk_pixbuf_get_has_alpha(aPixbuf) &&
                 gdk_pixbuf_get_n_channels(aPixbuf) == 4,
                 NS_ERROR_UNEXPECTED);

  const int n_channels = 4;
  gsize buf_size = 2 + n_channels * height * width;
  PRUint8 * const buf = (PRUint8*)NS_Alloc(buf_size);
  NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);
  PRUint8 *out = buf;

  *(out++) = width;
  *(out++) = height;

  const guchar * const pixels = gdk_pixbuf_get_pixels(aPixbuf);
  int rowextra = gdk_pixbuf_get_rowstride(aPixbuf) - width * n_channels;

  // encode the RGB data and the A data
  const guchar * in = pixels;
  for (int y = 0; y < height; ++y, in += rowextra) {
    for (int x = 0; x < width; ++x) {
      PRUint8 r = *(in++);
      PRUint8 g = *(in++);
      PRUint8 b = *(in++);
      PRUint8 a = *(in++);
#define DO_PREMULTIPLY(c_) PRUint8(PRUint16(c_) * PRUint16(a) / PRUint16(255))
#ifdef IS_LITTLE_ENDIAN
      *(out++) = DO_PREMULTIPLY(b);
      *(out++) = DO_PREMULTIPLY(g);
      *(out++) = DO_PREMULTIPLY(r);
      *(out++) = a;
#else
      *(out++) = a;
      *(out++) = DO_PREMULTIPLY(r);
      *(out++) = DO_PREMULTIPLY(g);
      *(out++) = DO_PREMULTIPLY(b);
#endif
#undef DO_PREMULTIPLY
    }
  }

  NS_ASSERTION(out == buf + buf_size, "size miscalculation");

  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream =
    do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = stream->AdoptData((char*)buf, buf_size);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewInputStreamChannel(aChannel, aURI, stream,
                                NS_LITERAL_CSTRING("image/icon"));
  return rv;
}

static GtkWidget *gProtoWindow = nsnull;
static GtkWidget *gStockImageWidget = nsnull;
#ifdef MOZ_ENABLE_GNOMEUI
static GnomeIconTheme *gIconTheme = nsnull;
#endif //MOZ_ENABLE_GNOMEUI

static void
ensure_stock_image_widget()
{
  // Only the style of the GtkImage needs to be used, but the widget is kept
  // to track dynamic style changes.
  if (!gProtoWindow) {
    gProtoWindow = gtk_window_new(GTK_WINDOW_POPUP);
    GtkWidget* protoLayout = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(gProtoWindow), protoLayout);

    gStockImageWidget = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(protoLayout), gStockImageWidget);

    gtk_widget_ensure_style(gStockImageWidget);
  }
}

#ifdef MOZ_ENABLE_GNOMEUI
static nsresult
ensure_libgnomeui()
{
  // Attempt to get the libgnomeui symbol references. We do it this way so that stock icons from Init()
  // don't get held back by InitWithGnome()'s libgnomeui dependency.
  if (!gTriedToLoadGnomeLibs) {
    gLibGnomeUI = PR_LoadLibrary("libgnomeui-2.so.0");
    if (!gLibGnomeUI)
      return NS_ERROR_NOT_AVAILABLE;

    _gnome_init = (_GnomeInit_fn)PR_FindFunctionSymbol(gLibGnomeUI, "gnome_init_with_popt_table");
    _gnome_icon_theme_new = (_GnomeIconThemeNew_fn)PR_FindFunctionSymbol(gLibGnomeUI, "gnome_icon_theme_new");
    _gnome_icon_lookup = (_GnomeIconLookup_fn)PR_FindFunctionSymbol(gLibGnomeUI, "gnome_icon_lookup");

    if (!_gnome_init || !_gnome_icon_theme_new || !_gnome_icon_lookup) {
      PR_UnloadLibrary(gLibGnomeUI);
      gLibGnomeUI = nsnull;
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (!gLibGnomeUI)
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

static nsresult
ensure_libgnome()
{
  if (!gTriedToLoadGnomeLibs) {
    gLibGnome = PR_LoadLibrary("libgnome-2.so.0");
    if (!gLibGnome)
      return NS_ERROR_NOT_AVAILABLE;

    _gnome_program_get = (_GnomeProgramGet_fn)PR_FindFunctionSymbol(gLibGnome, "gnome_program_get");
    if (!_gnome_program_get) {
      PR_UnloadLibrary(gLibGnome);
      gLibGnome = nsnull;
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (!gLibGnome)
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

static nsresult
ensure_libgnomevfs()
{
  if (!gTriedToLoadGnomeLibs) {
    gLibGnomeVFS = PR_LoadLibrary("libgnomevfs-2.so.0");
    if (!gLibGnomeVFS)
      return NS_ERROR_NOT_AVAILABLE;

    _gnome_vfs_get_file_info = (_GnomeVFSGetFileInfo_fn)PR_FindFunctionSymbol(gLibGnomeVFS, "gnome_vfs_get_file_info");
    _gnome_vfs_file_info_clear = (_GnomeVFSFileInfoClear_fn)PR_FindFunctionSymbol(gLibGnomeVFS, "gnome_vfs_file_info_clear");
    if (!_gnome_vfs_get_file_info || !_gnome_vfs_file_info_clear) {
      PR_UnloadLibrary(gLibGnomeVFS);
      gLibGnomeVFS = nsnull;
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  if (!gLibGnomeVFS)
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}
#endif //MOZ_ENABLE_GNOMEUI

static GtkIconSize
moz_gtk_icon_size(const char *name)
{
  if (strcmp(name, "button") == 0)
    return GTK_ICON_SIZE_BUTTON;

  if (strcmp(name, "menu") == 0)
    return GTK_ICON_SIZE_MENU;

  if (strcmp(name, "toolbar") == 0)
    return GTK_ICON_SIZE_LARGE_TOOLBAR;

  if (strcmp(name, "toolbarsmall") == 0)
    return GTK_ICON_SIZE_SMALL_TOOLBAR;

  if (strcmp(name, "dnd") == 0)
    return GTK_ICON_SIZE_DND;

  if (strcmp(name, "dialog") == 0)
    return GTK_ICON_SIZE_DIALOG;

  return GTK_ICON_SIZE_MENU;
}

static PRInt32
GetIconSize(nsIMozIconURI *aIconURI)
{
  nsCAutoString iconSizeString;

  aIconURI->GetIconSize(iconSizeString);
  if (iconSizeString.IsEmpty()) {
    PRUint32 size;
    mozilla::DebugOnly<nsresult> rv = aIconURI->GetImageSize(&size);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetImageSize failed");
    return size; 
  } else {
    int size;

    GtkIconSize icon_size = moz_gtk_icon_size(iconSizeString.get());
    gtk_icon_size_lookup(icon_size, &size, NULL);
    return size;
  }
}

/* Scale icon buffer to preferred size */
static nsresult
ScaleIconBuf(GdkPixbuf **aBuf, PRInt32 iconSize)
{
  // Scale buffer only if width or height differ from preferred size
  if (gdk_pixbuf_get_width(*aBuf)  != iconSize &&
      gdk_pixbuf_get_height(*aBuf) != iconSize) {
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(*aBuf, iconSize, iconSize,
                                                GDK_INTERP_BILINEAR);
    // replace original buffer by scaled
    g_object_unref(*aBuf);
    *aBuf = scaled;
    if (!scaled)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

#ifdef MOZ_ENABLE_GNOMEUI
nsresult
nsIconChannel::InitWithGnome(nsIMozIconURI *aIconURI)
{
  nsresult rv;

  if (NS_FAILED(ensure_libgnomeui()) || NS_FAILED(ensure_libgnome()) || NS_FAILED(ensure_libgnomevfs())) {
    gTriedToLoadGnomeLibs = PR_TRUE;
    return NS_ERROR_NOT_AVAILABLE;
  }

  gTriedToLoadGnomeLibs = PR_TRUE;

  if (!_gnome_program_get()) {
    // Get the brandShortName from the string bundle to pass to GNOME
    // as the application name.  This may be used for things such as
    // the title of grouped windows in the panel.
    nsCOMPtr<nsIStringBundleService> bundleService = 
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);

    NS_ASSERTION(bundleService, "String bundle service must be present!");

    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                getter_AddRefs(bundle));
    nsAutoString appName;

    if (bundle) {
      bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                getter_Copies(appName));
    } else {
      NS_WARNING("brand.properties not present, using default application name");
      appName.Assign(NS_LITERAL_STRING("Gecko"));
    }

    char* empty[] = { "" };
    _gnome_init(NS_ConvertUTF16toUTF8(appName).get(), "1.0", 1, empty, NULL, 0, NULL);
  }

  PRUint32 iconSize = GetIconSize(aIconURI);
  nsCAutoString type;
  aIconURI->GetContentType(type);

  GnomeVFSFileInfo fileInfo = {0};
  fileInfo.refcount = 1; // In case some GnomeVFS function addrefs and releases it

  nsCAutoString spec;
  nsCOMPtr<nsIURL> url;
  rv = aIconURI->GetIconURL(getter_AddRefs(url));
  if (url) {
    url->GetAsciiSpec(spec);
    // Only ask gnome-vfs for a GnomeVFSFileInfo for file: uris, to avoid a
    // network request
    bool isFile;
    if (NS_SUCCEEDED(url->SchemeIs("file", &isFile)) && isFile) {
      _gnome_vfs_get_file_info(spec.get(), &fileInfo, GNOME_VFS_FILE_INFO_DEFAULT);
    }
    else {
      // The filename we get is UTF-8-compatible, which matches gnome expectations.
      // See also: http://lists.gnome.org/archives/gnome-vfs-list/2004-March/msg00049.html
      // "Whenever we can detect the charset used for the URI type we try to
      //  convert it to/from utf8 automatically inside gnome-vfs."
      // I'll interpret that as "otherwise, this field is random junk".
      nsCAutoString name;
      url->GetFileName(name);
      fileInfo.name = g_strdup(name.get());

      if (!type.IsEmpty()) {
        fileInfo.valid_fields = GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
        fileInfo.mime_type = g_strdup(type.get());
      }
    }
  }

  if (type.IsEmpty()) {
    nsCOMPtr<nsIMIMEService> ms(do_GetService("@mozilla.org/mime;1"));
    if (ms) {
      nsCAutoString fileExt;
      aIconURI->GetFileExtension(fileExt);
      if (!fileExt.IsEmpty()) {
        ms->GetTypeFromExtension(fileExt, type);
      }
    }
  }
  // Get the icon theme
  if (!gIconTheme) {
    gIconTheme = _gnome_icon_theme_new();

    if (!gIconTheme) {
      _gnome_vfs_file_info_clear(&fileInfo);
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  char* name = _gnome_icon_lookup(gIconTheme, NULL, spec.get(), NULL, &fileInfo,
                                  type.get(), GNOME_ICON_LOOKUP_FLAGS_NONE,
                                  NULL);

  _gnome_vfs_file_info_clear(&fileInfo);
  if (!name)
    return NS_ERROR_NOT_AVAILABLE;
  
  // Get the default theme associated with the screen
  // Do NOT free.
  GtkIconTheme *theme = gtk_icon_theme_get_default();
  if (!theme) {
    g_free(name);
    return NS_ERROR_UNEXPECTED;
  }

  GError *err = nsnull;
  GdkPixbuf* buf = gtk_icon_theme_load_icon(theme, name, iconSize, (GtkIconLookupFlags)0, &err);
  g_free(name);

  if (!buf) {
    if (err)
      g_error_free(err);
    return NS_ERROR_UNEXPECTED;
  }

  rv = ScaleIconBuf(&buf, iconSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = moz_gdk_pixbuf_to_channel(buf, aIconURI,
                                 getter_AddRefs(mRealChannel));
  g_object_unref(buf);
  return rv;
}
#endif // MOZ_ENABLE_GNOMEUI

#ifdef MOZ_ENABLE_GIO
nsresult
nsIconChannel::InitWithGIO(nsIMozIconURI *aIconURI)
{
  nsresult rv;

  GIcon *icon = NULL;
  nsCOMPtr<nsIURL> fileURI;

  // Read icon content
  aIconURI->GetIconURL(getter_AddRefs(fileURI));

  // Get icon for file specified by URI
  if (fileURI) {
    bool isFile;
    nsCAutoString spec;
    fileURI->GetAsciiSpec(spec);
    if (NS_SUCCEEDED(fileURI->SchemeIs("file", &isFile)) && isFile) {
      GFile *file = g_file_new_for_uri(spec.get());
      GFileInfo *fileInfo = g_file_query_info(file,
                                              G_FILE_ATTRIBUTE_STANDARD_ICON,
                                              G_FILE_QUERY_INFO_NONE, NULL, NULL);
      g_object_unref(file);
      if (fileInfo) {
        // icon from g_content_type_get_icon doesn't need unref
        icon = g_file_info_get_icon(fileInfo);
        if (icon)
          g_object_ref(icon);
        g_object_unref(fileInfo);
      }
    }
  }
  
  // Try to get icon by using MIME type
  if (!icon) {
    nsCAutoString type;
    aIconURI->GetContentType(type);
    // Try to get MIME type from file extension by using nsIMIMEService
    if (type.IsEmpty()) {
      nsCOMPtr<nsIMIMEService> ms(do_GetService("@mozilla.org/mime;1"));
      if (ms) {
        nsCAutoString fileExt;
        aIconURI->GetFileExtension(fileExt);
        ms->GetTypeFromExtension(fileExt, type);
      }
    }
    char *ctype = NULL; // character representation of content type
    if (!type.IsEmpty()) {
      ctype = g_content_type_from_mime_type(type.get());
    }
    if (ctype) {
      icon = g_content_type_get_icon(ctype);
      g_free(ctype);
    }
  }

  // Get default icon theme
  GtkIconTheme *iconTheme = gtk_icon_theme_get_default();  
  GtkIconInfo *iconInfo = NULL;
  // Get icon size
  PRInt32 iconSize = GetIconSize(aIconURI);

  if (icon) {
    NS_SUCCEEDED(rv);

    // Use icon and theme to get GtkIconInfo
    iconInfo = gtk_icon_theme_lookup_by_gicon(iconTheme,
                                              icon, iconSize,
                                              (GtkIconLookupFlags)0);
    g_object_unref(icon);
  }
  
  if (!iconInfo) {
    // Mozilla's mimetype lookup failed. Try the "unknown" icon.
    iconInfo = gtk_icon_theme_lookup_icon(iconTheme,
                                          "unknown", iconSize,
                                          (GtkIconLookupFlags)0);
    if (!iconInfo) {
      return NS_ERROR_NOT_AVAILABLE;
    }
  }
  
  // Create a GdkPixbuf buffer containing icon and scale it
  GdkPixbuf* buf = gtk_icon_info_load_icon(iconInfo, NULL);
  gtk_icon_info_free(iconInfo);
  if (!buf) {
    return NS_ERROR_UNEXPECTED;
  }
  
  rv = ScaleIconBuf(&buf, iconSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = moz_gdk_pixbuf_to_channel(buf, aIconURI,
                                 getter_AddRefs(mRealChannel));
  g_object_unref(buf);
  return rv;
}
#endif // MOZ_ENABLE_GIO

nsresult
nsIconChannel::Init(nsIURI* aURI)
{
  nsCOMPtr<nsIMozIconURI> iconURI = do_QueryInterface(aURI);
  NS_ASSERTION(iconURI, "URI is not an nsIMozIconURI");

  nsCAutoString stockIcon;
  iconURI->GetStockIcon(stockIcon);
  if (stockIcon.IsEmpty()) {
#ifdef MOZ_ENABLE_GNOMEUI
    return InitWithGnome(iconURI);
#else 
#ifdef MOZ_ENABLE_GIO
    return InitWithGIO(iconURI);
#else
    return NS_ERROR_NOT_AVAILABLE;
#endif
#endif
  }

  // Search for stockIcon
  nsCAutoString iconSizeString;
  iconURI->GetIconSize(iconSizeString);

  nsCAutoString iconStateString;
  iconURI->GetIconState(iconStateString);

  GtkIconSize icon_size = moz_gtk_icon_size(iconSizeString.get());
  GtkStateType state = iconStateString.EqualsLiteral("disabled") ?
    GTK_STATE_INSENSITIVE : GTK_STATE_NORMAL;

  // First lookup the icon by stock id and text direction.
  GtkTextDirection direction = GTK_TEXT_DIR_NONE;
  if (StringEndsWith(stockIcon, NS_LITERAL_CSTRING("-ltr"))) {
    direction = GTK_TEXT_DIR_LTR;
  } else if (StringEndsWith(stockIcon, NS_LITERAL_CSTRING("-rtl"))) {
    direction = GTK_TEXT_DIR_RTL;
  }

  bool forceDirection = direction != GTK_TEXT_DIR_NONE;
  nsCAutoString stockID;
  bool useIconName = false;
  if (!forceDirection) {
    direction = gtk_widget_get_default_direction();
    stockID = stockIcon;
  } else {
    // GTK versions < 2.22 use icon names from concatenating stock id with
    // -(rtl|ltr), which is how the moz-icon stock name is interpreted here.
    stockID = Substring(stockIcon, 0, stockIcon.Length() - 4);
    // However, if we lookup bidi icons by the stock name, then GTK versions
    // >= 2.22 will use a bidi lookup convention that most icon themes do not
    // yet follow.  Therefore, we first check to see if the theme supports the
    // old icon name as this will have bidi support (if found).
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    // Micking what gtk_icon_set_render_icon does with sizes, though it's not
    // critical as icons will be scaled to suit size.  It just means we follow
    // the same pathes and so share caches.
    gint width, height;
    if (gtk_icon_size_lookup(icon_size, &width, &height)) {
      gint size = NS_MIN(width, height);
      // We use gtk_icon_theme_lookup_icon() without
      // GTK_ICON_LOOKUP_USE_BUILTIN instead of gtk_icon_theme_has_icon() so
      // we don't pick up fallback icons added by distributions for backward
      // compatibility.
      GtkIconInfo *icon =
        gtk_icon_theme_lookup_icon(icon_theme, stockIcon.get(),
                                   size, (GtkIconLookupFlags)0);
      if (icon) {
        useIconName = PR_TRUE;
        gtk_icon_info_free(icon);
      }
    }
  }

  ensure_stock_image_widget();
  GtkStyle *style = gtk_widget_get_style(gStockImageWidget);
  GtkIconSet *icon_set = NULL;
  if (!useIconName) {
    icon_set = gtk_style_lookup_icon_set(style, stockID.get());
  }

  if (!icon_set) {
    // Either we have choosen icon-name lookup for a bidi icon, or stockIcon is
    // not a stock id so we assume it is an icon name.
    useIconName = PR_TRUE;
    // Creating a GtkIconSet is a convenient way to allow the style to
    // render the icon, possibly with variations suitable for insensitive
    // states.
    icon_set = gtk_icon_set_new();
    GtkIconSource *icon_source = gtk_icon_source_new();
    
    gtk_icon_source_set_icon_name(icon_source, stockIcon.get());
    gtk_icon_set_add_source(icon_set, icon_source);
    gtk_icon_source_free(icon_source);
  }

  GdkPixbuf *icon =
    gtk_icon_set_render_icon (icon_set, style, direction, state,
                              icon_size, gStockImageWidget, NULL);
  if (useIconName) {
    gtk_icon_set_unref(icon_set);
  }

  // According to documentation, gtk_icon_set_render_icon() never returns
  // NULL, but it does return NULL when we have the problem reported here:
  // https://bugzilla.gnome.org/show_bug.cgi?id=629878#c13
  if (!icon)
    return NS_ERROR_NOT_AVAILABLE;
  
  nsresult rv = moz_gdk_pixbuf_to_channel(icon, iconURI,
                                          getter_AddRefs(mRealChannel));

  g_object_unref(icon);

  return rv;
}

void
nsIconChannel::Shutdown() {
  if (gProtoWindow) {
    gtk_widget_destroy(gProtoWindow);
    gProtoWindow = nsnull;
    gStockImageWidget = nsnull;
  }
#ifdef MOZ_ENABLE_GNOMEUI
  if (gIconTheme) {
    g_object_unref(G_OBJECT(gIconTheme));
    gIconTheme = nsnull;
  }
  gTriedToLoadGnomeLibs = PR_FALSE;
  if (gLibGnomeUI) {
    PR_UnloadLibrary(gLibGnomeUI);
    gLibGnomeUI = nsnull;
  }
  if (gLibGnome) {
    PR_UnloadLibrary(gLibGnome);
    gLibGnome = nsnull;
  }
  if (gLibGnomeVFS) {
    PR_UnloadLibrary(gLibGnomeVFS);
    gLibGnomeVFS = nsnull;
  }
#endif //MOZ_ENABLE_GNOMEUI
}
