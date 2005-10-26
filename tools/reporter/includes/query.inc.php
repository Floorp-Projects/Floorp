<?php
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');

class query
{
    function query(){}

    function getQueryInputs(){
        global $config;

        // approved "selectable" and "queryable" fields
        $approved_selects = array('count' /*special */,
                                  'host_id',
                                  'host_hostname',
                                  'report_id',
                                  'report_url',
                                  'report_host_id',
                                  'report_problem_type',
                                  'report_description',
                                  'report_behind_login',
                                  'report_useragent',
                                  'report_platform',
                                  'report_oscpu',
                                  'report_language',
                                  'report_gecko',
                                  'report_buildconfig',
                                  'report_product',
/*                                'report_email',
                                  'report_ip',
*/                                'report_file_date'
        );
        $approved_wheres = array('host_hostname',
                                 'report_id',
                                 'report_url',
                                 'report_problem_type',
                                 'report_description',
                                 'report_behind_login',
                                 'report_useragent',
                                 'report_platform',
                                 'report_oscpu',
                                 'report_gecko',
                                 'report_language',
                                 'report_gecko',
                                 'report_buildconfig',
                                 'report_product',
                                 'report_file_date'
        );

        /*******************
         * ASCDESC
         *******************/
        if (strtolower($_GET['ascdesc']) == 'asc' || strtolower($_GET['ascdesc']) == 'asc'){
            $ascdesc = $_GET['ascdesc'];
        } else {
            $ascdesc = 'desc';
        }

        /*******************
         * ORDER BY
         *******************/
        if (isset($_GET['orderby']) && in_array($_GET['orderby'], $approved_wheres)) {
            $orderby = $_GET['orderby'];
        } else {
            $orderby = 'report_file_date';
        }
        // no default, since we sub order later on

        /*******************
         * SHOW (really "Limit")
         *******************/
        if (!$_GET['show']){
            $show = $config['show'];
        } else {
            $show = $_GET['show'];
        }

        // no more than 200 results per page
        if (!$_GET['show'] > 200){
            $show = 200;
        }

        /*******************
         * PAGE (really other part of "Limit"
         *******************/
        if (!$_GET['page']){
            $page = 1;
        } else {
            $page = $_GET['page'];
        }

        /*******************
         * Count
         *******************/
        if (isset($_GET['count'])){
            $count = 'host_id'; // XX limitation for now
        }
        // if nothing... it's nothing

        /*******************
         * SELECT
         *******************/
        $selected = array();

        /*
           If user defines what to select and were not counting, just
           use their input.
        */
        if ($_GET['selected'] && !isset($_GET['count'])){
            foreach($_GET['selected'] as $selectedChild){
              $selected[$selectedChild] = $config['fields'][$selectedChild];
            }
        } else {

            // Otherwise, we do it for  them
            $selected = array('report_id' => 'Report ID', 'host_hostname' => 'Host');
        }

        // If we are counting, we need to add A column for it
        if (isset($_GET['count'])){
            // set the count variable

            $selected['count'] = 'Number';
            unset($selected['report_id']);

            // Hardcode host_id
            $_GET['count'] = 'host_id'; // XXX we just hardcode this (just easier for now, and all people will be doing).
            // XX NOTE:  We don't escape count below because 'host_id' != `host_id`.

            //Sort by
            if (!isset($orderby)){ //XXX this isn't ideal, but nobody will sort by date (pointless and not an option)
                $orderby = 'count';
            }
        }
        else {
            if(!isset($selected['report_id'])){
                $selected['report_id'] = "Report ID";
                $artificialReportID = true;
            }
//            $selected['report_file_date'] = "Date";
        }
        /*******************
         * WHERE
         *******************/
        $where = array();
        reset($_GET);
        while (list($column, $value) = each($_GET)) {
            /* To help prevent stupidity with columns, we only add
               it to the WHERE statement if it's passes as a column
               we allow.  Others simly don't make sense, or we don't
               allow them for security purposes.
            */
            if (in_array($column, $approved_wheres)){
                // these are our various ways of saying "no value"
                if (($value != -1) && ($value != null) && ($value != '0')){
                    // if there's a wildcard (%,_) we should use 'LIKE', otherwise '='
                    if ((strpos($value, "%") === false) && (strpos($value, "_") === false)){
                        $operator = "=";
                    } else {
                        $operator = "LIKE";
                    }
                    // Add to query
                    if (in_array($column, $approved_wheres)){
                        $where[] = array($column, $operator, $value);
                    }
                }
            }
        }

        return array('selected'               => $selected,
                     'where'                  => $where,
                     'orderby'                => $orderby,
                     'ascdesc'                => $ascdesc,
                     'show'                   => $show,
                     'page'                   => $page,
                     'count'                  => $count,
                     'artificialReportID'     => $artificialReportID
        );
    }

    function doQuery($select, $where, $orderby, $ascdesc, $show, $page, $count){
        global $db;
        $db->debug = true;

        /************
         * SELECT
         ************/
        $sql_select = 'SELECT ';
        foreach($select as $select_child => $select_child_title){
            // we don't $db->quote here since unless it's in our approved array (exactly), we drop it anyway. i.e. report_id is on our list, 'report_id' is not.
            // we sanitize on our own
            if ($select_child == 'count'){
                $sql_select .= 'COUNT( '.$count.' ) AS count';
                $orderby = 'COUNT';
            } else {
                $sql_select .= $select_child;
            }
            $sql_select .= ', ';
        }
        if(sizeof($select) > 0){
            $sql_select = substr($sql_select, 0, -2);
            $sql_select = $sql_select.' ';
        }

        /************
         * FROM
         ************/
        $sql_from = 'FROM `report`, `host`';

        /************
         * WHERE
         ************/
        $sql_where = 'WHERE ';
        foreach($where as $where_child){
            $sql_where .= $where_child[0].' '.$where_child[1].' '.$db->quote($where_child[2]).' AND ';
        }

        // Dates ar special
        // if the user didn't delete the default YYYY-MM-DD mask, we do it for them
        if ($_GET['report_file_date_start'] == 'YYYY-MM-DD'){
            $_GET['report_file_date_start'] = null;
        }
        if ($_GET['report_file_date_end'] == 'YYYY-MM-DD'){
            $_GET['report_file_date_end'] = null;
        }
        if (($_GET['report_file_date_start'] != null)  || ($_GET['report_file_date_end'] != null)){

            // if we have both, we do a BETWEEN
            if ($_GET['report_file_date_start'] && $_GET['report_file_date_end']){
                $sql_where .= "(report_file_date BETWEEN ".$db->quote($_GET['report_file_date_start'])." and ".$db->quote($_GET['report_file_date_end']).") AND ";
            }

            // if we have only a start, then we do a >
            else if ($_GET['report_file_date_start']){
                $sql_where .= "report_file_date > ".$db->quote($_GET['report_file_date_start'])." AND ";
            }

            // if we have only a end, we do a <
            else if ($_GET['report_file_date_end']){
                $sql_where .= "report_file_date < ".$db->quote($_GET['report_file_date_end'])." AND ";
            }
        }

        if(sizeof(trim($sql_where)) <= 0){
            $sql_where .= ' AND ';
        }
        $sql_where .= 'host.host_id = report_host_id ';

        /************
         * OrderBy
         ************/
        if($orderby){
            $sql_orderby = 'ORDER BY '.$orderby.' ';
        }
        
        /*******************
         * Count
         *******************/
        if (isset($_GET['count'])){
            $sql_groupby = 'GROUP BY host_id DESC ';
        }

        $sql = $sql_select." \r".$sql_from." \r".$sql_where." \r".$sql_groupby.$sql_orderby.$ascdesc." \r".$sql_subOrder;

        // Calculate Start
        $start = ($page-1)*$show;

        /**************
         * QUERY
         **************/
        $dbQuery = $db->SelectLimit($sql,$show,$start,$inputarr=false);

        if ($dbQuery){
            while (!$dbQuery->EOF) {
                $dbResult[] = $dbQuery->fields;
                $dbQuery->MoveNext();
 	    }
        }
        /**************
         * Count Total
         **************/
        if($dbQuery){
            $totalQuery = $db->Execute("SELECT COUNT(*)
                                        FROM `report`, `host`
                                        $sql_where");
            $totalResults = $totalQuery->fields['COUNT(*)'];
        }


        return array('data' => $dbResult, 'totalResults' => $totalResults);
    }

    function continuityParams($query_input){
        reset($query_input['where']);
        foreach($query_input['where'] as $node => $item){
            if($item[0] == 'report_id' && $query_input['artificialReportID']){
            } else {
                if(!is_numeric($item[2])){
                    $continuity_params .= $item[0].'='.$item[2];
                } else {
                    $continuity_params .= $item[0].'='.urlencode($item[2]);
                }
                $continuity_params .= '&amp;';
            }
        }
        foreach($query_input['selected'] as $selected_node => $selected_item){
            if($selected_node == 'report_id' && $query_input['artificialReportID']){
            } else {
                $continuity_params .= 'selected%5B%5D='.$selected_node.'&amp;';
            }
        }
        if($query_input['count']){
             $continuity_params .= '&amp;count=on';
        }
        return $continuity_params;
    }

    function columnHeaders($query_input, $continuity_params){
        $columnCount = 0;
        $column[$columnCount]['text'] = 'Detail';
        $columnCount++;
        foreach($query_input['selected'] as $title_name => $title){
            if($title_name == 'report_id' && $query_input['artificialReportID']){
            } else {
                $column[$columnCount]['text'] = $title;

                $o_orderby = $title_name;
                if ($query_input['ascdesc'] == 'desc'){
                    $o_ascdesc = 'asc';
                } else {
                    $o_ascdesc = 'desc';
                }

                $column[$columnCount]['url'] = '?'.$continuity_params.'&amp;orderby='.$o_orderby.'&amp;ascdesc='.$o_ascdesc;
                $columnCount++;
            }
        }
        return $column;
    }

    function outputHTML($result, $query_input){

        // Continuity
        $continuity_params = $this->continuityParams($query_input);

        // Columns
        $columnHeaders = $this->columnHeaders($query_input, $continuity_params);

        // Data
        $data = array();
        $rowNum = 0;
        if(sizeof($result['data']) > 0){
            foreach($result['data'] as $row){
                $colNum = 0;

                // Prepend if new_front;
                $data[$rowNum][0]['text'] = 'Detail';
                if (isset($row['count'])){
                    $data[$rowNum][0]['url']  = '/query/?host_hostname='.$row['host_hostname'];
                }
                else {
                  $data[$rowNum][0]['url']  = '/report/?report_id='.$row['report_id'];
                }
                $colNum++;
    
                foreach($row as $cellName => $cellData){
                    if($cellName == 'report_id' && $query_input['artificialReportID']){
                    } else {
                        $data[$rowNum][$colNum]['text'] = $cellData;
                    }
                    $colNum++;
                }
                $rowNum++;
            }
        }
        return array('columnHeaders' => $columnHeaders, 'data' => $data);
    }

    function outputXML(){}
    function outputCSV(){}
    function outputXLS(){}
    function outputRSS(){}
}
?>