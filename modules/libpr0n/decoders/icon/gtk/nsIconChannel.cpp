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

#include <libgnome/libgnome.h>
#include <libgnomeui/gnome-icon-theme.h>
#include <libgnomeui/gnome-icon-lookup.h>
#include <libgnomeui/gnome-ui-init.h>

#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-ops.h>

#include "nsIMIMEService.h"

#include "nsIStringBundle.h"

#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIURL.h"

#include "nsIconChannel.h"

NS_IMPL_ISUPPORTS2(nsIconChannel,
                   nsIRequest,
                   nsIChannel)

/**
 * Given a path to a PNG Image, creates a channel from it.
 * Note that the channel will delete the file when it's done with it.
 *
 * (When this function fails, the file will NOT be deleted)
 */
static nsresult
pngfile_to_channel(const char* aFilename, nsIChannel** aChannel) {
  // Now we have to create an uri for the file...
  nsCOMPtr<nsILocalFile> lf;
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(aFilename), PR_FALSE,
                                      getter_AddRefs(lf));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIInputStream> is;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(is), lf, -1, -1,
                                  nsIFileInputStream::DELETE_ON_CLOSE);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURI> realURI;
  rv = NS_NewFileURI(getter_AddRefs(realURI), lf);
  if (NS_FAILED(rv))
    return rv;

  rv = NS_NewInputStreamChannel(aChannel, realURI, is,
                                NS_LITERAL_CSTRING("image/png"));
  return rv;
}

nsresult
nsIconChannel::Init(nsIURI* aURI) {
  mURI = do_QueryInterface(aURI);
  NS_ASSERTION(mURI, "URI passed to nsIconChannel is no nsIMozIconURI!");

  if (!gnome_program_get()) {
    // Get the brandShortName from the string bundle to pass to GNOME
    // as the application name.  This may be used for things such as
    // the title of grouped windows in the panel.
    nsCOMPtr<nsIStringBundleService> bundleService = 
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);

    NS_ASSERTION(bundleService, "String bundle service must be present!");

    nsCOMPtr<nsIStringBundle> bundle;
    bundleService->CreateBundle("chrome://global/locale/brand.properties",
                                getter_AddRefs(bundle));
    nsXPIDLString appName;

    if (bundle) {
      bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
                                getter_Copies(appName));
    } else {
      NS_WARNING("brand.properties not present, using default application name");
      appName.Assign(NS_LITERAL_STRING("Gecko"));
    }

    char* empty[] = { "" };
    gnome_init(NS_ConvertUTF16toUTF8(appName).get(), "1.0", 1, empty);
  }

  PRUint32 iconSize;
  nsresult rv = mURI->GetImageSize(&iconSize);
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetImageSize failed");

  nsCAutoString type;
  mURI->GetContentType(type);

  GnomeVFSFileInfo fileInfo = {0};
  fileInfo.refcount = 1; // In case some GnomeVFS function addrefs and releases it

  nsCAutoString spec;
  nsCOMPtr<nsIURI> fileURI;
  rv = mURI->GetIconFile(getter_AddRefs(fileURI));
  if (fileURI) {
    fileURI->GetAsciiSpec(spec);
    // Only ask gnome-vfs for a GnomeVFSFileInfo for file: uris, to avoid a
    // network request
    PRBool isFile;
    if (NS_SUCCEEDED(fileURI->SchemeIs("file", &isFile)) && isFile) {
      gnome_vfs_get_file_info(spec.get(), &fileInfo, GNOME_VFS_FILE_INFO_DEFAULT);
    }
    else {
      // We have to get a leaf name from our uri...
      nsCOMPtr<nsIURL> url(do_QueryInterface(fileURI));
      if (url) {
        nsCAutoString name;
        // The filename we get is UTF-8-compatible, which matches gnome expectations.
        // See also: http://lists.gnome.org/archives/gnome-vfs-list/2004-March/msg00049.html
        // "Whenever we can detect the charset used for the URI type we try to
        //  convert it to/from utf8 automatically inside gnome-vfs."
        // I'll interpret that as "otherwise, this field is random junk".
        url->GetFileName(name);
        fileInfo.name = g_strdup(name.get());
      }
      // If this is no nsIURL, nothing we can do really.

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
      mURI->GetFileExtension(fileExt);
      ms->GetTypeFromExtension(fileExt, type);
    }
  }

  // Get the icon theme
  GnomeIconTheme *t = gnome_icon_theme_new();
  if (!t) {
    gnome_vfs_file_info_clear(&fileInfo);
    return NS_ERROR_NOT_AVAILABLE;
  }
 

  char* name = gnome_icon_lookup(t, NULL, spec.get(), NULL, &fileInfo, type.get(), GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
  gnome_vfs_file_info_clear(&fileInfo);
  if (!name) {
    g_object_unref(G_OBJECT(t));
    return NS_ERROR_NOT_AVAILABLE;
  }
 
  char* file = gnome_icon_theme_lookup_icon(t, name, iconSize, NULL, NULL);
  g_free(name);
  g_object_unref(G_OBJECT(t));
  if (!file)
    return NS_ERROR_NOT_AVAILABLE;

  // Create a GdkPixbuf buffer and scale it
  GError *err = nsnull;
  GdkPixbuf* buf = gdk_pixbuf_new_from_file(file, &err);
  g_free(file);
  if (!buf) {
    if (err)
      g_error_free(err);
    return NS_ERROR_UNEXPECTED;
  }

  GdkPixbuf* scaled = buf;
  if (gdk_pixbuf_get_width(buf)  != iconSize &&
      gdk_pixbuf_get_height(buf) != iconSize) {
    // scale...
    scaled = gdk_pixbuf_scale_simple(buf, iconSize, iconSize,
                                     GDK_INTERP_BILINEAR);
    gdk_pixbuf_unref(buf);
    if (!scaled)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  char tmpfile[] = "/tmp/moziconXXXXXX";
  int fd = mkstemp(tmpfile);
  if (fd == -1) {
    gdk_pixbuf_unref(scaled);
    return NS_ERROR_UNEXPECTED;
  }

  gboolean ok = gdk_pixbuf_save(scaled, tmpfile, "png", &err, NULL);
  gdk_pixbuf_unref(scaled);
  if (!ok) {
    close(fd);
    remove(tmpfile);
    if (err)
      g_error_free(err);
    return NS_ERROR_UNEXPECTED;
  }

  rv = pngfile_to_channel(tmpfile, getter_AddRefs(mRealChannel));
  close(fd);
  if (NS_FAILED(rv))
    remove(tmpfile);
  return rv;
}

