/*
 * Copyright 2015-2016 Bounce Storage, Inc. <info@bouncestorage.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.geckoview.test.util;

import android.content.Context;
import android.content.res.AssetManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.math.BigInteger;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Enumeration;
import java.util.Random;

import javax.servlet.http.Cookie;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.eclipse.jetty.server.Request;
import org.eclipse.jetty.server.handler.AbstractHandler;

import org.json.JSONException;
import org.json.JSONObject;

class HttpBinHandler extends AbstractHandler {
    private static final String LOGTAG = "HttpBinHandler";
    private static final int BUFSIZE = 4096;

    private AssetManager mAssets;

    public HttpBinHandler(@NonNull Context context) {
        super();

        mAssets = context.getResources().getAssets();
    }

    private static void pipe(final @NonNull InputStream is) throws IOException {
        pipe(is, null);
    }

    private static void pipe(final @NonNull InputStream is, final @Nullable OutputStream os)
        throws IOException {
        final byte[] buf = new byte[BUFSIZE];
        int count = 0;
        while ((count = is.read(buf)) > 0) {
            if (os != null) {
                os.write(buf, 0, count);
            }
        }
    }

    private void respondJSON(HttpServletResponse response, OutputStream os, JSONObject obj)
            throws IOException {
        final byte[] body = obj.toString().getBytes();

        response.setContentLength(body.length);
        response.setContentType("application/json");
        response.setStatus(HttpServletResponse.SC_OK);
        os.write(body);
        os.flush();
    }

    private void redirectTo(HttpServletResponse response, String location) {
        response.setHeader("Location", location);
        response.setStatus(HttpServletResponse.SC_MOVED_TEMPORARILY);
    }

    @Override
    @SuppressWarnings("unchecked")
    public void handle(String target, Request baseRequest,
                       HttpServletRequest request, HttpServletResponse servletResponse)
            throws IOException {
        String method = request.getMethod();
        String uri = request.getRequestURI();
        try (InputStream is = request.getInputStream();
             OutputStream os = servletResponse.getOutputStream()) {
            if (method.equals("GET") && uri.startsWith("/status/")) {
                pipe(is);
                int status = Integer.parseInt(uri.substring(
                        "/status/".length()));
                servletResponse.setStatus(status);
                baseRequest.setHandled(true);
            } else if (uri.equals("/redirect-to")) {
                pipe(is);
                redirectTo(servletResponse, request.getParameter("url"));
                baseRequest.setHandled(true);
            } else if (uri.startsWith("/redirect/")) {
                pipe(is);

                int count = Integer.parseInt(uri.substring("/redirect/".length())) - 1;
                if (count > 0) {
                    redirectTo(servletResponse, "/redirect/" + count);
                } else {
                    servletResponse.setStatus(HttpServletResponse.SC_OK);
                }

                baseRequest.setHandled(true);
            } else if (uri.equals("/cookies")) {
                pipe(is);

                final JSONObject cookies = new JSONObject();

                if (request.getCookies() != null) {
                    for (final Cookie cookie : request.getCookies()) {
                        cookies.put(cookie.getName(), cookie.getValue());
                    }
                }

                final JSONObject response = new JSONObject();
                response.put("cookies", cookies);

                respondJSON(servletResponse, os, response);
                baseRequest.setHandled(true);
            } else if (uri.startsWith("/cookies/set/")) {
                pipe(is);

                final String[] parts = uri.substring("/cookies/set/".length()).split("/");

                servletResponse.addHeader("Set-Cookie",
                        String.format("%s=%s; Path=/", parts[0], parts[1]));

                servletResponse.setHeader("Location", "/cookies");
                servletResponse.setStatus(HttpServletResponse.SC_MOVED_TEMPORARILY);
                baseRequest.setHandled(true);
            } else if (uri.startsWith("/basic-auth")) {
                pipe(is);

                // FIXME: we don't actually check the username/password here
                servletResponse.addHeader("WWW-Authenticate",
                        "Basic realm=\"Fake Realm\"");
                servletResponse.setStatus(HttpServletResponse.SC_UNAUTHORIZED);
                baseRequest.setHandled(true);
            } else if (uri.equals("/anything")) {
                servletResponse.setStatus(HttpServletResponse.SC_OK);
                baseRequest.setHandled(true);

                final JSONObject response = new JSONObject();

                // Method
                response.put("method", method);

                // Headers
                final JSONObject headers = new JSONObject();
                response.put("headers", headers);

                for (Enumeration<String> names = request.getHeaderNames(); names.hasMoreElements();) {
                    final String name = names.nextElement();
                    headers.put(name, request.getHeader(name));
                }

                // Body data
                final ByteArrayOutputStream data = new ByteArrayOutputStream();
                pipe(is, data);

                response.put("data", data.toString("UTF-8"));
                respondJSON(servletResponse, os, response);
                baseRequest.setHandled(true);
            } else if (uri.startsWith("/bytes")) {
                pipe(is);

                final int count = Integer.parseInt(uri.substring("/bytes/".length()));

                final Random random = new Random(System.currentTimeMillis());
                final byte[] payload = new byte[count];
                random.nextBytes(payload);

                servletResponse.setStatus(HttpServletResponse.SC_OK);
                servletResponse.setContentLength(count);
                servletResponse.setContentType("application/octet-stream");

                final byte[] digest = MessageDigest.getInstance("SHA-256").digest(payload);
                servletResponse.addHeader("X-SHA-256",
                        String.format("%064x", new BigInteger(1, digest)));

                os.write(payload);
                os.flush();

                baseRequest.setHandled(true);
            } else if (uri.startsWith("/assets")) {
                pipe(is);
                pipe(mAssets.open(uri.substring("/assets/".length())), os);
                os.flush();
                baseRequest.setHandled(true);
            }

            if (!baseRequest.isHandled()) {
                servletResponse.setStatus(501);
                baseRequest.setHandled(true);
            }
        } catch (JSONException e) {
            Log.e(LOGTAG, "JSON error while handling response", e);
            servletResponse.setStatus(500);
            baseRequest.setHandled(true);
        } catch (NoSuchAlgorithmException e) {
            Log.e(LOGTAG, "Failed to generate digest", e);
            servletResponse.setStatus(500);
            baseRequest.setHandled(true);
        } catch (IOException e) {
            Log.e(LOGTAG, "Failed to respond", e);
            servletResponse.setStatus(500);
            baseRequest.setHandled(true);
        }
    }
}
