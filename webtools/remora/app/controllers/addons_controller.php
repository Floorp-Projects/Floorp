<?php

class AddonsController extends AppController
{
    var $name = 'Addons';
    var $components = array('Rdf', 'Versioncompare');

   /**
    *
    */
    function index() {
        $this->set('addons', $this->Addon->findall());
    }

   /**
    * Add new or new version of an add-on
    * @param int $id
    */
    function add($id = 0) {
        $this->layout = 'developers';

        if (isset($this->data['Addon']['add_step1'])) {
            if ($this->data['Addon']['file']['error'] === 0) {
                $uploadedFile = $this->data['Addon']['file']['tmp_name'];
                $tempLocation = REPO_PATH.'/temp/'.$this->data['Addon']['file']['name'];

                if (move_uploaded_file($uploadedFile, $tempLocation)) {
                    $uploadedFile = $tempLocation;
                    chmod($uploadedFile, 0644);
                }
                else {
                    $this->set('fileError', 'Could not move file');
                    $this->render('add_step1');
                }

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

                if ($manifestExists === true) {

                }

                $this->render('add_step2');
            }
            else {
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
        }
        elseif (isset($this->data['Addon']['add_step2'])) {
            $this->render('add_step3');
        }
        elseif (isset($this->data['Addon']['add_step3'])) {
            $this->render('add_step4');
        }
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
