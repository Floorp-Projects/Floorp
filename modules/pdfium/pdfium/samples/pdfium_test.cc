// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined PDF_ENABLE_SKIA && !defined _SKIA_SUPPORT_
#define _SKIA_SUPPORT_
#endif

#include "core/fdrm/crypto/fx_crypt.h"
#include "public/fpdf_dataavail.h"
#include "public/fpdf_edit.h"
#include "public/fpdf_ext.h"
#include "public/fpdf_formfill.h"
#include "public/fpdf_text.h"
#include "public/fpdfview.h"
#include "samples/image_diff_png.h"
#include "testing/test_support.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifdef PDF_ENABLE_V8
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"
#endif  // PDF_ENABLE_V8

#ifdef PDF_ENABLE_SKIA
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#endif

#ifdef _WIN32
#define access _access
#define snprintf _snprintf
#define R_OK 4
#endif

enum OutputFormat {
  OUTPUT_NONE,
  OUTPUT_TEXT,
  OUTPUT_PPM,
  OUTPUT_PNG,
#ifdef _WIN32
  OUTPUT_BMP,
  OUTPUT_EMF,
#endif
#ifdef PDF_ENABLE_SKIA
  OUTPUT_SKP,
#endif
};

struct Options {
  Options()
      : show_config(false),
        send_events(false),
        pages(false),
        md5(false),
        output_format(OUTPUT_NONE) {}

  bool show_config;
  bool send_events;
  bool pages;
  bool md5;
  OutputFormat output_format;
  std::string scale_factor_as_string;
  std::string exe_path;
  std::string bin_directory;
  std::string font_directory;
  // 0-based page numbers to be rendered.
  int first_page;
  int last_page;
};

struct FPDF_FORMFILLINFO_PDFiumTest : public FPDF_FORMFILLINFO {
  // Hold a map of the currently loaded pages in order to avoid them
  // to get loaded twice.
  std::map<int, FPDF_PAGE> loaded_pages;

  // Hold a pointer of FPDF_FORMHANDLE so that PDFium app hooks can
  // make use of it.
  FPDF_FORMHANDLE form_handle;
};

struct AvailDeleter {
  inline void operator()(FPDF_AVAIL avail) const { FPDFAvail_Destroy(avail); }
};

static FPDF_FORMFILLINFO_PDFiumTest* ToPDFiumTestFormFillInfo(
    FPDF_FORMFILLINFO* form_fill_info) {
  return static_cast<FPDF_FORMFILLINFO_PDFiumTest*>(form_fill_info);
}

static bool CheckDimensions(int stride, int width, int height) {
  if (stride < 0 || width < 0 || height < 0)
    return false;
  if (height > 0 && width > INT_MAX / height)
    return false;
  return true;
}

static void OutputMD5Hash(const char* file_name, const char* buffer, int len) {
  // Get the MD5 hash and write it to stdout.
  uint8_t digest[16];
  CRYPT_MD5Generate(reinterpret_cast<const uint8_t*>(buffer), len, digest);
  printf("MD5:%s:", file_name);
  for (int i = 0; i < 16; i++)
    printf("%02x", digest[i]);
  printf("\n");
}

static std::string WritePpm(const char* pdf_name,
                            int num,
                            const void* buffer_void,
                            int stride,
                            int width,
                            int height) {
  const char* buffer = reinterpret_cast<const char*>(buffer_void);

  if (!CheckDimensions(stride, width, height))
    return "";

  int out_len = width * height;
  if (out_len > INT_MAX / 3)
    return "";
  out_len *= 3;

  char filename[256];
  snprintf(filename, sizeof(filename), "%s.%d.ppm", pdf_name, num);
  FILE* fp = fopen(filename, "wb");
  if (!fp)
    return "";
  fprintf(fp, "P6\n# PDF test render\n%d %d\n255\n", width, height);
  // Source data is B, G, R, unused.
  // Dest data is R, G, B.
  std::vector<char> result(out_len);
  for (int h = 0; h < height; ++h) {
    const char* src_line = buffer + (stride * h);
    char* dest_line = result.data() + (width * h * 3);
    for (int w = 0; w < width; ++w) {
      // R
      dest_line[w * 3] = src_line[(w * 4) + 2];
      // G
      dest_line[(w * 3) + 1] = src_line[(w * 4) + 1];
      // B
      dest_line[(w * 3) + 2] = src_line[w * 4];
    }
  }
  fwrite(result.data(), out_len, 1, fp);
  fclose(fp);
  return std::string(filename);
}

void WriteText(FPDF_PAGE page, const char* pdf_name, int num) {
  char filename[256];
  int chars_formatted =
      snprintf(filename, sizeof(filename), "%s.%d.txt", pdf_name, num);
  if (chars_formatted < 0 ||
      static_cast<size_t>(chars_formatted) >= sizeof(filename)) {
    fprintf(stderr, "Filename %s is too long\n", filename);
    return;
  }

  FILE* fp = fopen(filename, "w");
  if (!fp) {
    fprintf(stderr, "Failed to open %s for output\n", filename);
    return;
  }

  // Output in UTF32-LE.
  uint32_t bom = 0x0000FEFF;
  fwrite(&bom, sizeof(bom), 1, fp);

  FPDF_TEXTPAGE textpage = FPDFText_LoadPage(page);
  for (int i = 0; i < FPDFText_CountChars(textpage); i++) {
    uint32_t c = FPDFText_GetUnicode(textpage, i);
    fwrite(&c, sizeof(c), 1, fp);
  }

  FPDFText_ClosePage(textpage);

  (void)fclose(fp);
}

static std::string WritePng(const char* pdf_name,
                            int num,
                            const void* buffer_void,
                            int stride,
                            int width,
                            int height) {
  if (!CheckDimensions(stride, width, height))
    return "";

  std::vector<unsigned char> png_encoding;
  const unsigned char* buffer = static_cast<const unsigned char*>(buffer_void);
  if (!image_diff_png::EncodeBGRAPNG(
          buffer, width, height, stride, false, &png_encoding)) {
    fprintf(stderr, "Failed to convert bitmap to PNG\n");
    return "";
  }

  char filename[256];
  int chars_formatted = snprintf(
      filename, sizeof(filename), "%s.%d.png", pdf_name, num);
  if (chars_formatted < 0 ||
      static_cast<size_t>(chars_formatted) >= sizeof(filename)) {
    fprintf(stderr, "Filename %s is too long\n", filename);
    return "";
  }

  FILE* fp = fopen(filename, "wb");
  if (!fp) {
    fprintf(stderr, "Failed to open %s for output\n", filename);
    return "";
  }

  size_t bytes_written = fwrite(
      &png_encoding.front(), 1, png_encoding.size(), fp);
  if (bytes_written != png_encoding.size())
    fprintf(stderr, "Failed to write to  %s\n", filename);

  (void)fclose(fp);
  return std::string(filename);
}

#ifdef _WIN32
static std::string WriteBmp(const char* pdf_name,
                            int num,
                            const void* buffer,
                            int stride,
                            int width,
                            int height) {
  if (!CheckDimensions(stride, width, height))
    return "";

  int out_len = stride * height;
  if (out_len > INT_MAX / 3)
    return "";

  char filename[256];
  snprintf(filename, sizeof(filename), "%s.%d.bmp", pdf_name, num);
  FILE* fp = fopen(filename, "wb");
  if (!fp)
    return "";

  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(bmi) - sizeof(RGBQUAD);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;  // top-down image
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;

  BITMAPFILEHEADER file_header = {};
  file_header.bfType = 0x4d42;
  file_header.bfSize = sizeof(file_header) + bmi.bmiHeader.biSize + out_len;
  file_header.bfOffBits = file_header.bfSize - out_len;

  fwrite(&file_header, sizeof(file_header), 1, fp);
  fwrite(&bmi, bmi.bmiHeader.biSize, 1, fp);
  fwrite(buffer, out_len, 1, fp);
  fclose(fp);
  return std::string(filename);
}

void WriteEmf(FPDF_PAGE page, const char* pdf_name, int num) {
  int width = static_cast<int>(FPDF_GetPageWidth(page));
  int height = static_cast<int>(FPDF_GetPageHeight(page));

  char filename[256];
  snprintf(filename, sizeof(filename), "%s.%d.emf", pdf_name, num);

  HDC dc = CreateEnhMetaFileA(nullptr, filename, nullptr, nullptr);

  HRGN rgn = CreateRectRgn(0, 0, width, height);
  SelectClipRgn(dc, rgn);
  DeleteObject(rgn);

  SelectObject(dc, GetStockObject(NULL_PEN));
  SelectObject(dc, GetStockObject(WHITE_BRUSH));
  // If a PS_NULL pen is used, the dimensions of the rectangle are 1 pixel less.
  Rectangle(dc, 0, 0, width + 1, height + 1);

  FPDF_RenderPage(dc, page, 0, 0, width, height, 0,
                  FPDF_ANNOT | FPDF_PRINTING | FPDF_NO_CATCH);

  DeleteEnhMetaFile(CloseEnhMetaFile(dc));
}
#endif

#ifdef PDF_ENABLE_SKIA
static std::string WriteSkp(const char* pdf_name,
                            int num,
                            SkPictureRecorder* recorder) {
  char filename[256];
  int chars_formatted =
      snprintf(filename, sizeof(filename), "%s.%d.skp", pdf_name, num);

  if (chars_formatted < 0 ||
      static_cast<size_t>(chars_formatted) >= sizeof(filename)) {
    fprintf(stderr, "Filename %s is too long\n", filename);
    return "";
  }

  sk_sp<SkPicture> picture(recorder->finishRecordingAsPicture());
  SkFILEWStream wStream(filename);
  picture->serialize(&wStream);
  return std::string(filename);
}
#endif

// These example JS platform callback handlers are entirely optional,
// and exist here to show the flow of information from a document back
// to the embedder.
int ExampleAppAlert(IPDF_JSPLATFORM*,
                    FPDF_WIDESTRING msg,
                    FPDF_WIDESTRING title,
                    int type,
                    int icon) {
  printf("%ls", GetPlatformWString(title).c_str());
  if (icon || type)
    printf("[icon=%d,type=%d]", icon, type);
  printf(": %ls\n", GetPlatformWString(msg).c_str());
  return 0;
}

int ExampleAppResponse(IPDF_JSPLATFORM*,
                       FPDF_WIDESTRING question,
                       FPDF_WIDESTRING title,
                       FPDF_WIDESTRING default_value,
                       FPDF_WIDESTRING label,
                       FPDF_BOOL is_password,
                       void* response,
                       int length) {
  printf("%ls: %ls, defaultValue=%ls, label=%ls, isPassword=%d, length=%d\n",
         GetPlatformWString(title).c_str(),
         GetPlatformWString(question).c_str(),
         GetPlatformWString(default_value).c_str(),
         GetPlatformWString(label).c_str(), is_password, length);

  // UTF-16, always LE regardless of platform.
  uint8_t* ptr = static_cast<uint8_t*>(response);
  ptr[0] = 'N';
  ptr[1] = 0;
  ptr[2] = 'o';
  ptr[3] = 0;
  return 4;
}

void ExampleDocGotoPage(IPDF_JSPLATFORM*, int page_number) {
  printf("Goto Page: %d\n", page_number);
}

void ExampleDocMail(IPDF_JSPLATFORM*,
                    void* mailData,
                    int length,
                    FPDF_BOOL UI,
                    FPDF_WIDESTRING To,
                    FPDF_WIDESTRING Subject,
                    FPDF_WIDESTRING CC,
                    FPDF_WIDESTRING BCC,
                    FPDF_WIDESTRING Msg) {
  printf("Mail Msg: %d, to=%ls, cc=%ls, bcc=%ls, subject=%ls, body=%ls\n", UI,
         GetPlatformWString(To).c_str(), GetPlatformWString(CC).c_str(),
         GetPlatformWString(BCC).c_str(), GetPlatformWString(Subject).c_str(),
         GetPlatformWString(Msg).c_str());
}

void ExampleUnsupportedHandler(UNSUPPORT_INFO*, int type) {
  std::string feature = "Unknown";
  switch (type) {
    case FPDF_UNSP_DOC_XFAFORM:
      feature = "XFA";
      break;
    case FPDF_UNSP_DOC_PORTABLECOLLECTION:
      feature = "Portfolios_Packages";
      break;
    case FPDF_UNSP_DOC_ATTACHMENT:
    case FPDF_UNSP_ANNOT_ATTACHMENT:
      feature = "Attachment";
      break;
    case FPDF_UNSP_DOC_SECURITY:
      feature = "Rights_Management";
      break;
    case FPDF_UNSP_DOC_SHAREDREVIEW:
      feature = "Shared_Review";
      break;
    case FPDF_UNSP_DOC_SHAREDFORM_ACROBAT:
    case FPDF_UNSP_DOC_SHAREDFORM_FILESYSTEM:
    case FPDF_UNSP_DOC_SHAREDFORM_EMAIL:
      feature = "Shared_Form";
      break;
    case FPDF_UNSP_ANNOT_3DANNOT:
      feature = "3D";
      break;
    case FPDF_UNSP_ANNOT_MOVIE:
      feature = "Movie";
      break;
    case FPDF_UNSP_ANNOT_SOUND:
      feature = "Sound";
      break;
    case FPDF_UNSP_ANNOT_SCREEN_MEDIA:
    case FPDF_UNSP_ANNOT_SCREEN_RICHMEDIA:
      feature = "Screen";
      break;
    case FPDF_UNSP_ANNOT_SIG:
      feature = "Digital_Signature";
      break;
  }
  printf("Unsupported feature: %s.\n", feature.c_str());
}

bool ParseCommandLine(const std::vector<std::string>& args,
                      Options* options,
                      std::vector<std::string>* files) {
  if (args.empty())
    return false;

  options->exe_path = args[0];
  size_t cur_idx = 1;
  for (; cur_idx < args.size(); ++cur_idx) {
    const std::string& cur_arg = args[cur_idx];
    if (cur_arg == "--show-config") {
      options->show_config = true;
    } else if (cur_arg == "--send-events") {
      options->send_events = true;
    } else if (cur_arg == "--ppm") {
      if (options->output_format != OUTPUT_NONE) {
        fprintf(stderr, "Duplicate or conflicting --ppm argument\n");
        return false;
      }
      options->output_format = OUTPUT_PPM;
    } else if (cur_arg == "--png") {
      if (options->output_format != OUTPUT_NONE) {
        fprintf(stderr, "Duplicate or conflicting --png argument\n");
        return false;
      }
      options->output_format = OUTPUT_PNG;
    } else if (cur_arg == "--txt") {
      if (options->output_format != OUTPUT_NONE) {
        fprintf(stderr, "Duplicate or conflicting --txt argument\n");
        return false;
      }
      options->output_format = OUTPUT_TEXT;
#ifdef PDF_ENABLE_SKIA
    } else if (cur_arg == "--skp") {
      if (options->output_format != OUTPUT_NONE) {
        fprintf(stderr, "Duplicate or conflicting --skp argument\n");
        return false;
      }
      options->output_format = OUTPUT_SKP;
#endif
    } else if (cur_arg.size() > 11 &&
               cur_arg.compare(0, 11, "--font-dir=") == 0) {
      if (!options->font_directory.empty()) {
        fprintf(stderr, "Duplicate --font-dir argument\n");
        return false;
      }
      options->font_directory = cur_arg.substr(11);
#ifdef _WIN32
    } else if (cur_arg == "--emf") {
      if (options->output_format != OUTPUT_NONE) {
        fprintf(stderr, "Duplicate or conflicting --emf argument\n");
        return false;
      }
      options->output_format = OUTPUT_EMF;
    } else if (cur_arg == "--bmp") {
      if (options->output_format != OUTPUT_NONE) {
        fprintf(stderr, "Duplicate or conflicting --bmp argument\n");
        return false;
      }
      options->output_format = OUTPUT_BMP;
#endif  // _WIN32

#ifdef PDF_ENABLE_V8
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
    } else if (cur_arg.size() > 10 &&
               cur_arg.compare(0, 10, "--bin-dir=") == 0) {
      if (!options->bin_directory.empty()) {
        fprintf(stderr, "Duplicate --bin-dir argument\n");
        return false;
      }
      options->bin_directory = cur_arg.substr(10);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
#endif  // PDF_ENABLE_V8

    } else if (cur_arg.size() > 8 && cur_arg.compare(0, 8, "--scale=") == 0) {
      if (!options->scale_factor_as_string.empty()) {
        fprintf(stderr, "Duplicate --scale argument\n");
        return false;
      }
      options->scale_factor_as_string = cur_arg.substr(8);
    } else if (cur_arg.size() > 8 && cur_arg.compare(0, 8, "--pages=") == 0) {
      if (options->pages) {
        fprintf(stderr, "Duplicate --pages argument\n");
        return false;
      }
      options->pages = true;
      const std::string pages_string = cur_arg.substr(8);
      size_t first_dash = pages_string.find("-");
      if (first_dash == std::string::npos) {
        std::stringstream(pages_string) >> options->first_page;
        options->last_page = options->first_page;
      } else {
        std::stringstream(pages_string.substr(0, first_dash)) >>
            options->first_page;
        std::stringstream(pages_string.substr(first_dash + 1)) >>
            options->last_page;
      }
    } else if (cur_arg == "--md5") {
      options->md5 = true;
    } else if (cur_arg.size() >= 2 && cur_arg[0] == '-' && cur_arg[1] == '-') {
      fprintf(stderr, "Unrecognized argument %s\n", cur_arg.c_str());
      return false;
    } else {
      break;
    }
  }
  for (size_t i = cur_idx; i < args.size(); i++)
    files->push_back(args[i]);

  return true;
}

FPDF_BOOL Is_Data_Avail(FX_FILEAVAIL* avail, size_t offset, size_t size) {
  return true;
}

void Add_Segment(FX_DOWNLOADHINTS* hints, size_t offset, size_t size) {}

void SendPageEvents(const FPDF_FORMHANDLE& form,
                    const FPDF_PAGE& page,
                    const std::string& events) {
  auto lines = StringSplit(events, '\n');
  for (auto line : lines) {
    auto command = StringSplit(line, '#');
    if (command[0].empty())
      continue;
    auto tokens = StringSplit(command[0], ',');
    if (tokens[0] == "keycode") {
      if (tokens.size() == 2) {
        int keycode = atoi(tokens[1].c_str());
        FORM_OnKeyDown(form, page, keycode, 0);
        FORM_OnKeyUp(form, page, keycode, 0);
      } else {
        fprintf(stderr, "keycode: bad args\n");
      }
    } else if (tokens[0] == "mousedown") {
      if (tokens.size() == 4) {
        int x = atoi(tokens[2].c_str());
        int y = atoi(tokens[3].c_str());
        if (tokens[1] == "left")
          FORM_OnLButtonDown(form, page, 0, x, y);
#ifdef PDF_ENABLE_XFA
        else if (tokens[1] == "right")
          FORM_OnRButtonDown(form, page, 0, x, y);
#endif
        else
          fprintf(stderr, "mousedown: bad button name\n");
      } else {
        fprintf(stderr, "mousedown: bad args\n");
      }
    } else if (tokens[0] == "mouseup") {
      if (tokens.size() == 4) {
        int x = atoi(tokens[2].c_str());
        int y = atoi(tokens[3].c_str());
        if (tokens[1] == "left")
          FORM_OnLButtonUp(form, page, 0, x, y);
#ifdef PDF_ENABLE_XFA
        else if (tokens[1] == "right")
          FORM_OnRButtonUp(form, page, 0, x, y);
#endif
        else
          fprintf(stderr, "mouseup: bad button name\n");
      } else {
        fprintf(stderr, "mouseup: bad args\n");
      }
    } else if (tokens[0] == "mousemove") {
      if (tokens.size() == 3) {
        int x = atoi(tokens[1].c_str());
        int y = atoi(tokens[2].c_str());
        FORM_OnMouseMove(form, page, 0, x, y);
      } else {
        fprintf(stderr, "mousemove: bad args\n");
      }
    } else {
      fprintf(stderr, "Unrecognized event: %s\n", tokens[0].c_str());
    }
  }
}

FPDF_PAGE GetPageForIndex(FPDF_FORMFILLINFO* param,
                          FPDF_DOCUMENT doc,
                          int index) {
  FPDF_FORMFILLINFO_PDFiumTest* form_fill_info =
      ToPDFiumTestFormFillInfo(param);
  auto& loaded_pages = form_fill_info->loaded_pages;

  auto iter = loaded_pages.find(index);
  if (iter != loaded_pages.end())
    return iter->second;

  FPDF_PAGE page = FPDF_LoadPage(doc, index);
  if (!page)
    return nullptr;

  FPDF_FORMHANDLE& form_handle = form_fill_info->form_handle;

  FORM_OnAfterLoadPage(page, form_handle);
  FORM_DoPageAAction(page, form_handle, FPDFPAGE_AACTION_OPEN);

  loaded_pages[index] = page;
  return page;
}

bool RenderPage(const std::string& name,
                FPDF_DOCUMENT doc,
                FPDF_FORMHANDLE& form,
                FPDF_FORMFILLINFO_PDFiumTest& form_fill_info,
                const int page_index,
                const Options& options,
                const std::string& events) {
  FPDF_PAGE page = GetPageForIndex(&form_fill_info, doc, page_index);
  if (!page)
    return false;

  FPDF_TEXTPAGE text_page = FPDFText_LoadPage(page);

  if (options.send_events)
    SendPageEvents(form, page, events);

  double scale = 1.0;
  if (!options.scale_factor_as_string.empty())
    std::stringstream(options.scale_factor_as_string) >> scale;

  int width = static_cast<int>(FPDF_GetPageWidth(page) * scale);
  int height = static_cast<int>(FPDF_GetPageHeight(page) * scale);
  int alpha = FPDFPage_HasTransparency(page) ? 1 : 0;
  FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, alpha);
  if (bitmap) {
    FPDF_DWORD fill_color = alpha ? 0x00000000 : 0xFFFFFFFF;
    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, fill_color);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, FPDF_ANNOT);

    FPDF_FFLDraw(form, bitmap, page, 0, 0, width, height, 0, FPDF_ANNOT);
    int stride = FPDFBitmap_GetStride(bitmap);
    const char* buffer =
        reinterpret_cast<const char*>(FPDFBitmap_GetBuffer(bitmap));

    std::string&& image_file_name = "";
    switch (options.output_format) {
#ifdef _WIN32
      case OUTPUT_BMP:
        image_file_name =
            WriteBmp(name.c_str(), page_index, buffer, stride, width, height);
        break;

      case OUTPUT_EMF:
        WriteEmf(page, name.c_str(), page_index);
        break;
#endif
      case OUTPUT_TEXT:
        WriteText(page, name.c_str(), page_index);
        break;

      case OUTPUT_PNG:
        image_file_name =
            WritePng(name.c_str(), page_index, buffer, stride, width, height);
        break;

      case OUTPUT_PPM:
        image_file_name =
            WritePpm(name.c_str(), page_index, buffer, stride, width, height);
        break;

#ifdef PDF_ENABLE_SKIA
      case OUTPUT_SKP: {
        std::unique_ptr<SkPictureRecorder> recorder(
            reinterpret_cast<SkPictureRecorder*>(
                FPDF_RenderPageSkp(page, width, height)));
        FPDF_FFLRecord(form, recorder.get(), page, 0, 0, width, height, 0, 0);
        image_file_name = WriteSkp(name.c_str(), page_index, recorder.get());
      } break;
#endif
      default:
        break;
    }

    // Write the filename and the MD5 of the buffer to stdout if we wrote a
    // file.
    if (options.md5 && image_file_name != "")
      OutputMD5Hash(image_file_name.c_str(), buffer, stride * height);

    FPDFBitmap_Destroy(bitmap);
  } else {
    fprintf(stderr, "Page was too large to be rendered.\n");
  }

  form_fill_info.loaded_pages.erase(page_index);

  FORM_DoPageAAction(page, form, FPDFPAGE_AACTION_CLOSE);
  FORM_OnBeforeClosePage(page, form);
  FPDFText_ClosePage(text_page);
  FPDF_ClosePage(page);
  return !!bitmap;
}

void RenderPdf(const std::string& name,
               const char* pBuf,
               size_t len,
               const Options& options,
               const std::string& events) {
  IPDF_JSPLATFORM platform_callbacks;
  memset(&platform_callbacks, '\0', sizeof(platform_callbacks));
  platform_callbacks.version = 3;
  platform_callbacks.app_alert = ExampleAppAlert;
  platform_callbacks.app_response = ExampleAppResponse;
  platform_callbacks.Doc_gotoPage = ExampleDocGotoPage;
  platform_callbacks.Doc_mail = ExampleDocMail;

  FPDF_FORMFILLINFO_PDFiumTest form_callbacks = {};
#ifdef PDF_ENABLE_XFA
  form_callbacks.version = 2;
#else   // PDF_ENABLE_XFA
  form_callbacks.version = 1;
#endif  // PDF_ENABLE_XFA
  form_callbacks.FFI_GetPage = GetPageForIndex;
  form_callbacks.m_pJsPlatform = &platform_callbacks;

  TestLoader loader(pBuf, len);
  FPDF_FILEACCESS file_access;
  memset(&file_access, '\0', sizeof(file_access));
  file_access.m_FileLen = static_cast<unsigned long>(len);
  file_access.m_GetBlock = TestLoader::GetBlock;
  file_access.m_Param = &loader;

  FX_FILEAVAIL file_avail;
  memset(&file_avail, '\0', sizeof(file_avail));
  file_avail.version = 1;
  file_avail.IsDataAvail = Is_Data_Avail;

  FX_DOWNLOADHINTS hints;
  memset(&hints, '\0', sizeof(hints));
  hints.version = 1;
  hints.AddSegment = Add_Segment;

  FPDF_DOCUMENT doc;
  int nRet = PDF_DATA_NOTAVAIL;
  bool bIsLinearized = false;
  FPDF_AVAIL pdf_avail = FPDFAvail_Create(&file_avail, &file_access);
  std::unique_ptr<void, AvailDeleter> scoped_pdf_avail_deleter(pdf_avail);

  if (FPDFAvail_IsLinearized(pdf_avail) == PDF_LINEARIZED) {
    doc = FPDFAvail_GetDocument(pdf_avail, nullptr);
    if (doc) {
      while (nRet == PDF_DATA_NOTAVAIL)
        nRet = FPDFAvail_IsDocAvail(pdf_avail, &hints);

      if (nRet == PDF_DATA_ERROR) {
        fprintf(stderr, "Unknown error in checking if doc was available.\n");
        FPDF_CloseDocument(doc);
        return;
      }
      nRet = FPDFAvail_IsFormAvail(pdf_avail, &hints);
      if (nRet == PDF_FORM_ERROR || nRet == PDF_FORM_NOTAVAIL) {
        fprintf(stderr,
                "Error %d was returned in checking if form was available.\n",
                nRet);
        FPDF_CloseDocument(doc);
        return;
      }
      bIsLinearized = true;
    }
  } else {
    doc = FPDF_LoadCustomDocument(&file_access, nullptr);
  }

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

    return;
  }

  (void)FPDF_GetDocPermissions(doc);

  FPDF_FORMHANDLE form = FPDFDOC_InitFormFillEnvironment(doc, &form_callbacks);
  form_callbacks.form_handle = form;

#ifdef PDF_ENABLE_XFA
  int doc_type = DOCTYPE_PDF;
  if (FPDF_HasXFAField(doc, &doc_type) && doc_type != DOCTYPE_PDF &&
      !FPDF_LoadXFA(doc)) {
    fprintf(stderr, "LoadXFA unsuccessful, continuing anyway.\n");
  }
#endif  // PDF_ENABLE_XFA
  FPDF_SetFormFieldHighlightColor(form, 0, 0xFFE4DD);
  FPDF_SetFormFieldHighlightAlpha(form, 100);

  FORM_DoDocumentJSAction(form);
  FORM_DoDocumentOpenAction(form);

  int page_count = FPDF_GetPageCount(doc);
  int rendered_pages = 0;
  int bad_pages = 0;
  int first_page = options.pages ? options.first_page : 0;
  int last_page = options.pages ? options.last_page + 1 : page_count;
  for (int i = first_page; i < last_page; ++i) {
    if (bIsLinearized) {
      nRet = PDF_DATA_NOTAVAIL;
      while (nRet == PDF_DATA_NOTAVAIL)
        nRet = FPDFAvail_IsPageAvail(pdf_avail, i, &hints);

      if (nRet == PDF_DATA_ERROR) {
        fprintf(stderr, "Unknown error in checking if page %d is available.\n",
                i);
        FPDFDOC_ExitFormFillEnvironment(form);
        FPDF_CloseDocument(doc);
        return;
      }
    }
    if (RenderPage(name, doc, form, form_callbacks, i, options, events))
      ++rendered_pages;
    else
      ++bad_pages;
  }

  FORM_DoDocumentAAction(form, FPDFDOC_AACTION_WC);

  FPDFDOC_ExitFormFillEnvironment(form);
  FPDF_CloseDocument(doc);

  fprintf(stderr, "Rendered %d pages.\n", rendered_pages);
  if (bad_pages)
    fprintf(stderr, "Skipped %d bad pages.\n", bad_pages);
}

static void ShowConfig() {
  std::string config;
  std::string maybe_comma;
#if PDF_ENABLE_V8
  config.append(maybe_comma);
  config.append("V8");
  maybe_comma = ",";
#endif  // PDF_ENABLE_V8
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  config.append(maybe_comma);
  config.append("V8_EXTERNAL");
  maybe_comma = ",";
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
#ifdef PDF_ENABLE_XFA
  config.append(maybe_comma);
  config.append("XFA");
  maybe_comma = ",";
#endif  // PDF_ENABLE_XFA
#ifdef PDF_ENABLE_ASAN
  config.append(maybe_comma);
  config.append("ASAN");
  maybe_comma = ",";
#endif  // PDF_ENABLE_ASAN
  printf("%s\n", config.c_str());
}

static const char kUsageString[] =
    "Usage: pdfium_test [OPTION] [FILE]...\n"
    "  --show-config     - print build options and exit\n"
    "  --send-events     - send input described by .evt file\n"
    "  --bin-dir=<path>  - override path to v8 external data\n"
    "  --font-dir=<path> - override path to external fonts\n"
    "  --scale=<number>  - scale output size by number (e.g. 0.5)\n"
    "  --pages=<number>(-<number>) - only render the given 0-based page(s)\n"
#ifdef _WIN32
    "  --bmp - write page images <pdf-name>.<page-number>.bmp\n"
    "  --emf - write page meta files <pdf-name>.<page-number>.emf\n"
#endif  // _WIN32
    "  --txt - write page text in UTF32-LE <pdf-name>.<page-number>.txt\n"
    "  --png - write page images <pdf-name>.<page-number>.png\n"
    "  --ppm - write page images <pdf-name>.<page-number>.ppm\n"
#ifdef PDF_ENABLE_SKIA
    "  --skp - write page images <pdf-name>.<page-number>.skp\n"
#endif
    "  --md5 - write output image paths and their md5 hashes to stdout.\n"
    "";

int main(int argc, const char* argv[]) {
  std::vector<std::string> args(argv, argv + argc);
  Options options;
  std::vector<std::string> files;
  if (!ParseCommandLine(args, &options, &files)) {
    fprintf(stderr, "%s", kUsageString);
    return 1;
  }

  if (options.show_config) {
    ShowConfig();
    return 0;
  }

  if (files.empty()) {
    fprintf(stderr, "No input files.\n");
    return 1;
  }

#ifdef PDF_ENABLE_V8
  v8::Platform* platform;
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  v8::StartupData natives;
  v8::StartupData snapshot;
  InitializeV8ForPDFium(options.exe_path, options.bin_directory, &natives,
                        &snapshot, &platform);
#else   // V8_USE_EXTERNAL_STARTUP_DATA
  InitializeV8ForPDFium(options.exe_path, &platform);
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
#endif  // PDF_ENABLE_V8

  FPDF_LIBRARY_CONFIG config;
  config.version = 2;
  config.m_pUserFontPaths = nullptr;
  config.m_pIsolate = nullptr;
  config.m_v8EmbedderSlot = 0;

  const char* path_array[2];
  if (!options.font_directory.empty()) {
    path_array[0] = options.font_directory.c_str();
    path_array[1] = nullptr;
    config.m_pUserFontPaths = path_array;
  }
  FPDF_InitLibraryWithConfig(&config);

  UNSUPPORT_INFO unsupported_info;
  memset(&unsupported_info, '\0', sizeof(unsupported_info));
  unsupported_info.version = 1;
  unsupported_info.FSDK_UnSupport_Handler = ExampleUnsupportedHandler;

  FSDK_SetUnSpObjProcessHandler(&unsupported_info);

  for (const std::string& filename : files) {
    size_t file_length = 0;
    std::unique_ptr<char, pdfium::FreeDeleter> file_contents =
        GetFileContents(filename.c_str(), &file_length);
    if (!file_contents)
      continue;
    fprintf(stderr, "Rendering PDF file %s.\n", filename.c_str());
    std::string events;
    if (options.send_events) {
      std::string event_filename = filename;
      size_t event_length = 0;
      size_t extension_pos = event_filename.find(".pdf");
      if (extension_pos != std::string::npos) {
        event_filename.replace(extension_pos, 4, ".evt");
        if (access(event_filename.c_str(), R_OK) == 0) {
          fprintf(stderr, "Using event file %s.\n", event_filename.c_str());
          std::unique_ptr<char, pdfium::FreeDeleter> event_contents =
              GetFileContents(event_filename.c_str(), &event_length);
          if (event_contents) {
            fprintf(stderr, "Sending events from: %s\n",
                    event_filename.c_str());
            events = std::string(event_contents.get(), event_length);
          }
        }
      }
    }
    RenderPdf(filename, file_contents.get(), file_length, options, events);
  }

  FPDF_DestroyLibrary();
#ifdef PDF_ENABLE_V8
  v8::V8::ShutdownPlatform();
  delete platform;

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  free(const_cast<char*>(natives.data));
  free(const_cast<char*>(snapshot.data));
#endif  // V8_USE_EXTERNAL_STARTUP_DATA
#endif  // PDF_ENABLE_V8

  return 0;
}
