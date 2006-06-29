<?php

/**
 * A simple class to print a csv from a 2 dimensional array.
 *
 */
class csv
{
    var $filename = '';

    var $data = array();

    /**
     * @param string name of the file to send
     */
    function csv($name=null) 
    {
        $this->filename = $name;

        if (empty($this->filename)) {
            $this->filename = 'export-'.date('Y-m-d').'.csv';
        }
    }


    /**
     * Function to load data from an array into the object
     * @param array a 2 dimensional array
     */
    function loadDataFromArray(&$array)
    {
        $this->data = $array;
    }

    /**
     * Will send the CSV to the browser (including headers)
     */
    function sendCSV()
    {
        $this->_sendHeaders();

        $this->_cleanData();

        foreach ($this->data as $var => $val) {
            // We put quotes around each value here
            $line = implode('","',$val);
            echo "\"{$line}\"\n";
        }
    }

    /**
     * Cleans data for export into a csv (quotes and newlines)
     * @access private
     */
    function _cleanData()
    {
        foreach ($this->data as $var => $val) {
            // escape the quotes by doubling them
            $val = str_replace('"','""',$val);

            // fix newlines
            $val = str_replace("\n\r", "\n", str_replace("\r", "\n", $val));
        }
    }

    /**
     * Sends headers for a .csv
     * @access private
     */
    function _sendHeaders()
    {
        header("Content-type: application/x-csv");
        header('Content-disposition: inline; filename="'.$this->filename.'"');
        header('Cache-Control: private');
        header('Pragma: public'); 	
    }

}
