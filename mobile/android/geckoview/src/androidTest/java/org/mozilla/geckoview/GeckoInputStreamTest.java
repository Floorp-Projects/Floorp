package org.mozilla.geckoview;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.geckoview.test.BaseSessionTest;

@RunWith(AndroidJUnit4.class)
@MediumTest
public class GeckoInputStreamTest extends BaseSessionTest {

  @Test
  public void readAndWriteFile() throws IOException, ExecutionException, InterruptedException {
    final byte[] originalBytes = getTestBytes(TEST_GIF_PATH);
    final File createdFile = File.createTempFile("temp", ".gif");
    final GeckoInputStream geckoInputStream = new GeckoInputStream(null);

    // Reads from the GeckoInputStream and rewrites to a new file
    final Thread readAndRewrite =
        new Thread() {
          public void run() {
            try (OutputStream output = new FileOutputStream(createdFile)) {
              byte[] buffer = new byte[4 * 1024];
              int read;
              while ((read = geckoInputStream.read(buffer)) != -1) {
                output.write(buffer, 0, read);
              }
              output.flush();
              geckoInputStream.close();
            } catch (IOException e) {
              throw new RuntimeException(e.getMessage());
            }
          }
        };

    // Writes the bytes from the original file to the GeckoInputStream
    final Thread write =
        new Thread() {
          public void run() {
            try {
              geckoInputStream.appendBuffer(originalBytes);
            } catch (IOException e) {
              throw new RuntimeException(e.getMessage());
            }
            geckoInputStream.sendEof();
          }
        };

    final CompletableFuture<Void> testReadWrite =
        CompletableFuture.allOf(
            CompletableFuture.runAsync(readAndRewrite), CompletableFuture.runAsync(write));
    testReadWrite.get();

    final byte[] fileContent = new byte[(int) createdFile.length()];
    final FileInputStream fis = new FileInputStream(createdFile);
    fis.read(fileContent);
    fis.close();

    Assert.assertTrue("File was recreated correctly.", Arrays.equals(originalBytes, fileContent));
  }

  class Writer implements Runnable {
    final char threadName;
    final int timesToRun;
    final GeckoInputStream stream;

    public Writer(char threadName, int timesToRun, GeckoInputStream stream) {
      this.threadName = threadName;
      this.timesToRun = timesToRun;
      this.stream = stream;
    }

    public void run() {
      for (int i = 0; i <= timesToRun; i++) {
        final byte[] data = String.format("%s %d %n", threadName, i).getBytes();
        try {
          stream.appendBuffer(data);
        } catch (IOException e) {
          throw new RuntimeException(e.getMessage());
        }
      }
    }
  }

  private boolean isSequenceInOrder(
      List<String> lines, List<Character> threadNames, int dataLength) {
    HashMap<Character, Integer> lastValue = new HashMap<>();
    for (Character thread : threadNames) {
      lastValue.put(thread, -1);
    }
    for (String line : lines) {
      final char thread = line.charAt(0);
      final int number = Integer.parseInt(line.replaceAll("[\\D]", ""));

      // Number should always be in sequence for a given thread
      if (lastValue.get(thread) + 1 == number) {
        lastValue.replace(thread, number);
      } else {
        return false;
      }
    }
    for (Character thread : threadNames) {
      if (lastValue.get(thread) != dataLength) {
        return false;
      }
    }
    return true;
  }

  @Test
  public void multipleWriters() throws ExecutionException, InterruptedException, IOException {
    final GeckoInputStream geckoInputStream = new GeckoInputStream(null);
    final List<Character> threadNames = Arrays.asList('A', 'B');
    final int writeCount = 1000;
    final CompletableFuture<Void> writers =
        CompletableFuture.allOf(
            CompletableFuture.runAsync(
                new Writer(threadNames.get(0), writeCount, geckoInputStream)),
            CompletableFuture.runAsync(
                new Writer(threadNames.get(1), writeCount, geckoInputStream)));
    writers.get();
    geckoInputStream.sendEof();

    final List<String> lines = new ArrayList<>();
    final BufferedReader reader = new BufferedReader(new InputStreamReader(geckoInputStream));
    while (reader.ready()) {
      lines.add(reader.readLine());
    }
    reader.close();

    Assert.assertTrue(
        "Writers wrote as expected.", isSequenceInOrder(lines, threadNames, writeCount));
  }

  @Test
  public void writeError() throws IOException {
    boolean didThrowIoException = false;
    final GeckoInputStream inputStream = new GeckoInputStream(null);
    final BufferedReader reader = new BufferedReader(new InputStreamReader(inputStream));
    final byte[] data = "Hello, World.".getBytes();
    inputStream.appendBuffer(data);
    inputStream.writeError();
    inputStream.sendEof();
    try {
      reader.readLine();
    } catch (IOException e) {
      didThrowIoException = true;
    }
    reader.close();
    Assert.assertTrue("Correctly caused an IOException from writer.", didThrowIoException);
  }
}
