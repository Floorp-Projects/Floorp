package org.mozilla.gecko.icons.processing;

import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.storage.DiskStorage;
import org.mozilla.gecko.util.StringUtils;

public class DiskProcessor implements Processor {
    @Override
    public void process(IconRequest request, IconResponse response) {
        if (request.shouldSkipDisk() || request.isPrivateMode()) {
            return;
        }

        if (!response.hasUrl() || !StringUtils.isHttpOrHttps(response.getUrl())) {
            // If the response does not contain an URL from which the icon was loaded or if this is
            // not a http(s) URL then we cannot store this or do not need to (because it's already
            // stored somewhere else, like for URLs pointing inside the omni.ja).
            return;
        }

        final DiskStorage storage = DiskStorage.get(request.getContext());

        if (response.isFromNetwork()) {
            // The icon has been loaded from the network. Store it on the disk now.
            storage.putIcon(response);
        }

        if (response.isFromMemory() || response.isFromDisk() || response.isFromNetwork()) {
            // Remember mapping between page URL and storage URL. Even when this icon has been loaded
            // from memory or disk this does not mean that we stored this mapping already: We could
            // have loaded this icon for a different page URL previously.
            storage.putMapping(request, response.getUrl());
        }
    }
}
