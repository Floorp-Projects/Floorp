/*!
	update.js
	
	This node.js script downloads latest version of microformat-shiv and it tests form the authors github repo.
	
	Make sure your have an uptodate copy of node.js on your machine then using a command line navigate the 
	directory containing the update.js and run the following commands: 
	
	$ npm install
	$ node unpdate.js
	
	The script will 
	
	1. Checks the current build status of the project.
	2. Checks the date of the last commit
	3. Downloads and updates the following directories and files:
		* microformat-shiv.js
		* test/lib
		* test/interface-tests
		* test/module-tests
		* test/standards-tests
		* test/static
	4. Adds the EXPORTED_SYMBOLS to the bottom of microformat-shiv.js
	5. Repath the links in test/module-tests/index.html file
	
	
	This will update the microformats parser and all the related tests.
	
		
	
	Copyright (C) 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
	*/