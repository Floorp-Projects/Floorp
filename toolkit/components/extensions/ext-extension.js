extensions.registerAPI((extension, context) => {
  return {
    extension: {
      getURL: function(url) {
        return extension.baseURI.resolve(url);
      },

      getViews: function(fetchProperties) {
        let result = Cu.cloneInto([], context.cloneScope);

        for (let view of extension.views) {
          if (fetchProperties && "type" in fetchProperties) {
            if (view.type != fetchProperties.type) {
              continue;
            }
          }

          if (fetchProperties && "windowId" in fetchProperties) {
            if (view.windowId != fetchProperties.windowId) {
              continue;
            }
          }

          result.push(view.contentWindow);
        }

        return result;
      },
    },
  };
});

