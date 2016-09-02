package org.mozilla.gecko.icons.preparation;

import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.util.StringUtils;

import java.util.Iterator;

/**
 * Filter non http/https URLs if the request is not from privileged code.
 */
public class FilterPrivilegedUrls implements Preparer {
    @Override
    public void prepare(IconRequest request) {
        if (request.isPrivileged()) {
            // This request is privileged. No need to filter anything.
            return;
        }

        final Iterator<IconDescriptor> iterator = request.getIconIterator();

        while (iterator.hasNext()) {
            IconDescriptor descriptor = iterator.next();

            if (!StringUtils.isHttpOrHttps(descriptor.getUrl())) {
                iterator.remove();
            }
        }
    }
}
