/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.graphics.Bitmap;
import android.util.Log;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import org.mozilla.geckoview.GeckoResult;

/**
 * Represents an Web API image resource as used in web app manifests and media session metadata.
 *
 * @see <a href="https://www.w3.org/TR/image-resource">Image Resource</a>
 */
@AnyThread
public class ImageResource {
  private static final String LOGTAG = "ImageResource";
  private static final boolean DEBUG = false;

  /** Represents the size of an image resource option. */
  public static class Size {
    /** The width in pixels. */
    public final int width;

    /** The height in pixels. */
    public final int height;

    /**
     * Size contructor.
     *
     * @param width The width in pixels.
     * @param height The height in pixels.
     */
    public Size(final int width, final int height) {
      this.width = width;
      this.height = height;
    }
  }

  /** The URI of the image resource. */
  public final @NonNull String src;

  /** The MIME type of the image resource. */
  public final @Nullable String type;

  /** A {@link Size} array of supported images sizes. */
  public final @Nullable Size[] sizes;

  /**
   * ImageResource constructor.
   *
   * @param src The URI string of the image resource.
   * @param type The MIME type of the image resource.
   * @param sizes The supported images {@link Size} array.
   */
  public ImageResource(
      final @NonNull String src, final @Nullable String type, final @Nullable Size[] sizes) {
    this.src = src;
    this.type = type != null ? type.toLowerCase(Locale.ROOT) : null;
    this.sizes = sizes;
  }

  /**
   * ImageResource constructor.
   *
   * @param src The URI string of the image resource.
   * @param type The MIME type of the image resource.
   * @param sizes The supported images sizes string.
   * @see <a href="https://html.spec.whatwg.org/multipage/semantics.html#dom-link-sizes">Attribute
   *     spec for sizes</a>
   */
  public ImageResource(
      final @NonNull String src, final @Nullable String type, final @Nullable String sizes) {
    this(src, type, parseSizes(sizes));
  }

  private static @Nullable Size[] parseSizes(final @Nullable String sizesStr) {
    if (sizesStr == null || sizesStr.isEmpty()) {
      return null;
    }

    final String[] sizesStrs = sizesStr.toLowerCase(Locale.ROOT).split(" ");
    final List<Size> sizes = new ArrayList<Size>();

    for (final String sizeStr : sizesStrs) {
      if (sizesStr.equals("any")) {
        // 0-width size will always be favored.
        sizes.add(new Size(0, 0));
        continue;
      }
      final String[] widthHeight = sizeStr.split("x");
      if (widthHeight.length != 2) {
        // Not spec-compliant size.
        continue;
      }
      try {
        sizes.add(new Size(Integer.valueOf(widthHeight[0]), Integer.valueOf(widthHeight[1])));
      } catch (final NumberFormatException e) {
        Log.e(LOGTAG, "Invalid image resource size", e);
      }
    }
    if (sizes.isEmpty()) {
      return null;
    }
    return sizes.toArray(new Size[0]);
  }

  public static @NonNull ImageResource fromBundle(final GeckoBundle bundle) {
    return new ImageResource(
        bundle.getString("src"), bundle.getString("type"), bundle.getString("sizes"));
  }

  @Override
  public String toString() {
    final StringBuilder builder = new StringBuilder("ImageResource {");
    builder
        .append("src=")
        .append(src)
        .append("type=")
        .append(type)
        .append("sizes=")
        .append(sizes)
        .append("}");
    return builder.toString();
  }

  /**
   * Get the best version of this image for size <code>size</code>. Embedders are encouraged to
   * cache the result of this method keyed with this instance.
   *
   * @param size pixel size at which this image will be displayed at.
   * @return A {@link GeckoResult} that resolves to the bitmap when ready.
   */
  @NonNull
  public GeckoResult<Bitmap> getBitmap(final int size) {
    return ImageDecoder.instance().decode(src, size);
  }

  /**
   * Represents a collection of {@link ImageResource} options. Image resources are often used in a
   * collection to provide multiple image options for various sizes. This data structure can be used
   * to retrieve the best image resource for any given target image size.
   */
  public static class Collection {
    private static class SizeIndexPair {
      public final int width;
      public final int idx;

      public SizeIndexPair(final int width, final int idx) {
        this.width = width;
        this.idx = idx;
      }
    }

    // The individual image resources, usually each with a unique src.
    private final List<ImageResource> mImages;

    // A sorted size-index list. The list is sorted based on the supported
    // sizes of the images in ascending order.
    private final List<SizeIndexPair> mSizeIndex;

    /* package */ Collection() {
      mImages = new ArrayList<>();
      mSizeIndex = new ArrayList<>();
    }

    /** Builder class for the construction of a {@link Collection}. */
    public static class Builder {
      final Collection mCollection;

      public Builder() {
        mCollection = new Collection();
      }

      /**
       * Add an image resource to the collection.
       *
       * @param image The {@link ImageResource} to be added.
       * @return This builder instance.
       */
      public @NonNull Builder add(final ImageResource image) {
        final int index = mCollection.mImages.size();

        if (image.sizes == null) {
          // Null-sizes are handled the same as `any`.
          mCollection.mSizeIndex.add(new SizeIndexPair(0, index));
        } else {
          for (final Size size : image.sizes) {
            mCollection.mSizeIndex.add(new SizeIndexPair(size.width, index));
          }
        }
        mCollection.mImages.add(image);
        return this;
      }

      /**
       * Finalize the collection.
       *
       * @return The final collection.
       */
      public @NonNull Collection build() {
        Collections.sort(mCollection.mSizeIndex, (a, b) -> Integer.compare(a.width, b.width));
        return mCollection;
      }
    }

    @Override
    public String toString() {
      final StringBuilder builder = new StringBuilder("ImageResource.Collection {");
      builder.append("images=[");

      for (final ImageResource image : mImages) {
        builder.append(image).append(", ");
      }
      builder.append("]}");
      return builder.toString();
    }

    /**
     * Returns the best suited {@link ImageResource} for the given size. This is usually determined
     * based on the minimal difference between the given size and one of the supported widths of an
     * image resource.
     *
     * @param size The target size for the image in pixels.
     * @return The best {@link ImageResource} for the given size from this collection.
     */
    public @Nullable ImageResource getBest(final int size) {
      if (mSizeIndex.isEmpty()) {
        return null;
      }
      int bestMatchIdx = mSizeIndex.get(0).idx;
      int lastDiff = size;
      for (final SizeIndexPair sizeIndex : mSizeIndex) {
        final int diff = Math.abs(sizeIndex.width - size);
        if (lastDiff <= diff) {
          // With increasing widths, the difference can only grow now.
          // 0-width means "any", so we're finished at the first
          // entry.
          break;
        }
        lastDiff = diff;
        bestMatchIdx = sizeIndex.idx;
      }
      return mImages.get(bestMatchIdx);
    }

    /**
     * Get the best version of this image for size <code>size</code>. Embedders are encouraged to
     * cache the result of this method keyed with this instance.
     *
     * @param size pixel size at which this image will be displayed at.
     * @return A {@link GeckoResult} that resolves to the bitmap when ready.
     */
    @NonNull
    public GeckoResult<Bitmap> getBitmap(final int size) {
      final ImageResource image = getBest(size);
      if (image == null) {
        return GeckoResult.fromValue(null);
      }
      return image.getBitmap(size);
    }

    public static Collection fromSizeSrcBundle(final GeckoBundle bundle) {
      final Builder builder = new Builder();

      for (final String key : bundle.keys()) {
        final Integer intKey = Integer.valueOf(key);
        if (intKey == null) {
          Log.e(LOGTAG, "Non-integer image key: " + intKey);

          if (DEBUG) {
            throw new RuntimeException("Non-integer image key: " + key);
          }
          continue;
        }

        final String src = getImageValue(bundle.get(key));
        if (src != null) {
          // Given the bundle structure, we don't have insight on
          // individual image resources so we have to create an
          // instance for each size entry.
          final ImageResource image =
              new ImageResource(src, null, new Size[] {new Size(intKey, intKey)});
          builder.add(image);
        }
      }
      return builder.build();
    }

    private static String getImageValue(final Object value) {
      // The image value can either be an object containing images for
      // each theme...
      if (value instanceof GeckoBundle) {
        // We don't support theme_images yet, so let's just return the
        // default value.
        final GeckoBundle themeImages = (GeckoBundle) value;
        final Object defaultImages = themeImages.get("default");

        if (!(defaultImages instanceof String)) {
          if (DEBUG) {
            throw new RuntimeException("Unexpected themed_icon value.");
          }
          Log.e(LOGTAG, "Unexpected themed_icon value.");
          return null;
        }

        return (String) defaultImages;
      }

      // ... or just a URL.
      if (value instanceof String) {
        return (String) value;
      }

      // We never expect it to be something else, so let's error out here.
      if (DEBUG) {
        throw new RuntimeException("Unexpected image value: " + value);
      }

      Log.e(LOGTAG, "Unexpected image value.");
      return null;
    }
  }
}
