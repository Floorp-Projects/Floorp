/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test;

import android.util.Base64;
import androidx.annotation.AnyThread;
import androidx.annotation.Nullable;
import java.math.BigInteger;
import java.nio.ByteBuffer;
import java.security.InvalidAlgorithmParameterException;
import java.security.KeyFactory;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.interfaces.ECPublicKey;
import java.security.spec.ECGenParameterSpec;
import java.security.spec.ECParameterSpec;
import java.security.spec.ECPoint;
import java.security.spec.ECPublicKeySpec;
import java.security.spec.InvalidKeySpecException;

/**
 * Utilities for converting {@link ECPublicKey} to/from X9.62 encoding.
 *
 * @see <a href="https://tools.ietf.org/html/rfc8291">Message Encryption for Web Push</a>
 */
/* package */ class WebPushUtils {
  public static final int P256_PUBLIC_KEY_LENGTH = 65; // 1 + 32 + 32
  private static final byte NIST_HEADER = 0x04; // uncompressed format

  private static ECParameterSpec sSpec;

  private WebPushUtils() {}

  /**
   * Encodes an {@link ECPublicKey} into X9.62 format as required by Web Push.
   *
   * @param key the {@link ECPublicKey} to encode
   * @return the encoded {@link ECPublicKey}
   */
  @AnyThread
  public static @Nullable byte[] keyToBytes(final @Nullable ECPublicKey key) {
    if (key == null) {
      return null;
    }

    final ByteBuffer buffer = ByteBuffer.allocate(P256_PUBLIC_KEY_LENGTH);
    buffer.put(NIST_HEADER);

    putUnsignedBigInteger(buffer, key.getW().getAffineX());
    putUnsignedBigInteger(buffer, key.getW().getAffineY());

    if (buffer.position() != P256_PUBLIC_KEY_LENGTH) {
      throw new RuntimeException("Unexpected key length " + buffer.position());
    }

    return buffer.array();
  }

  private static void putUnsignedBigInteger(final ByteBuffer buffer, final BigInteger value) {
    final byte[] bytes = value.toByteArray();
    if (bytes.length < 32) {
      buffer.put(new byte[32 - bytes.length]);
      buffer.put(bytes);
    } else {
      buffer.put(bytes, bytes.length - 32, 32);
    }
  }

  /**
   * Encodes an {@link ECPublicKey} into X9.62 format as required by Web Push, further encoded into
   * Base64.
   *
   * @param key the {@link ECPublicKey} to encode
   * @return the encoded {@link ECPublicKey}
   */
  @AnyThread
  public static @Nullable String keyToString(final @Nullable ECPublicKey key) {
    return Base64.encodeToString(
        keyToBytes(key), Base64.URL_SAFE | Base64.NO_WRAP | Base64.NO_PADDING);
  }

  /**
   * @return A {@link ECParameterSpec} for P-256 (secp256r1).
   */
  public static ECParameterSpec getP256Spec() {
    if (sSpec == null) {
      try {
        final KeyPairGenerator gen = KeyPairGenerator.getInstance("EC");
        final ECGenParameterSpec genSpec = new ECGenParameterSpec("secp256r1");
        gen.initialize(genSpec);
        sSpec = ((ECPublicKey) gen.generateKeyPair().getPublic()).getParams();
      } catch (final NoSuchAlgorithmException e) {
        throw new RuntimeException(e);
      } catch (final InvalidAlgorithmParameterException e) {
        throw new RuntimeException(e);
      }
    }

    return sSpec;
  }

  /**
   * Converts a Base64 X9.62 encoded Web Push key into a {@link ECPublicKey}.
   *
   * @param base64Bytes the X9.62 data as Base64
   * @return a {@link ECPublicKey}
   */
  @AnyThread
  public static @Nullable ECPublicKey keyFromString(final @Nullable String base64Bytes) {
    if (base64Bytes == null) {
      return null;
    }

    return keyFromBytes(Base64.decode(base64Bytes, Base64.URL_SAFE));
  }

  private static BigInteger readUnsignedBigInteger(
      final byte[] bytes, final int offset, final int length) {
    byte[] mag = bytes;
    if (offset != 0 || length != bytes.length) {
      mag = new byte[length];
      System.arraycopy(bytes, offset, mag, 0, length);
    }
    return new BigInteger(1, mag);
  }

  /**
   * Converts a X9.62 encoded Web Push key into a {@link ECPublicKey}.
   *
   * @param bytes the X9.62 data
   * @return a {@link ECPublicKey}
   */
  @AnyThread
  public static @Nullable ECPublicKey keyFromBytes(final @Nullable byte[] bytes) {
    if (bytes == null) {
      return null;
    }

    if (bytes.length != P256_PUBLIC_KEY_LENGTH) {
      throw new IllegalArgumentException(
          String.format("Expected exactly %d bytes", P256_PUBLIC_KEY_LENGTH));
    }

    if (bytes[0] != NIST_HEADER) {
      throw new IllegalArgumentException("Expected uncompressed NIST format");
    }

    try {
      final BigInteger x = readUnsignedBigInteger(bytes, 1, 32);
      final BigInteger y = readUnsignedBigInteger(bytes, 33, 32);

      final ECPoint point = new ECPoint(x, y);
      final ECPublicKeySpec spec = new ECPublicKeySpec(point, getP256Spec());
      final KeyFactory factory = KeyFactory.getInstance("EC");

      return (ECPublicKey) factory.generatePublic(spec);
    } catch (final NoSuchAlgorithmException e) {
      throw new RuntimeException(e);
    } catch (final InvalidKeySpecException e) {
      throw new RuntimeException(e);
    }
  }
}
