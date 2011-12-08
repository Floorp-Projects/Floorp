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
 * The Original Code is Mozilla code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
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

#ifndef mozilla_FileLocation_h
#define mozilla_FileLocation_h

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsILocalFile.h"
#include "nsIURI.h"
#include "FileUtils.h"

class nsZipArchive;
class nsZipItem;

namespace mozilla {

class FileLocation
{
public:
  /**
   * FileLocation is an helper to handle different kind of file locations
   * within Gecko:
   * - on filesystems
   * - in archives
   * - in archives within archives
   * As such, it stores a path within an archive, as well as the archive
   * path itself, or the complete file path alone when on a filesystem.
   * When the archive is in an archive, an nsZipArchive is stored instead
   * of a file path.
   */
  FileLocation() { }

  /**
   * Constructor for plain files
   */
  FileLocation(nsILocalFile *file)
  {
    Init(file);
  }

  /**
   * Constructors for path within an archive. The archive can be given either
   * as nsILocalFile or nsZipArchive.
   */
  FileLocation(nsILocalFile *zip, const char *path)
  {
    Init(zip, path);
  }

  FileLocation(nsZipArchive *zip, const char *path)
  {
    Init(zip, path);
  }

  /**
   * Creates a new file location relative to another one.
   */
  FileLocation(const FileLocation &file, const char *path = NULL);

  /**
   * Initialization functions corresponding to constructors
   */
  void Init(nsILocalFile *file)
  {
    mBaseZip = NULL;
    mBaseFile = file;
    mPath.Truncate();
  }

  void Init(nsILocalFile *zip, const char *path)
  {
    mBaseZip = NULL;
    mBaseFile = zip;
    mPath = path;
  }

  void Init(nsZipArchive *zip, const char *path)
  {
    mBaseZip = zip;
    mBaseFile = NULL;
    mPath = path;
  }

  /**
   * Returns an URI string corresponding to the file location
   */
  void GetURIString(nsACString &result) const;

  /**
   * Returns the base file of the location, where base file is defined as:
   * - The file itself when the location is on a filesystem
   * - The archive file when the location is in an archive
   * - The outer archive file when the location is in an archive in an archive
   */
  already_AddRefed<nsILocalFile> GetBaseFile();

  /**
   * Returns whether the "base file" (see GetBaseFile) is an archive
   */
  bool IsZip() const
  {
    return !mPath.IsEmpty();
  }

  /**
   * Returns the path within the archive, when within an archive
   */
  void GetPath(nsACString &result) const
  {
    result = mPath;
  }

  /**
   * Boolean value corresponding to whether the file location is initialized
   * or not.
   */
  operator bool() const
  {
    return mBaseFile || mBaseZip;
  }

  /**
   * Returns whether another FileLocation points to the same resource
   */
  bool Equals(const FileLocation &file) const;

  /**
   * Data associated with a FileLocation.
   */
  class Data
  {
  public:
    /**
     * Returns the data size
     */
    nsresult GetSize(PRUint32 *result);

    /**
     * Copies the data in the given buffer
     */
    nsresult Copy(char *buf, PRUint32 len);
  protected:
    friend class FileLocation;
    nsZipItem *mItem;
    nsRefPtr<nsZipArchive> mZip;
    mozilla::AutoFDClose mFd;
  };

  /**
   * Returns the data associated with the resource pointed at by the file
   * location.
   */
  nsresult GetData(Data &data);
private:
  nsCOMPtr<nsILocalFile> mBaseFile;
  nsRefPtr<nsZipArchive> mBaseZip;
  nsCString mPath;
}; /* class FileLocation */

} /* namespace mozilla */

#endif /* mozilla_FileLocation_h */
