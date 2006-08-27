<?php
require_once('Archive/Zip.php');

class AddonsController extends AppController
{
    var $name = 'Addons';
    var $uses = array('Addon', 'Platform', 'Application', 'Appversion', 'Tag');
    var $components = array('Amo', 'Rdf', 'Versioncompare');
    var $scaffold;
   /**
    *
    *
    function index() {
        $this->set('addons', $this->Amo->addonTypes);
    }*/

   /**
    * Add new or new version of an add-on
    * @param int $id
    */
    function add($id = '') {
        $this->layout = 'developers';
        $this->set('addonTypes', $this->Amo->addonTypes);

        //Determine if an adding a new addon or new version
        if ($id != '' && $this->Amo->checkOwnership($id)) {
            $this->Addon->id = $id;
            $existing = $this->Addon->read();
            $this->set('existing', $existing);
            $newAddon = false;
        }
        else {
            $newAddon = true;
        }
        $this->set('id', $id);
        $this->set('newAddon', $newAddon);

        //Step 2: Parse uploaded file and if correct, display addon-wide information form
        if (isset($this->data['Addon']['add_step1'])) {
            //Check for model validation first (in step 1, this is just addon type)
            if (!$this->Addon->validates($this->data)) {
                $this->validateErrors();
                $this->set('fileError', '');
                $this->render('add_step1');
                die();
            }

            //Check for file upload errors
            if ($this->data['Addon']['file']['error'] !== 0) {
                $this->Addon->invalidate('file');
                $fileErrors = array('1' => _('Exceeds maximum upload size'),
                                    '2' => _('Exceeds maximum upload size'),
                                    '3' => _('Incomplete transfer'),
                                    '4' => _('No file uploaded')
                              );
                $fileError = $fileErrors[$this->data['Addon']['file']['error']];
                $this->set('fileError', $fileError);
                $this->render('add_step1');
                die();
            }

            $fileName = $this->data['Addon']['file']['name'];
            $fileSize = round($this->data['Addon']['file']['size']/1024, 1); //in KB
            $fileExtension = substr($fileName, strrpos($fileName, '.'));
            $allowedExtensions = array('.xpi', '.jar', '.src');
            //Check for file extenion match
            if (!in_array($fileExtension, $allowedExtensions)) {
                $this->Addon->invalidate('file');
                $this->set('fileError', sprintf(_('Disallowed file extension (%s)'), $fileExtension));
                $this->render('add_step1');
                die();
            }

            //Move temporary file to repository
            $uploadedFile = $this->data['Addon']['file']['tmp_name'];
            $tempLocation = REPO_PATH.'/temp/'.$fileName;
            if (move_uploaded_file($uploadedFile, $tempLocation)) {
                $uploadedFile = $tempLocation;
                chmod($uploadedFile, 0644);
            }
            else {
                $this->Addon->invalidate('file');
                $this->set('fileError', _('Could not move file'));
                $this->render('add_step1');
                die();
            }

            //Search plugins do not have install.rdf to parse
            if ($this->Amo->addonTypes[$this->data['Addon']['addontype_id']] != 'Search Plugin') {

                //Extract install.rdf from the package
                $zip = new Archive_Zip($uploadedFile);
                $extraction = $zip->extract(array('extract_as_string' => true, 'by_name' => array('install.rdf')));

                //Make sure install.rdf is present
                if ($extraction !== 0) {
                    $fileContents = $extraction[0]['content'];
                }
                else {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', _('No install.rdf present'));
                    $this->render('add_step1');
                    die();
                }

                //Use Rdf Component to parse install.rdf
                $manifestData = $this->Rdf->parseInstallManifest($fileContents);

                //If the result is a string, it is an error message
                if (!is_array($manifestData)) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', sprintf(_('The following error occurred while parsing install.rdf: %s'), $manifestData));
                    $this->render('add_step1');
                    die();
                }

                //Check if install.rdf has an updateURL
                if (isset($manifestData['updateURL'])) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', _('Add-ons cannot use an external updateURL. Please remove this from install.rdf and try again.'));
                    $this->render('add_step1');
                    die();
                }

                //Check the GUID
                if (!isset($manifestData['id']) || !preg_match('/^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i', $manifestData['id'])) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', _('The ID of this add-on is invalid or not set.'));
                    $this->render('add_step1');
                    die();
                }

                //Make sure version has no spaces
                if (!isset($manifestData['version']) || preg_match('/.*\s.*/', $manifestData['version'])) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', _('The version of this add-on is invalid or not set. Versions cannot contain spaces.'));
                    $this->render('add_step1');
                    die();
                }

                //These are arrays by locale
                $addonNames = $manifestData['name'];
                $addonDesc = $manifestData['description'];

                //In case user said it was a new add-on when it is actually an update
                if ($existing = $this->Addon->findAllByGuid($manifestData['id'])) {
                    if ($newAddon === true) {  
                        $newAddon = false;
                        $this->set('newAddon', $newAddon);
                    }
                    $existing = $existing[0];
                }
                else {
                    if ($newAddon === false) {
                        $newAddon = true;
                        $this->set('newAddon', $newAddon);
                    }
                }

                //Initialize targetApp checking
                $noMozApps = true;
                $versionErrors = array();

                if (count($manifestData['targetApplication']) > 0) {
                    //Iterate through each target app and find it in the DB
                    foreach ($manifestData['targetApplication'] as $appKey => $appVal) {
                        if ($matchingApp = $this->Application->find(array('guid' => $appKey), null, null, -1)) {
                            $noMozApps = false;

                            //Check if the minVersion is valid
                            if (!$matchingMinVers = $this->Appversion->find(array(
                                                          'application_id' => $matchingApp['Application']['id'],
                                                          'version' => $appVal['minVersion'],
                                                          'public' => 1
                                                      ), null, null, -1)) {
                                $versionErrors[] = sprintf(_('%s is not a valid version for %s'), $appVal['minVersion'], $matchingApp['Application']['name']);
                            }

                            //Check if the maxVersion is valid
                            if (!$matchingMaxVers = $this->Appversion->find(array(
                                                          'application_id' => $matchingApp['Application']['id'],
                                                          'version' => $appVal['maxVersion'],
                                                          'public' => 1
                                                      ), null, null, -1)) {
                                $versionErrors[] = sprintf(_('%s is not a valid version for %s'), $appVal['maxVersion'], $matchingApp['Application']['name']);
                            }
                        }
                    }
                }

                //Must have at least one mozilla app
                if ($noMozApps === true) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', _('You must have at least one valid Mozilla Target Application.'));
                    $this->render('add_step1');
                    die();
                }

                //Max/min version errors
                if (count($versionErrors) > 0) {
                    $this->Addon->invalidate('file');
                    $errorStr = implode($versionErrors, '<br />');
                    $this->set('fileError', _('The following errors were found in install.rdf:').'<br />'.$errorStr);
                    $this->render('add_step1');
                    die();
                }
            }
            //If it is a search plugin, read the .src file
            else {

            }

            //Get tags based on addontype
            $tagsQry = $this->Tag->findAll(array('addontype_id' => $this->data['Addon']['addontype_id']),
                                                     null, null, null, null, -1);
            foreach ($tagsQry as $k => $v) {
                $tags[$v['Tag']['id']] = $v['Tag']['name'];
            }

            $info['name'] = (!empty($existing['Addon']['name'])) ? $existing['Addon']['name'] : $manifestData['name']['en-US'];
            $info['description'] = (!empty($existing['Addon']['description'])) ? $existing['Addon']['description'] : $manifestData['description']['en-US'];
            $info['homepage'] = (!empty($existing['Addon']['homepage'])) ? $existing['Addon']['homepage'] : $manifestData['homepageURL'];
            $info['version'] = $manifestData['version'];
            $info['summary'] = $existing['Addon']['summary'];

            if (count($existing['Tag']) > 0) {
                foreach ($existing['Tag'] as $tag) {
                    $info['selectedTags'][$tag['id']] = $tag['name'];
                }
            }
            else {
                $info['selectedTags'] = array();
            }

            $this->set('tags', $tags);
            $this->set('info', $info);
            $this->set('fileName', $fileName);
            $this->set('fileSize', $fileSize);
            $this->set('manifestData', $manifestData);
            $this->render('add_step2');
        }
        elseif (isset($this->data['Addon']['add_step2'])) {
            
            $this->render('add_step25');
        }
        elseif (isset($this->data['Addon']['add_step25'])) {
            //Get Platforms list
            $platformQry = $this->Platform->findAll();
            foreach ($platformQry as $k => $v) {
                $platforms[$v['Platform']['id']] = $v['Platform']['name'];
            }

            $this->set('platforms', $platforms);
            $this->render('add_step3');
        }
        elseif (isset($this->data['Addon']['add_step3'])) {
            $this->render('add_step4');
        }
        //Step 1: Add-on type and file upload
        else {
            $this->set('fileError', '');
            $this->render('add_step1');
        }
    }

   /**
    * Manage an add-on
    * @param int $id
    */
    function manage($id) {

    }

   /**
    * Edit add-on
    * @param int $id
    *
    function edit($id) {

    }*/

   /**
    * Edit a version
    * @param int $id
    */
    function editVersion($id) {

    }

}

?>
