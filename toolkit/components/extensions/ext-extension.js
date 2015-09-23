extensions.registerAPI((extension, context) => {
  return {
    extension: {
      getURL: function(url) {
        return extension.baseURI.resolve(url);
      },
    },
  };
});

