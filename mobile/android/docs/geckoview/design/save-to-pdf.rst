GeckoView Save to PDF
=====================

Olivia Hall <ohall@mozilla.com>, Jonathan Almeida <jon@mozilla.com>

Why
---

- The Save to PDF feature was originally available in Fennec and users would
  like to see the return of this feature. There are a lot of user requests for
  Save to PDF in Fenix.
- We would have more parity with Desktop, and be able to share the same
  underlying implementation with them.
- Product is currently evaluating the addition of pdf.js as well; having Save
  to PDF would be an added bonus.

Goals
-----

- Save the current page to a text-based PDF document.
- Embedders should also be able to call into GeckoView to provide a PDF copy of
  the selected GeckoSession.
- Enable the ability to iterate on PDF customizations.

Non-Goals
---------

- We do not want to implement a PDF “preview” of the document prior to the
  download. This has open questions: does Product want this, should this be
  implemented by the embedder, etc.
- The generated PDF should not match the theme (e.g., light or dark mode) of
  the currently displayed page - the PDF will always appear as themeless or as
  a plain document.
- No customizable settings. The current API design will not include
  customization settings that the embedder can control. This can be worked on
  in a follow-up feature request. Our current API design however, would enable
  for these particular iterations.

What
----

This work will add a method to ``GeckoSession`` called ``savePdf`` for
embedders to use, which will communicate with a new ``GeckoViewPdf.jsm`` to
create the PDF file. When the document is available, the
``GeckoViewPdfController`` will notify the
``ContentDelegate.onExternalResponse`` with the downloadable document.

- ``GeckoViewPdf.jsm`` - JavaScript implementation that converts the content to
  a PDF and saves the file, also responds to messaging from
  ``GeckoViewPdfController``.
- ``GeckoViewPdfController.java`` - The Controller coordinates between the Java
  and JS through response messaging and notifies the content delegate when the
  PDF is available for use.

API
---

GeckoSession.java
^^^^^^^^^^^^^^^^^

.. code:: java

  public class GeckoSession {
    public GeckoSession(final @Nullable GeckoSessionSettings settings) {
      mPdfController = new PdfController(this);
    }

    @UiThread
    public void saveAsPdf(PdfSettings settings) {
      mPdfController.savePdf(null);
    }
  }


GeckoViewPdf.jsm
^^^^^^^^^^^^^^^^
.. code:: java

  this.registerListener([
      "GeckoView:SavePdf",
    ]);

  async onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoView:SavePdf":
        this.saveToPDF();
        Break;
      }
    }
  }

  async saveToPDF() {
   // Reference: https://searchfox.org/mozilla-central/source/remote/cdp/domains/parent/Page.jsm#519
  }


GeckoViewPdfController.java
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
.. code:: java

  class PdfController {
    private static final String LOGTAG = "PdfController";
    private final GeckoSession mSession;

    PdfController(final GeckoSession session) {
      mSession = session;
    }

    private PdfDelegate mDelegate;
    private BundleEventListener mEventListener;

    /* package */
    PdfController() {
      mEventListener = new EventListener();
      EventDispatcher.getInstance()
        .registerUiThreadListener(mEventListener,"GeckoView:PdfSaved");
    }

    @UiThread
    public void setDelegate(final @Nullable PdfDelegate delegate) {
      ThreadUtils.assertOnUiThread();
      mDelegate = delegate;
    }

    @UiThread
    @Nullable
    public PdfDelegate getDelegate() {
      ThreadUtils.assertOnUiThread();
      return mDelegate;
    }

    @UiThread
    public void savePdf() {
      ThreadUtils.assertOnUiThread();
      mEventDispatcher.dispatch("GeckoView:SavePdf", null);
    }


    private class EventListener implements BundleEventListener {

      @Override
      public void handleMessage(
        final String event,
        final GeckoBundle message,
        final EventCallback callback
      ) {
        if (mDelegate == null) {
          callback.sendError("Not allowed");
          return;
        }

        switch (event) {
          case "GeckoView:PdfSaved": {
            final ContentDelegate delegate = mSession.getContentDelegate();

            if (message.containsKey("pdfPath")) {
            InputStream inputStream; /* construct InputStream from local file path */
            WebResponse response = WebResponse.Builder()
              .body(inputStream)
              // Add other attributes as well.
              .build();

              if (delegate != null) {
                delegate.onExternalResponse(mSession, response);
              } else {
                throw Exception("Needs ContentDelegate for this to work.")
              }
            }

            break;
          }
        }
      }
    }
  }

geckoview.js
^^^^^^^^^^^^
.. code:: java

  {
    name: "GeckoViewPdf",
    onInit: {
       resource: "resource://gre/modules/GeckoViewPdf.jsm",
    }
  }


Testing
-------

- Tests for the jsm and java code will be covered by mochitests and junit.
- Make assertions to check that the text and images are in the finished PDF;
  the PDF is a non-zero file size.

Risks
-----

The API and the code that this work would be using are pretty new, currently
prefered off in Nightly and could contain implementation bugs.
