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

    var localSoftwareUpdateRegistry = 'resource:/res/xpinstall/SoftwareUpdates.rdf'; 


    function Init()
    {
        dump("Software Notification!");
        var tree = document.getElementById('tree');
        
        var mainRegistry;
        
        try 
        {
            // First try to construct a new one and load it
            // synchronously. nsIRDFService::GetDataSource() loads RDF/XML
            // asynchronously by default.

            mainRegistry = Components.classes['component://netscape/rdf/datasource?name=xml-datasource'].createInstance();
            mainRegistry = mainRegistry.QueryInterface(Components.interfaces.nsIRDFDataSource);

            var remote = mainRegistry.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
            remote.Init(localSoftwareUpdateRegistry); // this will throw if it's already been opened and registered.

            // read it in synchronously.
            remote.Refresh(true);
        }
        catch (ex) 
        {
            // if we get here, then the RDF/XML has been opened and read
            // once. We just need to grab the datasource.
            mainRegistry = RDF.GetDataSource(localSoftwareUpdateRegistry);
        }
        
        // Create a 'container' wrapper around the NC:SoftwareUpdateDataSources
        // resource so we can use some utility routines that make access a
        // bit easier.

        var mainContainer = Components.classes['component://netscape/rdf/container'].createInstance();
        mainContainer = mainContainer.QueryInterface(Components.interfaces.nsIRDFContainer);

        mainContainer.Init(mainRegistry, RDF.GetResource('NC:SoftwareUpdateDataSources'));

        
        // Now enumerate all of the softwareupdate datasources.
        var mainEnumerator = mainContainer.GetElements();
        while (mainEnumerator.HasMoreElements()) 
        {
            var aDistributor = mainEnumerator.GetNext();
            aDistributor = aDistributor.QueryInterface(Components.interfaces.nsIRDFResource);
            
            dump('Contacting Distributor: "' + aDistributor.Value + '"...\n');
            
            var distributorRegistry;

            try 
            {
                // First try to construct a new one and load it
                // synchronously. nsIRDFService::GetDataSource() loads RDF/XML
                // asynchronously by default.

                distributorRegistry = Components.classes['component://netscape/rdf/datasource?name=xml-datasource'].createInstance();
                distributorRegistry = distributorRegistry.QueryInterface(Components.interfaces.nsIRDFDataSource);

                var remote = distributorRegistry.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
                remote.Init(aDistributor.Value); // this will throw if it's already been opened and registered.

                // read it in synchronously.
                remote.Refresh(true);
            }
            catch (ex) 
            {
                // if we get here, then the RDF/XML has been opened and read
                // once. We just need to grab the datasource.
                distributorRegistry = RDF.GetDataSource(aDistributor.Value);
            }

            try
            {
                // Create a 'container' wrapper around the NC:SoftwareUpdateRoot
                // resource so we can use some utility routines that make access a
                // bit easier.
                
                var distributorContainer = Components.classes['component://netscape/rdf/container'].createInstance();
                distributorContainer = distributorContainer.QueryInterface(Components.interfaces.nsIRDFContainer);
                distributorContainer.Init(distributorRegistry, RDF.GetResource('NC:SoftwarePackages'));
                

                // Now enumerate all of the distributorContainer's packages.
            
                var distributorEnumerator = distributorContainer.GetElements();

                dump('Parsing "' + aDistributor.Value + '" \n');
                dump('Contatiner "' + distributorContainer + '" \n');
                dump('Enumerator "' + distributorEnumerator + '" \n');

                while (distributorEnumerator.HasMoreElements()) 
                {
                    // Create a new RDF/XML datasource. 
                    
                    var aPackage = distributorEnumerator.GetNext();
                    aPackage = aPackage.QueryInterface(Components.interfaces.nsIRDFResource);

                     // remove any that we do not want.
                     dump('package name "' + getAttr(distributorRegistry, aPackage, 'title') + '" \n');

                //     var InstallTrigger.CompareVersion(getAttr(distributorRegistry, aPackage, 'registryKey'),
                  //                                 getAttr(distributorRegistry, aPackage, 'version'));


                     if (getAttr(distributorRegistry, aPackage, 'title') == "AOL AIM")
                     {
                        distributorContainer.RemoveElement(aPackage, true);
                     }

                 }

                tree.database.AddDataSource(distributorRegistry);
               
            }
            catch (ex) 
            {
                dump('an exception occurred trying to load "' + aDistributor.Value + '":\n');
                dump(ex + '\n');
            }
        
            // TODO

            // XXX hack to force the tree to rebuild
            tree.setAttribute('ref', 'NC:SoftwareUpdateRoot');
        }
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

    function Reload(url, pollInterval)
    {
        // Reload the specified datasource and reschedule.
        dump('Reload(' + url + ', ' + pollInterval + ')\n');

        var datasource = RDF.GetDataSource(url);
        datasource = datasource.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);

        // Reload, asynchronously.
        datasource.Refresh(false);

        // Reschedule
        Schedule(url, pollInterval);
    }

    function Schedule(url, pollInterval)
    {
        setTimeout('Reload("' + url + '", ' + pollInterval + ')', pollInterval * 1000);
    }


    // To get around "window.onload" not working in viewer.
    function Boot()
    {
        var tree = document.getElementById('tree');
        
        if (tree == null) 
        {
            setTimeout(Boot, 0);
        }
        else 
        {
            Init();
        }
    }

    setTimeout('Boot()', 0);

