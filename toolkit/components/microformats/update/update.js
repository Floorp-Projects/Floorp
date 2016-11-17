/* !
	update.js

	run $ npm install
	run $ node unpdate.js

	Downloads latest version of microformat-shiv and it tests form github repo
	Files downloaded:
	* microformat-shiv.js (note: modern version)
	* lib
	* test/interface-tests
	* test/module-tests
	* test/standards-tests
	* test/static

	Copyright (C) 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
	*/

// configuration
var deployDir = '../'
	exportedSymbol = 'try {\n    // mozilla jsm support\n    Components.utils.importGlobalProperties(["URL"]);\n} catch(e) {}\nthis.EXPORTED_SYMBOLS = [\'Microformats\'];';



var path			= require('path'),
	request 		= require('request'),
	fs 				= require('fs-extra'),
	download 		= require('download-github-repo');


var repo = 'glennjones/microformat-shiv',
	tempDir = path.resolve(__dirname, 'temp-repo'),
	deployDirResolved = path.resolve(__dirname, deployDir),
	pathList = [
		['/modern/microformat-shiv-modern.js', '/microformat-shiv.js'],
		['/lib', '/test/lib'],
		['/test/interface-tests', '/test/interface-tests'],
		['/test/module-tests', '/test/module-tests'],
		['/test/standards-tests', '/test/standards-tests'],
		['/test/static', '/test/static']
		];



getLastBuildState( repo, function( err, buildState) {
	if (buildState) {
		console.log('last build state:', buildState);

		if (buildState === 'passed') {

			console.log('downloading git repo', repo);
			getLastCommitDate( repo, function( err, date) {
				if (date) {
					console.log( 'last commit:', new Date(date).toString() );
				}
			});
			updateFromRepo();

		} else {
			console.log('not updating because of build state is failing please contact Glenn Jones glennjones@gmail.com');
		}

	} else {
		console.log('could not get build state from travis-ci:', err);
	}
});


/**
 * updates from directories and files from repo
 *
 */
function updateFromRepo() {
	download(repo, tempDir, function(err, data) {

		// the err and data from download-github-repo give false negatives
		if ( fs.existsSync( tempDir ) ) {

			var version = getRepoVersion();
			removeCurrentFiles( pathList, deployDirResolved );
			addNewFiles( pathList, deployDirResolved );
			fs.removeSync(tempDir);

			// changes files for firefox
			replaceInFile('/test/module-tests/index.html', /..\/..\/lib\//g, '../lib/' );
			addExportedSymbol( '/microformat-shiv.js' );

			console.log('microformat-shiv is now uptodate to v' + version);

		} else {
			console.log('error getting repo', err);
		}

	});
}


/**
 * removes old version of delpoyed directories and files
 *
 * @param  {Array} pathList
 * @param  {String} deployDirResolved
 */
function removeCurrentFiles( pathList, deployDirResolved ) {
	pathList.forEach( function( path ) {
		console.log('removed:', deployDirResolved + path[1]);
		fs.removeSync(deployDirResolved + path[1]);
	});
}


/**
 * copies over required directories and files into deployed path
 *
 * @param  {Array} pathList
 * @param  {String} deployDirResolved
 */
function addNewFiles( pathList, deployDirResolved ) {
	pathList.forEach( function( path ) {
		console.log('added:', deployDirResolved + path[1]);
		fs.copySync(tempDir + path[0], deployDirResolved + path[1]);
	});

}


/**
 * gets the repo version number
 *
 * @return {String}
 */
function getRepoVersion() {
	var pack = fs.readFileSync(path.resolve(tempDir, 'package.json'), {encoding: 'utf8'});
	if (pack) {
		pack = JSON.parse(pack)
		if (pack && pack.version) {
			return pack.version;
		}
	}
	return '';
}


/**
 * get the last commit date from github repo
 *
 * @param  {String} repo
 * @param  {Function} callback
 */
function getLastCommitDate( repo, callback ) {

	var options = {
	  url: 'https://api.github.com/repos/' + repo + '/commits?per_page=1',
	  headers: {
	    'User-Agent': 'request'
	  }
	};

	request(options, function(error, response, body) {
	  if (!error && response.statusCode == 200) {
		var date = null,
			json = JSON.parse(body);
			if (json && json.length && json[0].commit && json[0].commit.author ) {
				date = json[0].commit.author.date;
			}
	    callback(null, date);
	  } else {
		  console.log(error, response, body);
		  callback('fail to get last commit date', null);
	  }
	});
}


/**
 * get the last build state from travis-ci
 *
 * @param  {String} repo
 * @param  {Function} callback
 */
function getLastBuildState( repo, callback ) {

	var options = {
	  url: 'https://api.travis-ci.org/repos/' + repo,
	  headers: {
	    'User-Agent': 'request',
		'Accept': 'application/vnd.travis-ci.2+json'
	  }
	};

	request(options, function(error, response, body) {
	  if (!error && response.statusCode == 200) {
		var buildState = null,
			json = JSON.parse(body);
			if (json && json.repo &&  json.repo.last_build_state ) {
				buildState = json.repo.last_build_state;
			}
	    callback(null, buildState);
	  } else {
		  console.log(error, response, body);
		  callback('fail to get last build state', null);
	  }
	});
}


/**
 * adds exported symbol to microformat-shiv.js file
 *
 * @param  {String} path
 * @param  {String} content
 */
function addExportedSymbol( path ) {
	if (path === '/microformat-shiv.js') {
		fs.appendFileSync(deployDirResolved + '/microformat-shiv.js', '\r\n' + exportedSymbol + '\r\n');
		console.log('appended exported symbol to microformat-shiv.js');
	}
}


/**
 * adds exported symbol to microformat-shiv.js file
 *
 * @param  {String} path
 * @param  {String} content
 */
function replaceInFile( path, findStr, replaceStr ) {
	readFile(deployDirResolved + path, function(err, fileStr) {
		if (fileStr) {
			fileStr = fileStr.replace(findStr, replaceStr)
			writeFile(deployDirResolved + path, fileStr);
			console.log('replaced ' + findStr + ' with ' + replaceStr + ' in ' + path);
		} else {
			console.log('error replaced strings in ' + path);
		}
	})
}


/**
 * write a file
 *
 * @param  {String} path
 * @param  {String} content
 */
function writeFile(path, content) {
	fs.writeFile(path, content, 'utf8', function(err) {
		if (err) {
			console.log(err);
		} else {
			console.log('The file: ' + path + ' was saved');
		}
	});
}


/**
 * read a file
 *
 * @param  {String} path
 * @param  {Function} callback
 */
function readFile(path, callback) {
	fs.readFile(path, 'utf8', callback);
}
