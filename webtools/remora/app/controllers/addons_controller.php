<?php

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
            }

            $fileName = $this->data['Addon']['file']['name'];
            $fileExtension = substr($fileName, strrpos($fileName, '.'));
            $allowedExtensions = array('.xpi', '.jar', '.src');
            //Check for file extenion match
            if (!in_array($fileExtension, $allowedExtensions)) {
                $this->Addon->invalidate('file');
                $this->set('fileError', 'Disallowed file extension ('.$fileExtension.')');
                $this->render('add_step1');
                die();
            }
echo('test');
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
            }

            //Find install.rdf in the package and get contents
            $manifestExists = false;

            if ($zip = @zip_open($uploadedFile)) {
                while ($zipEntry = zip_read($zip)) {
                    if (zip_entry_name($zipEntry) == 'install.rdf') {
                        $manifestExists = true;
                        if (zip_entry_open($zip, $zipEntry, 'r')) {
                            $manifestData = zip_entry_read($zipEntry, zip_entry_filesize($zipEntry));
                            zip_entry_close($zipEntry);
                        }
                    }
                }
                zip_close($zip);
            }

            //Parse install.rdf
            if ($manifestExists === true) {

            }

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
