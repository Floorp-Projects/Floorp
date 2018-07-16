/*
 * Copyright 2014, Leanplum, Inc. All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package com.leanplum.messagetemplates;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Typeface;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.graphics.drawable.shapes.Shape;
import android.os.Build;
import android.os.Handler;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.DecelerateInterpolator;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.leanplum.ActionContext;
import com.leanplum.Leanplum;
import com.leanplum.utils.BitmapUtil;
import com.leanplum.utils.SizeUtil;
import com.leanplum.views.BackgroundImageView;
import com.leanplum.views.CloseButton;

import org.json.JSONObject;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.Map;

/**
 * Base dialog used to display the Center Popup, Interstitial, Web Interstitial, HTML template.
 *
 * @author Martin Yanakiev, Anna Orlova
 */
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Typeface;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.graphics.drawable.shapes.Shape;
import android.os.Build;
import android.os.Handler;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.AccelerateInterpolator;
import android.view.animation.AlphaAnimation;
import android.view.animation.Animation;
import android.view.animation.DecelerateInterpolator;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.leanplum.ActionContext;
import com.leanplum.Leanplum;
import com.leanplum.utils.BitmapUtil;
import com.leanplum.utils.SizeUtil;
import com.leanplum.views.BackgroundImageView;
import com.leanplum.views.CloseButton;

import org.json.JSONObject;

import java.io.UnsupportedEncodingException;
import java.net.URLDecoder;
import java.util.Map;

/**
 * Base dialog used to display the Center Popup, Interstitial, Web Interstitial, HTML template.
 *
 * @author Martin Yanakiev, Anna Orlova
 */
public class BaseMessageDialog extends Dialog {
  protected RelativeLayout dialogView;
  protected BaseMessageOptions options;
  protected WebInterstitialOptions webOptions;
  protected HTMLOptions htmlOptions;
  protected Activity activity;
  protected WebView webView;

  private boolean isWeb = false;
  private boolean isHtml = false;
  private boolean isClosing = false;

  protected BaseMessageDialog(Activity activity, boolean fullscreen, BaseMessageOptions options,
                              WebInterstitialOptions webOptions, HTMLOptions htmlOptions) {
    super(activity, getTheme(activity));

    SizeUtil.init(activity);
    this.activity = activity;
    this.options = options;
    this.webOptions = webOptions;
    this.htmlOptions = htmlOptions;
    if (webOptions != null) {
      isWeb = true;
    }
    if (htmlOptions != null) {
      isHtml = true;
    }
    dialogView = new RelativeLayout(activity);
    RelativeLayout.LayoutParams layoutParams = new RelativeLayout.LayoutParams(
            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    dialogView.setBackgroundColor(Color.TRANSPARENT);
    dialogView.setLayoutParams(layoutParams);

    RelativeLayout view = createContainerView(activity, fullscreen);
    view.setId(108);
    dialogView.addView(view, view.getLayoutParams());

    if ((!isWeb || (webOptions != null && webOptions.hasDismissButton())) && !isHtml) {
      CloseButton closeButton = createCloseButton(activity, fullscreen, view);
      dialogView.addView(closeButton, closeButton.getLayoutParams());
    }

    setContentView(dialogView, dialogView.getLayoutParams());

    dialogView.setAnimation(createFadeInAnimation());

    if (!fullscreen) {
      Window window = getWindow();
      if (window == null) {
        return;
      }
      if (!isHtml) {
        window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        if (Build.VERSION.SDK_INT >= 14) {
          window.setDimAmount(0.7f);
        }
      } else {
        window.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        window.setFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
                WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        if (htmlOptions != null &&
                MessageTemplates.Args.HTML_ALIGN_BOTTOM.equals(htmlOptions.getHtmlAlign())) {
          dialogView.setGravity(Gravity.BOTTOM);
        }
      }
    }
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
    try {
      if (webView != null) {
        if (hasFocus) {
          webView.onResume();
        } else {
          webView.onPause();
        }
      }
    } catch (Throwable ignore) {
    }
    super.onWindowFocusChanged(hasFocus);
  }

  private Animation createFadeInAnimation() {
    Animation fadeIn = new AlphaAnimation(0, 1);
    fadeIn.setInterpolator(new DecelerateInterpolator());
    fadeIn.setDuration(350);
    return fadeIn;
  }

  private Animation createFadeOutAnimation() {
    Animation fadeOut = new AlphaAnimation(1, 0);
    fadeOut.setInterpolator(new AccelerateInterpolator());
    fadeOut.setDuration(350);
    return fadeOut;
  }

  @Override
  public void cancel() {
    if (isClosing) {
      return;
    }
    isClosing = true;
    Animation animation = createFadeOutAnimation();
    animation.setAnimationListener(new Animation.AnimationListener() {
      @Override
      public void onAnimationStart(Animation animation) {
      }

      @Override
      public void onAnimationRepeat(Animation animation) {
      }

      @Override
      public void onAnimationEnd(Animation animation) {
        BaseMessageDialog.super.cancel();
        Handler handler = new Handler();
        handler.postDelayed(new Runnable() {
          @Override
          public void run() {
            if (isHtml && webView != null) {
              webView.stopLoading();
              webView.loadUrl("");
              if (dialogView != null) {
                dialogView.removeAllViews();
              }
              webView.removeAllViews();
              webView.destroy();
            }
          }
        }, 10);
      }
    });
    dialogView.startAnimation(animation);
  }

  private CloseButton createCloseButton(Activity context, boolean fullscreen, View parent) {
    CloseButton closeButton = new CloseButton(context);
    closeButton.setId(103);
    RelativeLayout.LayoutParams closeLayout = new RelativeLayout.LayoutParams(
            LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
    if (fullscreen) {
      closeLayout.addRule(RelativeLayout.ALIGN_PARENT_TOP, dialogView.getId());
      closeLayout.addRule(RelativeLayout.ALIGN_PARENT_RIGHT, dialogView.getId());
      closeLayout.setMargins(0, SizeUtil.dp5, SizeUtil.dp5, 0);
    } else {
      closeLayout.addRule(RelativeLayout.ALIGN_TOP, parent.getId());
      closeLayout.addRule(RelativeLayout.ALIGN_RIGHT, parent.getId());
      closeLayout.setMargins(0, -SizeUtil.dp7, -SizeUtil.dp7, 0);
    }
    closeButton.setLayoutParams(closeLayout);
    closeButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View arg0) {
        cancel();
      }
    });
    return closeButton;
  }

  @SuppressWarnings("deprecation")
  private RelativeLayout createContainerView(Activity context, boolean fullscreen) {
    RelativeLayout view = new RelativeLayout(context);

    // Positions the dialog.
    RelativeLayout.LayoutParams layoutParams;
    if (fullscreen) {
      layoutParams = new RelativeLayout.LayoutParams(
              LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    } else if (isHtml) {
      int height = SizeUtil.dpToPx(context, htmlOptions.getHtmlHeight());
      HTMLOptions.Size htmlWidth = htmlOptions.getHtmlWidth();
      if (htmlWidth == null || TextUtils.isEmpty(htmlWidth.type)) {
        layoutParams = new RelativeLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, height);
      } else {
        int width = htmlWidth.value;
        if ("%".equals(htmlWidth.type)) {
          Point size = SizeUtil.getDisplaySize(context);
          width = size.x * width / 100;
        } else {
          width = SizeUtil.dpToPx(context, width);
        }
        layoutParams = new RelativeLayout.LayoutParams(width, height);
      }

      layoutParams.addRule(RelativeLayout.CENTER_HORIZONTAL, RelativeLayout.TRUE);
      int htmlYOffset = htmlOptions.getHtmlYOffset(context);
      if (MessageTemplates.Args.HTML_ALIGN_BOTTOM.equals(htmlOptions.getHtmlAlign())) {
        layoutParams.bottomMargin = htmlYOffset;
      } else {
        layoutParams.topMargin = htmlYOffset;
      }
    } else {
      // Make sure the dialog fits on screen.
      Point size = SizeUtil.getDisplaySize(context);
      int width = SizeUtil.dpToPx(context, ((CenterPopupOptions) options).getWidth());
      int height = SizeUtil.dpToPx(context, ((CenterPopupOptions) options).getHeight());

      int maxWidth = size.x - SizeUtil.dp20;
      int maxHeight = size.y - SizeUtil.dp20;
      double aspectRatio = width / (double) height;
      if (width > maxWidth && (int) (width / aspectRatio) < maxHeight) {
        width = maxWidth;
        height = (int) (width / aspectRatio);
      }
      if (height > maxHeight && (int) (height * aspectRatio) < maxWidth) {
        height = maxHeight;
        width = (int) (height * aspectRatio);
      }

      layoutParams = new RelativeLayout.LayoutParams(width, height);
      layoutParams.addRule(RelativeLayout.CENTER_IN_PARENT, RelativeLayout.TRUE);
    }

    view.setLayoutParams(layoutParams);

    ShapeDrawable footerBackground = new ShapeDrawable();
    footerBackground.setShape(createRoundRect(fullscreen ? 0 : SizeUtil.dp20));
    footerBackground.getPaint().setColor(0x00000000);
    if (Build.VERSION.SDK_INT >= 16) {
      view.setBackground(footerBackground);
    } else {
      view.setBackgroundDrawable(footerBackground);
    }

    if (!isWeb && !isHtml) {
      ImageView image = createBackgroundImageView(context, fullscreen);
      view.addView(image, image.getLayoutParams());

      View title = createTitleView(context);
      title.setId(104);
      view.addView(title, title.getLayoutParams());

      View button = createAcceptButton(context);
      button.setId(105);
      view.addView(button, button.getLayoutParams());

      View message = createMessageView(context);
      ((RelativeLayout.LayoutParams) message.getLayoutParams())
              .addRule(RelativeLayout.BELOW, title.getId());
      ((RelativeLayout.LayoutParams) message.getLayoutParams())
              .addRule(RelativeLayout.ABOVE, button.getId());
      view.addView(message, message.getLayoutParams());
    } else if (isWeb) {
      WebView webView = createWebView(context);
      view.addView(webView, webView.getLayoutParams());
    } else {
      webView = createHtml(context);
      view.addView(webView, webView.getLayoutParams());
    }

    return view;
  }

  private Shape createRoundRect(int cornerRadius) {
    int c = cornerRadius;
    float[] outerRadii = new float[] {c, c, c, c, c, c, c, c};
    return new RoundRectShape(outerRadii, null, null);
  }

  // setBackgroundDrawable was deprecated at API 16.
  @SuppressWarnings("deprecation")
  private ImageView createBackgroundImageView(Context context, boolean fullscreen) {
    BackgroundImageView view = new BackgroundImageView(context, fullscreen);
    view.setScaleType(ImageView.ScaleType.CENTER_CROP);
    int cornerRadius;
    if (!fullscreen) {
      cornerRadius = SizeUtil.dp20;
    } else {
      cornerRadius = 0;
    }
    view.setImageBitmap(options.getBackgroundImage());
    ShapeDrawable footerBackground = new ShapeDrawable();
    footerBackground.setShape(createRoundRect(cornerRadius));
    footerBackground.getPaint().setColor(options.getBackgroundColor());
    if (Build.VERSION.SDK_INT >= 16) {
      view.setBackground(footerBackground);
    } else {
      view.setBackgroundDrawable(footerBackground);
    }
    RelativeLayout.LayoutParams layoutParams = new RelativeLayout.LayoutParams(
            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    view.setLayoutParams(layoutParams);
    return view;
  }

  private RelativeLayout createTitleView(Context context) {
    RelativeLayout view = new RelativeLayout(context);
    view.setLayoutParams(new LayoutParams(
            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

    TextView title = new TextView(context);
    title.setPadding(0, SizeUtil.dp5, 0, SizeUtil.dp5);
    title.setGravity(Gravity.CENTER);
    title.setText(options.getTitle());
    title.setTextColor(options.getTitleColor());
    title.setTextSize(TypedValue.COMPLEX_UNIT_SP, SizeUtil.textSize0);
    title.setTypeface(null, Typeface.BOLD);
    RelativeLayout.LayoutParams layoutParams = new RelativeLayout.LayoutParams(
            LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
    layoutParams.addRule(RelativeLayout.CENTER_HORIZONTAL, RelativeLayout.TRUE);
    layoutParams.addRule(RelativeLayout.CENTER_VERTICAL, RelativeLayout.TRUE);
    title.setLayoutParams(layoutParams);

    view.addView(title, title.getLayoutParams());
    return view;
  }

  private TextView createMessageView(Context context) {
    TextView view = new TextView(context);
    RelativeLayout.LayoutParams layoutParams = new RelativeLayout.LayoutParams(
            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    view.setLayoutParams(layoutParams);
    view.setGravity(Gravity.CENTER);
    view.setText(options.getMessageText());
    view.setTextColor(options.getMessageColor());
    view.setTextSize(TypedValue.COMPLEX_UNIT_SP, SizeUtil.textSize0_1);
    return view;
  }

  private WebView createWebView(Context context) {
    WebView view = new WebView(context);
    RelativeLayout.LayoutParams layoutParams = new RelativeLayout.LayoutParams(
            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    view.setLayoutParams(layoutParams);
    view.setWebViewClient(new WebViewClient() {
      @SuppressWarnings("deprecation")
      @Override
      public boolean shouldOverrideUrlLoading(WebView wView, String url) {
        if (url.contains(webOptions.getCloseUrl())) {
          cancel();
          String[] urlComponents = url.split("\\?");
          if (urlComponents.length > 1) {
            String queryString = urlComponents[1];
            String[] parameters = queryString.split("&");
            for (String parameter : parameters) {
              String[] parameterComponents = parameter.split("=");
              if (parameterComponents.length > 1 && parameterComponents[0].equals("result")) {
                Leanplum.track(parameterComponents[1]);
              }
            }
          }
          return true;
        }
        return false;
      }
    });
    view.loadUrl(webOptions.getUrl());
    return view;
  }

  /**
   * Create WebView with HTML template.
   *
   * @param context Current context.
   * @return WebVew WebVew with HTML template.
   */
  @SuppressLint("SetJavaScriptEnabled")
  private WebView createHtml(Context context) {
    dialogView.setVisibility(View.GONE);
    final WebView webView = new WebView(context);
    webView.setBackgroundColor(Color.TRANSPARENT);
    webView.setVerticalScrollBarEnabled(false);
    webView.setHorizontalScrollBarEnabled(false);
    webView.setOnTouchListener(new View.OnTouchListener() {
      public boolean onTouch(View v, MotionEvent event) {
        return (event.getAction() == MotionEvent.ACTION_MOVE);
      }
    });
    webView.canGoBack();
    // Disable long click.
    webView.setLongClickable(false);
    webView.setHapticFeedbackEnabled(false);
    webView.setOnLongClickListener(new View.OnLongClickListener() {
      @Override
      public boolean onLongClick(View v) {
        return true;
      }
    });

    WebSettings webViewSettings = webView.getSettings();
    if (Build.VERSION.SDK_INT >= 17) {
      webViewSettings.setMediaPlaybackRequiresUserGesture(false);
    }
    webViewSettings.setAppCacheEnabled(true);
    webViewSettings.getSaveFormData();
    webViewSettings.setAllowFileAccess(true);
    webViewSettings.setJavaScriptEnabled(true);
    webViewSettings.setDomStorageEnabled(true);
    webViewSettings.setJavaScriptCanOpenWindowsAutomatically(true);
    webViewSettings.setLoadWithOverviewMode(true);
    webViewSettings.setLoadsImagesAutomatically(true);

    if (Build.VERSION.SDK_INT >= 16) {
      webViewSettings.setAllowFileAccessFromFileURLs(true);
      webViewSettings.setAllowUniversalAccessFromFileURLs(true);
    }

    webViewSettings.setBuiltInZoomControls(false);
    webViewSettings.setDisplayZoomControls(false);
    webViewSettings.setSupportZoom(false);

    RelativeLayout.LayoutParams layoutParams = new RelativeLayout.LayoutParams(
            LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
    webView.setLayoutParams(layoutParams);
    final Dialog currentDialog = this;
    webView.setWebChromeClient(new WebChromeClient());
    webView.setWebViewClient(new WebViewClient() {
      // shouldOverrideUrlLoading(WebView wView, String url) was deprecated at API 24.
      @SuppressWarnings("deprecation")
      @Override
      public boolean shouldOverrideUrlLoading(WebView wView, String url) {
        // Open URL event.
        if (url.contains(htmlOptions.getOpenUrl())) {
          dialogView.setVisibility(View.VISIBLE);
          if (activity != null && !activity.isFinishing()) {
            currentDialog.show();
          }
          return true;
        }

        // Close URL event.
        if (url.contains(htmlOptions.getCloseUrl())) {
          cancel();
          String queryComponentsFromUrl = queryComponentsFromUrl(url, "result");
          if (!TextUtils.isEmpty(queryComponentsFromUrl)) {
            Leanplum.track(queryComponentsFromUrl);
          }
          return true;
        }

        // Track URL event.
        if (url.contains(htmlOptions.getTrackUrl())) {
          String eventName = queryComponentsFromUrl(url, "event");
          if (!TextUtils.isEmpty(eventName)) {
            Double value = Double.parseDouble(queryComponentsFromUrl(url, "value"));
            String info = queryComponentsFromUrl(url, "info");
            Map<String, Object> paramsMap = null;

            try {
              paramsMap = ActionContext.mapFromJson(new JSONObject(queryComponentsFromUrl(url,
                      "parameters")));
            } catch (Exception ignored) {
            }

            if (queryComponentsFromUrl(url, "isMessageEvent").equals("true")) {
              ActionContext actionContext = htmlOptions.getActionContext();
              actionContext.trackMessageEvent(eventName, value, info, paramsMap);
            } else {
              Leanplum.track(eventName, value, info, paramsMap);
            }
          }
          return true;
        }

        // Action URL or track action URL event.
        if (url.contains(htmlOptions.getActionUrl()) ||
                url.contains(htmlOptions.getTrackActionUrl())) {
          cancel();
          String queryComponentsFromUrl = queryComponentsFromUrl(url, "action");
          try {
            queryComponentsFromUrl = URLDecoder.decode(queryComponentsFromUrl, "UTF-8");
          } catch (UnsupportedEncodingException ignored) {
          }

          ActionContext actionContext = htmlOptions.getActionContext();
          if (!TextUtils.isEmpty(queryComponentsFromUrl) && actionContext != null) {
            if (url.contains(htmlOptions.getActionUrl())) {
              actionContext.runActionNamed(queryComponentsFromUrl);
            } else {
              actionContext.runTrackedActionNamed(queryComponentsFromUrl);
            }
          }
          return true;
        }

        return false;
      }
    });
    String html = htmlOptions.getHtmlTemplate();

    webView.loadDataWithBaseURL(null, html, "text/html", "UTF-8", null);

    return webView;
  }

  /**
   * Get query components from URL.
   *
   * @param url URL string.
   * @param components Name of components.
   * @return String String with query components.
   */
  private String queryComponentsFromUrl(String url, String components) {
    String componentsFromUrl = "";
    String[] urlComponents = url.split("\\?");
    if (urlComponents.length > 1) {
      String queryString = urlComponents[1];
      String[] parameters = queryString.split("&");
      for (String parameter : parameters) {
        String[] parameterComponents = parameter.split("=");
        if (parameterComponents.length > 1 && parameterComponents[0].equals(components)) {
          componentsFromUrl = parameterComponents[1];
        }
      }
    }
    try {
      componentsFromUrl = URLDecoder.decode(componentsFromUrl, "UTF-8");
    } catch (Exception ignored) {
    }
    return componentsFromUrl;
  }

  private TextView createAcceptButton(Context context) {
    TextView view = new TextView(context);
    RelativeLayout.LayoutParams layoutParams = new RelativeLayout.LayoutParams(
            LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
    layoutParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM, RelativeLayout.TRUE);
    layoutParams.addRule(RelativeLayout.CENTER_HORIZONTAL, RelativeLayout.TRUE);
    layoutParams.setMargins(0, 0, 0, SizeUtil.dp5);

    view.setPadding(SizeUtil.dp20, SizeUtil.dp5, SizeUtil.dp20, SizeUtil.dp5);
    view.setLayoutParams(layoutParams);
    view.setText(options.getAcceptButtonText());
    view.setTextColor(options.getAcceptButtonTextColor());
    view.setTypeface(null, Typeface.BOLD);

    BitmapUtil.stateBackgroundDarkerByPercentage(view,
            options.getAcceptButtonBackgroundColor(), 30);

    view.setTextSize(TypedValue.COMPLEX_UNIT_SP, SizeUtil.textSize0_1);
    view.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View arg0) {
        if (!isClosing) {
          options.accept();
          cancel();
        }
      }
    });
    return view;
  }

  private static int getTheme(Activity activity) {
    boolean full = (activity.getWindow().getAttributes().flags &
            WindowManager.LayoutParams.FLAG_FULLSCREEN) == WindowManager.LayoutParams.FLAG_FULLSCREEN;
    if (full) {
      return android.R.style.Theme_Translucent_NoTitleBar_Fullscreen;
    } else {
      return android.R.style.Theme_Translucent_NoTitleBar;
    }
  }
}
