package org.mozilla.gecko.util;

/**
 * Callback interface for Gecko requests.
 *
 * For each instance of EventCallback, exactly one of sendResponse, sendError, or sendCancel
 * must be called to prevent observer leaks. If more than one send* method is called, or if a
 * single send method is called multiple times, an {@link IllegalStateException} will be thrown.
 */
public interface EventCallback {
    /**
     * Sends a success response with the given data.
     *
     * @param response The response data to send to Gecko. Can be any of the types accepted by
     *                 JSONObject#put(String, Object).
     */
    public void sendSuccess(Object response);

    /**
     * Sends an error response with the given data.
     *
     * @param response The response data to send to Gecko. Can be any of the types accepted by
     *                 JSONObject#put(String, Object).
     */
    public void sendError(Object response);

    /**
     * Cancels the request, preventing any Gecko-side callbacks from being executed.
     */
    public void sendCancel();
}
