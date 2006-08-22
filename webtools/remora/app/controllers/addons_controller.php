<?php
require_once('Archive/Zip.php');

class AddonsController extends AppController
{
    var $name = 'Addons';
    var $components = array('Amo', 'Rdf', 'Versioncompare');

   /**
    *
    */
    function index() {
        $this->set('addons', $this->Amo->addonTypes);
    }

   /**
    * Add new or new version of an add-on
    * @param int $id
    */
    function add($id = 0) {
        $this->layout = 'developers';
        $this->set('addonTypes', $this->Amo->addonTypes);

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
                $fileErrors = array('1' => 'Exceeds maximum upload size',
                                    '2' => 'Exceeds maximum upload size',
                                    '3' => 'Incomplete transfer',
                                    '4' => 'No file uploaded'
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
                $this->set('fileError', 'Disallowed file extension ('.$fileExtension.')');
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
                $this->set('fileError', 'Could not move file');
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
                    $this->set('fileError', 'No install.rdf present');
                    $this->render('add_step1');
                    die();
                }

                //Use Rdf Component to parse install.rdf
                $manifestData = $this->Rdf->parseInstallManifest($fileContents);
pr($manifestData);
                //If the result is a string, it is an error message
                if (!is_array($manifestData)) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', 'The following error occurred while parsing install.rdf: '.$manifestData);
                    $this->render('add_step1');
                    die();
                }

                //Check if install.rdf has an updateURL
                if (isset($manifestData['updateURL'])) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', 'Add-ons cannot use an external updateURL. Please remove this from install.rdf and try again.');
                    $this->render('add_step1');
                    die();
                }

                //Check the GUID
                if (!isset($manifestData['id']) || !preg_match('/^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i', $manifestData['id'])) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', 'The ID of this add-on is invalid or not set.');
                    $this->render('add_step1');
                    die();
                }

                //Make sure version has no spaces
                if (!isset($manifestData['version']) || preg_match('/.*\s.*/', $manifestData['version'])) {
                    $this->Addon->invalidate('file');
                    $this->set('fileError', 'The version of this add-on is invalid or not set. Versions cannot contain spaces.');
                    $this->render('add_step1');
                    die();
                }

                //These are arrays by locale
                $addonNames = $manifestData['name'];
                $addonDesc = $manifestData['description'];

                //If adding a new version to existing addon, check author
                $existing = $this->Addon->findAllByGuid($manifestData['id']);

            }
            //If it is a search plugin, read the .src file
            else {

            }


            $this->set('fileName', $fileName);
            $this->set('fileSize', $fileSize);
            $this->set('manifestData', $manifestData);
            $this->render('add_step2');
        }
        elseif (isset($this->data['Addon']['add_step2'])) {
            $this->render('add_step3');
        }
        elseif (isset($this->data['Addon']['add_step3'])) {
            $this->render('add_step4');
        }
        //Step 1: Add-on type and file upload
        else {
            if ($id != 0) {
                //updating add-on
            }

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
    */
    function edit($id) {

    }

   /**
    * Edit a version
    * @param int $id
    */
    function editVersion($id) {

    }

}

?>
