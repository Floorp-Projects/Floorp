/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_FileLocation_h
#define mozilla_FileLocation_h

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIFile.h"
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
  FileLocation();
  ~FileLocation();

  /**
   * Constructor for plain files
   */
  explicit FileLocation(nsIFile *file);

  /**
   * Constructors for path within an archive. The archive can be given either
   * as nsIFile or nsZipArchive.
   */
  FileLocation(nsIFile *zip, const char *path);

  FileLocation(nsZipArchive *zip, const char *path);

  /**
   * Creates a new file location relative to another one.
   */
  FileLocation(const FileLocation &file, const char *path = nullptr);

  /**
   * Initialization functions corresponding to constructors
   */
  void Init(nsIFile *file);

  void Init(nsIFile *zip, const char *path);

  void Init(nsZipArchive *zip, const char *path);

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
  already_AddRefed<nsIFile> GetBaseFile();

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
    nsresult GetSize(uint32_t *result);

    /**
     * Copies the data in the given buffer
     */
    nsresult Copy(char *buf, uint32_t len);
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
  nsCOMPtr<nsIFile> mBaseFile;
  nsRefPtr<nsZipArchive> mBaseZip;
  nsCString mPath;
}; /* class FileLocation */

} /* namespace mozilla */

#endif /* mozilla_FileLocation_h */
