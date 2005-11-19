<?php
require_once($config['base_path'].'/includes/contrib/adodb/adodb.inc.php');

class query
{
        // approved "selectable" and "queryable" fields.  In sort order
        var $approved_selects = array('count' /*special */,
                                      'report_file_date',
                                      'host_hostname',
                                      'report_url',
                                      'report_behind_login',
                                      'report_useragent',
                                      'report_problem_type',
                                      'report_platform',
                                      'report_oscpu',
                                      'report_gecko',
                                      'report_product',
                                      'report_language',
                                      'report_id',
                                      'report_buildconfig',
                                      'report_description',
    /*                                'report_email',
                                      'report_ip',
    */
                                      'report_host_id',
                                      'host_id',
        );
        var $approved_wheres = array('count',
                                     'host_hostname',
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


    function query(){}

    function getQueryInputs(){
        global $config;

        /*******************
         * ASCDESC
         *******************/
        if (strtolower($_GET['ascdesc']) == 'asc' || strtolower($_GET['ascdesc']) == 'asc'){
            $ascdesc = $_GET['ascdesc'];
        } else {
            $ascdesc = 'desc';
        }

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
                if(in_array(strtolower($selectedChild), $this->approved_selects)){
                    $selected[$selectedChild] = $config['fields'][$selectedChild];
                }
            }
        } else {

            // Otherwise, we do it for  them
            $selected = array('report_id' => 'Report ID', 'host_hostname' => 'Host');
        }


      /*******************
       * ORDER BY
       *******************/
        $orderby = array();
        if(in_array(strtolower($_GET['orderby']), $this->approved_selects)){
            $orderby[] = $_GET['orderby'];
        }
        $orderby = array_merge($orderby, $this->calcOrderBy($selected));

        // If we are counting, we need to add A column for it
        if (isset($_GET['count'])){
            // set the count variable

            $selected['count'] = 'Number';
            unset($selected['report_id']);

            // Hardcode host_id
            $_GET['count'] = 'host_id'; // XXX we just hardcode this (just easier for now, and all people will be doing).
            // XX NOTE:  We don't escape count below because 'host_id' != `host_id`.

            /*******************
             * ORDER BY
             *******************/
            if (!isset($orderby)){
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
            if (in_array(strtolower($column), $this->approved_wheres) && strtolower($column) != 'count' && strtolower($column)){
                // these are our various ways of saying "no value"
                if (($value != -1) && ($value != null) && ($value != '0')){
                    // if there's a wildcard (%,_) we should use 'LIKE', otherwise '='
                    if ((strpos($value, "%") === false) && (strpos($value, "_") === false)){
                        $operator = "=";
                    } else {
                        $operator = "LIKE";
                    }
                    // Add to query
                    if (in_array(strtolower($column), $this->approved_wheres)){
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

        /************
         * SELECT
         ************/
        $sql_select = 'SELECT ';
        foreach($select as $select_child => $select_child_title){
            // we don't $db->quote here since unless it's in our approved array (exactly), we drop it anyway. i.e. report_id is on our list, 'report_id' is not.
            // we sanitize on our own
            if ($select_child == 'count'){
                $sql_select .= 'COUNT( '.$count.' ) AS count';
                $orderby = array_merge(array('count'), $orderby);
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

        /*******************
         * ORDER BY
         *******************/
         if(isset($orderby)){
             $sql_orderby = 'ORDER BY ';
             $orderbyCounter =0;
             foreach($orderby as $orderChild){
                 $sql_orderby .= $orderChild;
                 if($orderbyCounter == 0){
                     $sql_orderby .= ' '.$ascdesc;
                 }
                 $sql_orderby .= ',';
                 $orderbyCounter++;
             }
             $sql_orderby = substr($sql_orderby,0,-1).' ';
             if(sizeof($orderby) > 1){
                 $sql_orderby .= ' DESC ';
             }
         }

        /*******************
         * Count
         *******************/
        if (isset($_GET['count'])){
            $sql_groupby = 'GROUP BY host_id DESC ';
        }

        $sql = $sql_select." \r".$sql_from." \r".$sql_where." \r".$sql_groupby.$sql_orderby." \r".$sql_subOrder;

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
                    $standard .= $item[0].'='.$item[2].'&amp;';;
                } else {
                    $standard .= $item[0].'='.urlencode($item[2]).'&amp;';;
                }
            }
        }
        foreach($query_input['selected'] as $selected_node => $selected_item){
            if($selected_node == 'report_id' && $query_input['artificialReportID']){
            } else {
                if($selected_node == 'count'){
                    $complete .= 'selected%5B%5D='.$selected_node.'&amp;';
                } else {
                    $standard .= 'selected%5B%5D='.$selected_node.'&amp;';
                }
            }
        }

        // make complete standard + complete
        $complete = $standard.$complete;

        // finish off complete
        if($query_input['count']){
             $complete .= '&amp;count=on';
        }

        // lets return
        return array($standard, $complete);
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
                if((isset($query_input['count']) && $title_name == 'count') || !isset($query_input['count'])){
                    $column[$columnCount]['url'] = '?'.$continuity_params[1].'&amp;orderby='.$o_orderby.'&amp;ascdesc='.$o_ascdesc;
                }
                $columnCount++;
            }
        }
        return $column;
    }

    function outputHTML($result, $query_input, $continuity_params, $columnHeaders){
        global $iolib;

        // Data
        $data = array();
        $rowNum = 0;
        if(sizeof($result['data']) > 0){
            foreach($result['data'] as $row){
                $colNum = 0;

                // Prepend if new_front;
                $data[$rowNum][0]['text'] = 'Detail';
                if (isset($row['count'])){
                    $data[$rowNum][0]['url']  = '/query/?host_hostname='.$row['host_hostname'].'&amp;'.$continuity_params[0];
                }
                else {
                    $data[$rowNum][0]['url']  = '/report/?report_id='.$row['report_id'].'&amp;'.$continuity_params[0];
                }
                $colNum++;
    
                foreach($row as $cellName => $cellData){
                    if($cellName == 'report_id' && $query_input['artificialReportID']){
                    } else {
                        $data[$rowNum][$colNum]['col'] = $cellName;

                        if($cellName == 'report_problem_type'){
                            $cellData = resolveProblemTypes($cellData);
                        }
                        else if($cellName == 'report_behind_login'){
                            $cellData = resolveBehindLogin($cellData);
                        }
                        $data[$rowNum][$colNum]['text'] = $cellData;
                    }
                    $colNum++;
                }
                $rowNum++;
            }
        }
        return array('data' => $data);
    }

    function outputXML(){}
    function outputCSV(){}
    function outputXLS(){}
    function outputRSS(){}
    
    function calcOrderBy($query){
        $result = array();
        foreach($this->approved_selects as $selectNode){
            if(array_key_exists($selectNode, $query)){
                $result[] = $selectNode;
            }
        }
        return $result;
    }
}
?>