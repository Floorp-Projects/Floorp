# Getting Started with PDFium

[TOC]

This guide walks through some examples of using the PDFium library. For an
example of using PDFium see the [Chromium PDF Plugin][chrome-plugin].

## Prerequisites

You will need the PDFium library on your computer. You can see the
[README](/README.md) for instructions on getting and installing PDFium.

*** note
You must compile PDFium without both V8 and XFA support for the examples
here to work. V8 can be compiled out by providing
`GYP_DEFINES="pdf_enable_v8=0 pdf_enable_xfa=0" build/gyp_pdfium`.

See the [V8 Getting Started][pdfium-v8] guide for how to
initialize PDFium when V8 is compiled into the binary.
***

## PDFium Headers

PDFium's API has been broken up over several headers. You only need to include
the headers for functionality you use in your application. The full set of
headers can be found in the [public/ folder of the repository][pdfium-public].

In all cases you'll need to include `fpdfview.h` as it defines the needed
methods for initialization and destruction of the library.

## Initializing PDFium

The first step to using PDFium is to initialize the library. Having done so,
you'll need to destroy the library when you're finished. When initializing the
library you provide the `FPDF_LIBRARY_CONFIG` parameters to
`FPDF_InitLibraryWithConfig`.

```c
#include <fpdfview.h>

int main() {
  FPDF_LIBRARY_CONFIG config;
  config.version = 2;
  config.m_pUserFontPaths = NULL;
  config.m_pIsolate = NULL;
  config.m_v8EmbedderSlot = 0;

  FPDF_InitLibraryWithConfig(&config);

  FPDF_DestroyLibrary();
  return 0;
}
```

Currently the `config.version` must be set to `2`. `m_pUserFontPaths` can be
used to override the font paths searched by PDFium. If you wish to use your
own font paths pass a `NULL` terminated list of `const char*` paths to use.

`m_pIsolate` and `m_v8EmbedderSlot` are both used to configure the V8
javascript engine. In the first case, you can provide an isolate through
`m_pIsolate` for PDFium to use to store per-isolate data. Passing `NULL` will
case PDFium to allocate a new isolate. `m_v8EmbedderSlot` is the embedder data
slot to use in the v8::Isolate to store PDFium data. The value must be between
0 and v8::Internals::kNumIsolateDataSlots. Typically, 0 is a good choice.

For more information on using Javascript see the [V8 Getting Started][pdfium-v8]
guide.

*** aside
PDFium is built as a set of static libraries. You'll need to specify them all on
the link line in order to compile. My build line was:

```
PDF_LIBS="-lpdfium -lfpdfapi -lfxge -lfpdfdoc -lfxcrt -lfx_agg \
-lfxcodec -lfx_lpng -lfx_libopenjpeg -lfx_lcms2 -lfx_freetype -ljpeg \
-lfx_zlib -lfdrm -lpdfwindow -lbigint -lformfiller -ljavascript \
-lfxedit"
PDF_DIR=<path/to/pdfium>

clang -I $PDF_DIR/public -o init init.c -L $PDF_DIR/out/Debug -lstdc++ -framework AppKit $PDF_LIBS
```

The `-framework AppKit` as needed as I'm building on a Mac. Internally PDFium
uses C++, which is why `-lstdc++` is required on the link line.
***

## Loading a Document

One of the main objects in PDFium is the `FPDF_DOCUMENT`. The object will allow
access to information from PDFs. There are four ways to to create a
`FPDF_DOCUMENT`. `FPDF_CreateNewDocument` will create an empty object which
can be used to create PDFs. For more information see the
[PDF Editing Guide][pdfium-edit-guide].

Loading an existing document is done in one of three ways: loading from file,
loading from memory, or loading via a custom loader. In all three cases you'll
provide a `FPDF_BYTESTRING` which is the password needed to unlock the PDF, if
encrypted. If the file is not encrypted the password can be `NULL`.

The two simplest methods are loading from file and loading from memory. To load
from file, you'll provide the name of the file to open, including extension. For
loading from memory you'll provide a data buffer containing the PDF and its
length.

```c
FPDF_STRING test_doc = "test_doc.pdf";
FPDF_DOCUMENT doc = FPDF_LoadDocument(test_doc, NULL);
if (!doc) {
  return 1;
}

FPDF_CloseDocument(doc);

```

In all three cases, `FPDF_LoadDocument`, `FPDF_LoadMemDocument`,
`FPDF_LoadCustomDocument` a return of `NULL` indicates an error opening the
document or that the file was not found.

You can use `FPDF_GetLastError` to determine what went wrong.

```c
#include <fpdfview.h>
#include <unistd.h>
#include <stdio.h>

int main() {
  FPDF_LIBRARY_CONFIG config;
  config.version = 2;
  config.m_pUserFontPaths = NULL;
  config.m_pIsolate = NULL;
  config.m_v8EmbedderSlot = 0;

  FPDF_InitLibraryWithConfig(&config);

  FPDF_DOCUMENT doc = FPDF_LoadDocument(test_doc, NULL);
  if (!doc) {
    unsigned long err = FPDF_GetLastError();
    fprintf(stderr, "Load pdf docs unsuccessful: ");
    switch (err) {
      case FPDF_ERR_SUCCESS:
        fprintf(stderr, "Success");
        break;
      case FPDF_ERR_UNKNOWN:
        fprintf(stderr, "Unknown error");
        break;
      case FPDF_ERR_FILE:
        fprintf(stderr, "File not found or could not be opened");
        break;
      case FPDF_ERR_FORMAT:
        fprintf(stderr, "File not in PDF format or corrupted");
        break;
      case FPDF_ERR_PASSWORD:
        fprintf(stderr, "Password required or incorrect password");
        break;
      case FPDF_ERR_SECURITY:
        fprintf(stderr, "Unsupported security scheme");
        break;
      case FPDF_ERR_PAGE:
        fprintf(stderr, "Page not found or content error");
        break;
      default:
        fprintf(stderr, "Unknown error %ld", err);
    }
    fprintf(stderr, ".\n");
    goto EXIT;
  }

  FPDF_CloseDocument(doc);
EXIT:
  FPDF_DestroyLibrary();
  return 0;
```

While the above are simple, the preferable technique is to use a custom loader.
This makes it possible to load pieces of the document only as needed. This is
useful for loading documents over the network.




[chrome-plugin]: https://chromium.googlesource.com/chromium/src/+/master/pdf/
[pdfium-public]: https://pdfium.googlesource.com/pdfium/+/master/public/
[pdfium-v8]: /docs/v8-getting-started.md
[pdfium-edit-guide]: /docs/pdfium-edit-guide.md
