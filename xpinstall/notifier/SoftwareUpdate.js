    // the rdf service
    var RDF = Components.classes['component://netscape/rdf/rdf-service'].getService();
    RDF = RDF.QueryInterface(Components.interfaces.nsIRDFService);

    function getAttr(registry,service,attr_name) 
    {
        var attr = registry.GetTarget(service,
                                      RDF.GetResource('http://home.netscape.com/NC-rdf#' + attr_name),
                                      true);
        if (attr)
            attr = attr.QueryInterface(Components.interfaces.nsIRDFLiteral);
  
        if (attr)
            attr = attr.Value;
        
        return attr;
    }

    // the location of the flash registry.
    var localSoftwareUpdateRegistry = 'resource:/res/rdf/SoftwareUpdates.rdf'; 

    function Init()
    {
        // this is the main rdf file.

        var mainRegistry;
        try 
        {
            // First try to construct a new one and load it
            // synchronously. nsIRDFService::GetDataSource() loads RDF/XML
            // asynchronously by default.
            mainRegistry = Components.classes['component://netscape/rdf/datasource?name=xml-datasource'].createInstance();
            mainRegistry = mainRegistry.QueryInterface(Components.interfaces.nsIRDFXMLDataSource);
            mainRegistry.Init(localSoftwareUpdateRegistry); // this will throw if it's already been opened and registered.

            // read it in synchronously.
            mainRegistry.Open(true);
        }
        catch (ex) 
        {
            // if we get here, then the RDF/XML has been opened and read
            // once. We just need to grab the datasource.
            mainRegistry = RDF.GetDataSource(localSoftwareUpdateRegistry);
        }


        var mainContainer = Components.classes['component://netscape/rdf/container'].createInstance();
        mainContainer = mainContainer.QueryInterface(Components.interfaces.nsIRDFContainer);

        mainContainer.Init(mainRegistry, RDF.GetResource('NC:SoftwareUpdateDataSources'));

        // Now enumerate all of the softwareupdate datasources.
        var mainEnumerator = mainContainer.GetElements();
        while (mainEnumerator.HasMoreElements()) 
        {
            var aDistributor = mainEnumerator.GetNext();
            aDistributor = aDistributor.QueryInterface(Components.interfaces.nsIRDFResource);
            
            var distributorContainer = Components.classes['component://netscape/rdf/container'].createInstance();
            distributorContainer = distributorContainer.QueryInterface(Components.interfaces.nsIRDFContainer);
            
            var distributorRegistry = RDF.GetDataSource(aDistributor.Value);
            var distributorResource = RDF.GetResource('NC:SoftwareUpdateRoot');

            distributorContainer.Init(distributorRegistry, distributorResource);

            // Now enumerate all of the distributorContainer's packages.
            
            var distributorEnumerator = distributorContainer.GetElements();
            
            while (distributorEnumerator.HasMoreElements()) 
            {
                 var aPackage = distributorEnumerator.GetNext();
                 aPackage = aPackage.QueryInterface(Components.interfaces.nsIRDFResource);
                 
                 // remove any that we do not want.
            
                 if (getAttr(distributorRegistry, aPackage, 'title') == "AOL AIM")
                 {
                    //distributorContainer.RemoveElement(aPackage, true);
                 }
            }
            var tree = document.getElementById('tree');

            // Add it to the tree control's composite datasource.
            tree.database.AddDataSource(distributorRegistry);
                        
        }
        
        // Install all of the stylesheets in the softwareupdate Registry into the
        // panel.

        // TODO

        // XXX hack to force the tree to rebuild
        var treebody = document.getElementById('NC:SoftwareUpdateRoot');
        treebody.setAttribute('id', 'NC:SoftwareUpdateRoot');
    }


    function OpenURL(event, node)
    {
        if (node.getAttribute('type') == "http://home.netscape.com/NC-rdf#SoftwarePackage") 
        {
            url = node.getAttribute('url');
     
            /*window.open(url,'bookmarks');*/
  
            var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
            if (!toolkitCore) 
            {
                toolkitCore = new ToolkitCore();
                if (toolkitCore) 
                {
                    toolkitCore.Init("ToolkitCore");
                }
            }
        
            if (toolkitCore) 
            {
                toolkitCore.ShowWindow(url,window);
            }

            dump("OpenURL(" + url + ")\n");
        
            return true;        
        }
        return false;
    }


    // To get around "window.onload" not working in viewer.
    function Boot()
    {
        var tree = document.getElementById('tree');
        if (tree == null) {
            setTimeout(Boot, 0);
        }
        else {
            Init();
        }
    }

    setTimeout('Boot()', 0);

