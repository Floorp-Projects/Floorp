.. _cloudsync_example:

=======
Example
=======

.. code-block:: javascript

	Cu.import("resource://gre/modules/CloudSync.jsm");

	let HelloWorld = {
	  onLoad: function() {
	    let cloudSync = CloudSync();
	    console.log("CLOUDSYNC -- hello world", cloudSync.local.id, cloudSync.local.name, cloudSync.adapters);
	    cloudSync.adapters.register('helloworld', {});
	    console.log("CLOUDSYNC -- " + JSON.stringify(cloudSync.adapters.getAdapterNames()));


	    cloudSync.tabs.addEventListener("change", function() {
	      console.log("tab change");
	      cloudSync.tabs.getLocalTabs().then(
	        function(records) {
	          console.log(JSON.stringify(records));
	        }
	      );
	    });

	    cloudSync.tabs.getLocalTabs().then(
	      function(records) {
	        console.log(JSON.stringify(records));
	      }
	    );

	    let remoteClient = {
	      id: "001",
	      name: "FakeClient",
	    };
	    let remoteTabs1 = [
	      {url:"https://www.google.ca",title:"Google",icon:"https://www.google.ca/favicon.ico",lastUsed:Date.now()},
	    ];
	    let remoteTabs2 = [
	      {url:"https://www.google.ca",title:"Google Canada",icon:"https://www.google.ca/favicon.ico",lastUsed:Date.now()},
	      {url:"http://www.reddit.com",title:"Reddit",icon:"http://www.reddit.com/favicon.ico",lastUsed:Date.now()},
	    ];
	    cloudSync.tabs.mergeRemoteTabs(remoteClient, remoteTabs1).then(
	      function() {
	        return cloudSync.tabs.mergeRemoteTabs(remoteClient, remoteTabs2);
	      }
	    ).then(
	      function() {
	        return cloudSync.tabs.getRemoteTabs();
	      }
	    ).then(
	      function(tabs) {
	        console.log("remote tabs:", tabs);
	      }
	    );

	    cloudSync.bookmarks.getRootFolder("Hello World").then(
	      function(rootFolder) {
	        console.log(rootFolder.name, rootFolder.id);
	        rootFolder.addEventListener("add", function(guid) {
	          console.log("CLOUDSYNC -- bookmark item added: " + guid);
	          rootFolder.getLocalItemsById([guid]).then(
	            function(items) {
	              console.log("CLOUDSYNC -- items: " + JSON.stringify(items));
	            }
	          );
	        });
	        rootFolder.addEventListener("remove", function(guid) {
	          console.log("CLOUDSYNC -- bookmark item removed: " + guid);
	          rootFolder.getLocalItemsById([guid]).then(
	            function(items) {
	              console.log("CLOUDSYNC -- items: " + JSON.stringify(items));
	            }
	          );
	        });
	        rootFolder.addEventListener("change", function(guid) {
	          console.log("CLOUDSYNC -- bookmark item changed: " + guid);
	          rootFolder.getLocalItemsById([guid]).then(
	            function(items) {
	              console.log("CLOUDSYNC -- items: " + JSON.stringify(items));
	            }
	          );
	        });
	        rootFolder.addEventListener("move", function(guid) {
	          console.log("CLOUDSYNC -- bookmark item moved: " + guid);
	          rootFolder.getLocalItemsById([guid]).then(
	            function(items) {
	              console.log("CLOUDSYNC -- items: " + JSON.stringify(items));
	            }
	          );
	        });

	        function logLocalItems() {
	          return rootFolder.getLocalItems().then(
	            function(items) {
	              console.log("CLOUDSYNC -- local items: " + JSON.stringify(items));
	            }
	          );
	        }

	        let items = [
	            {"id":"9fdoci2KOME6","type":rootFolder.FOLDER,"parent":rootFolder.id,"title":"My Bookmarks 1"},
	            {"id":"1fdoci2KOME5","type":rootFolder.FOLDER,"parent":rootFolder.id,"title":"My Bookmarks 2"},
	            {"id":"G_UL4ZhOyX8m","type":rootFolder.BOOKMARK,"parent":"1fdoci2KOME5","title":"reddit: the front page of the internet","uri":"http://www.reddit.com/"},
	          ];
	        function mergeSomeItems() {
	          return rootFolder.mergeRemoteItems(items);
	        }

	        logLocalItems().then(
	          mergeSomeItems
	        ).then(
	          function(processedItems) {
	            console.log("!!!", processedItems);
	            console.log("merge complete");
	          },
	          function(error) {
	            console.log("merge failed:", error);
	          }
	        ).then(
	          logLocalItems
	        );
	      }
	    );


	  },
	};

	window.addEventListener("load", function(e) { HelloWorld.onLoad(e); }, false);
