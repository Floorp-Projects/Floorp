<?php
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is survey.mozilla.com site.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Wil Clouser <clouserw@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

class Result extends AppModel {
    var $name = 'Result';

    var $belongsTo = array('Application');

    var $hasAndBelongsToMany = array('Choice' =>
                                   array('className' => 'Choice',
                                         'uniq'      => true
                                        )
                               );

    var $Sanitize;

    function Result()
    {
        parent::appModel();
        $this->Sanitize = new Sanitize();
    }

    /**
     * Count's all the comments.  To speed things up I'm using found_rows().  That
     * means this must be called directly after getComments()!
     * @return int count value
     */
    function getCommentCount()
    {
        $comments = $this->query("SELECT FOUND_ROWS() as count");

        return $comments[0][0]['count'];
    }

    /**
     * Will retrieve all the comments within param's and pagination's parameters
     * @param array URL parameters
     * @param array pagination values from the controller
     * @param boolean if privacy is true phone numbers and email addresses will be
     * masked
     * @return cake result set
     */
    function getComments($params, $pagination, $privacy=true)
    {
        $params = $this->cleanArrayForSql($params);

        $_application_id = $this->Application->getIdFromUrl($params);

        // We only want to see rows with comments
        $_conditions = array("comments NOT LIKE ''");

        if (!empty($params['start_date'])) {
            $_timestamp = strtotime($params['start_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                array_push($_conditions, "`created` >= '{$_date}'");
            }
        }
        if (!empty($params['end_date'])) {
            $_timestamp = strtotime($params['end_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d 23:59:59', $_timestamp);//sql format
                array_push($_conditions, "`created` <= '{$_date}'");
            }
        }

        $_application_id = $this->Application->getIdFromUrl($params);
        array_push($_conditions, "`Result`.`application_id`={$_application_id}");

        // Next determine our collection
        if (!empty($params['collection'])) {
            $_collection_id = $this->Choice->Collection->findByDescription($params['collection']);
            $clear = true;
            foreach ($_collection_id['Application'] as $var => $val) {
                if ($_application_id == $val['id']) {
                    $clear = false;
                }
            }
            if ($clear) {
                $_id = $this->Application->getMaxCollectionId($_application_id, 'issue');
                $_collection_id['Collection']['id'] = $_id[0][0]['max'];
            }
        } else {
            $_id = $this->Application->getMaxCollectionId($_application_id, 'issue');
            $_collection_id['Collection']['id'] = $_id[0][0]['max'];
        }

        // SQL_CALC_FOUND_ROWS used for counting comments.
        // The nested query brings back too many rows (all applications) but this
        // query is 300% faster than doing joins to get the correct rows back (look
        // in CVS history if you want that query)
        $_query = "
            SELECT 
                SQL_CALC_FOUND_ROWS DISTINCT 
                `Result`.`id`, 
                `Result`.`comments`, 
                `Result`.`created` 
            FROM 
                `results` AS `Result` 
            WHERE comments != ''
            AND Result.application_id={$_application_id}
            AND Result.id IN (
                SELECT choices_results.result_id
                FROM choices_results
                JOIN choices ON choices_results.choice_id = choices.id 
                JOIN choices_collections ON choices_collections.choice_id = choices.id 
                WHERE collection_id={$_collection_id['Collection']['id']}
            )
            ";

        $_start =($pagination['page'] -1) * $pagination['show'];

        $_query .= "
            ORDER BY `Result`.`created` {$pagination['direction']}
            LIMIT {$_start},{$pagination['show']}
        ";
        $comments = $this->query($_query);

        if ($privacy) {
            // Pull out all the email addresses and phone numbers.  The original
            // lines are below, but commented out for the sake of speed.
            // preg_replace() will replace a single level of an array according to a
            // pattern.  This behavior doesn't seem to be documented (at this time), so I'm not sure
            // if they are going to "fix" it later.  If they do, you can replace the
            // current code with the commented ones, but realize it will take about
            // twice as long.
            foreach ($comments as $var => $val) {

                // Handle foo@bar.com
                $_email_regex = '/\ ?(.+)?@(.+)?\.(.+)?\ ?/';
                $comments[$var]['Result'] = preg_replace($_email_regex,'$1@****.$3',$comments[$var]['Result']);

                //$comments[$var]['Result']['comments'] = preg_replace($_email_regex,'$1@****.$3',$comments[$var]['Result']['comments']);
                //$comments[$var]['Result']['intention_text'] = preg_replace($_email_regex,'$1@****.$3',$comments[$var]['Result']['intention_text']);

                // Handle xxx-xxx-xxxx
                $_phone_regex = '/([0-9]{3})[ .-]?[0-9]{4}/';
                $comments[$var]['Result'] = preg_replace($_phone_regex,'$1-****',$comments[$var]['Result']);

                //$comments[$var]['Result']['comments'] = preg_replace($_phone_regex,'$1-****',$comments[$var]['Result']['comments']);
                //$comments[$var]['Result']['intention_text'] = preg_replace($_phone_regex,'$1-****',$comments[$var]['Result']['intention_text']);
            }
        }


        return $comments;
    }


    /**
     * This function runs the query to get the export data for the CSV file.
     *
     * @param array URL parameters
     * @param boolean if privacy is true phone numbers and email addresses will be
     * masked
     * @return array two dimensional array that should be pretty easy to transform
     *  into a CSV.
     */
    function getCsvExportData($params, $privacy=true)
    {
        $params = $this->cleanArrayForSql($params);

        // We have to use a left join here because there isn't always an intention
        $_query = "
            SELECT
                `results`.`id`,
                `results`.`created`,
                `results`.`intention_text` as `intention_other`,
                `results`.`comments`,
                `intentions`.`description` as `intention`
            FROM `results` 
            LEFT JOIN `intentions` ON `results`.`intention_id`=`intentions`.`id` 
            INNER JOIN `applications` ON `applications`.`id` = `results`.`application_id`
            WHERE
                1=1
        ";
        
        if (!empty($params['start_date'])) {
            $_timestamp = strtotime($params['start_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                $_query .= " AND `results`.`created` >= '{$_date}'";
            }
        }

        if (!empty($params['end_date'])) {
            $_timestamp = strtotime($params['end_date']);

            if (!($_timestamp == -1) || $_timestamp == false) {
                $_date = date('Y-m-d 23:59:59', $_timestamp);//sql format
                $_query .= " AND `results`.`created` <= '{$_date}'";
            }
        }

        if (!empty($params['product'])) {
            // product's come in looking like:
            //      Mozilla Firefox 1.5.0.1
            $_exp = explode(' ',urldecode($params['product']));

            if(count($_exp) == 3) {
                $_product = $_exp[0].' '.$_exp[1];

                $_version = $_exp[2];

                $_query .= " AND `applications`.`name` LIKE '{$_product}'";
                $_query .= " AND `applications`.`version` LIKE '{$_version}'";
            } else {
                // defaults I guess?
                $_query .= " AND `applications`.`name` LIKE '".DEFAULT_APP_NAME."'";
                $_query .= " AND `applications`.`version` LIKE '".DEFAULT_APP_VERSION."'";
            }
        } else {
            // I'm providing a default here, because otherwise all results will be
            // returned (across all applications) and that is not desired
            $_query .= " AND `applications`.`name` LIKE '".DEFAULT_APP_NAME."'";
            $_query .= " AND `applications`.`version` LIKE '".DEFAULT_APP_VERSION."'";
        }

        $_query .= " ORDER BY `results`.`created` ASC";

        $res = $this->query($_query);

        // Since we're exporting to a CSV, we need to flatten the results into a 2
        // dimensional table array

        foreach ($res as $result) {

            $newdata[] = array_merge($result['results'], $result['intentions']);
        }

        if ($privacy) {
            // Pull out all the email addresses and phone numbers.  The original
            // lines are below, but commented out for the sake of speed.
            // preg_replace() will replace a single level of an array according to a
            // pattern.  This behavior doesn't seem to be documented (at this time), so I'm not sure
            // if they are going to "fix" it later.  If they do, you can replace the
            // current code with the commented ones, but realize it will take about
            // twice as long.
            foreach ($newdata as $var => $val) {

                // Handle foo@bar.com
                $_email_regex = '/\ ?(.+)?@(.+)?\.(.+)?\ ?/';
                $newdata[$var] = preg_replace($_email_regex,'$1@****.$3',$newdata[$var]);

                //$newdata[$var]['comments'] = preg_replace($_email_regex,'$1@****.$3',$newdata[$var]['comments']);
                //$newdata[$var]['intention_other'] = preg_replace($_email_regex,'$1@****.$3',$newdata[$var]['intention_other']);

                // Handle xxx-xxx-xxxx
                $_phone_regex = '/([0-9]{3})[ .-]?[0-9]{4}/';
                $newdata[$var] = preg_replace($_phone_regex,'$1-****',$newdata[$var]);

                //$newdata[$var]['comments'] = preg_replace($_phone_regex,'$1-****',$newdata[$var]['comments']);
                //$newdata[$var]['intention_other'] = preg_replace($_phone_regex,'$1-****',$newdata[$var]['intention_other']);
            }
        }

        // Our CSV library just prints out everything in order, so we have to put the
        // column labels on here ourselves
        $newdata = array_merge(array(array_keys($newdata[0])), $newdata);

        return $newdata;
    }

    /**
     * Will retrieve the information used for graphing.  This function has undergone
     * quite an evolution in the name of speed, from 1 query to 3, speedwise from
     * 5.5s to just under a half a second.  Thanks to morgamic for some crazy
     * sql kung fu. :)
     * @param the url parameters (unescaped)
     * @return a result set
     */
    function getDescriptionAndTotalsData($params)
    {
        // Clean parameters for inserting into SQL
        $params = $this->cleanArrayForSql($params);

        /* Below is the original query for this function.  It was beautiful and
         * brought back just what we needed.  However, it took 5.5s to run with 43000
         * results, and since that number is just going up, it won't do to have a
         * query take that long (especially one on the front page).

            //It would be nice to drop something like this in the SELECT:
            //    CONCAT(COUNT(*)/(SELECT COUNT(*) FROM our_giant_query_all_over_again)*100,'%') AS `percentage`
            $_query = "
                  SELECT
                      issues.description,
                      COUNT( DISTINCT results.id ) AS total
                  FROM
                      issues
                  LEFT JOIN issues_results ON issues_results.issue_id=issues.id
                  LEFT JOIN results        ON results.id=issues_results.result_id AND results.application_id=applications.id
                  JOIN applications_issues ON applications_issues.issue_id=issues.id
                  JOIN applications        ON applications.id=applications_issues.application_id
                  WHERE 1=1
            ";

            // Any other restraints (date, app, etc.)

            $_query .= " GROUP BY `issues`.`description`
                         ORDER BY `issues`.`description` DESC";
        * 
        * Update (Months later update with 220k results):  
        *   Running this query gets:     20 rows in set (53 min 28.78 sec)
        *
        * Update 2:
        *   The dataset is too large to support these queries on every pageload, so
        *   by default it will load from a cache table (does the queries if a date
        *   range is specified)
        */

        // Firstly, determine our application
        $_application_id = $this->Application->getIdFromUrl($params);

        if (!is_numeric($_application_id)) {
            // Not a lot we can do here, but might as well not run the queries. I'm
            // not going to put in a default, because URL params don't exist by
            // default - they requested something that we don't have, give them back
            // nothing.
            return false;

        }
        
        // Next determine our collection
        if (!empty($params['collection'])) {
            $_collection_id = $this->Choice->Collection->findByDescription($params['collection']);
            // Check if the current app has a collection by that name.  If it doesn't
            // fall back to max (this way they can jump between apps without having
            // messages that say "no data found".  This has the unfortunate side
            // effect of returning a result set, even with bad $_GET data
            $clear = true;
            foreach ($_collection_id['Application'] as $var => $val) {
                if ($_application_id == $val['id']) {
                    $clear = false;
                }
            }
            if ($clear) {
                $_id = $this->Application->getMaxCollectionId($_application_id, 'issue');
                $_collection_id['Collection']['id'] = $_id[0][0]['max'];
            }
        } else {
            // If collection isn't set, default to the highest (newest) one
            $_id = $this->Application->getMaxCollectionId($_application_id, 'issue');
            $_collection_id['Collection']['id'] = $_id[0][0]['max'];
        }

        // If they are looking for data without the date parameters set, use our
        // caching table.  This should save us some expensive queries.
        if (!empty($params['start_date']) || !empty($params['end_date'])) {

            // The next query will retrieve all the issues that are related to our
            // application.
            $_query = "
                SELECT 
                    choices.description, choices.id
                FROM
                    choices
                JOIN choices_collections ON choices_collections.choice_id = choices.id
                JOIN collections ON collections.id = choices_collections.collection_id
                JOIN applications_collections ON applications_collections.collection_id = collections.id
                JOIN applications ON applications.id = applications_collections.application_id
                AND applications.id = {$_application_id}
                AND collections.id = {$_collection_id['Collection']['id']}
                AND choices.type = 'issue'
                ORDER BY choices.pos ASC
                    ";

            $_issues = $this->query($_query);

            $_query = '';
            $_issue_ids = '';//used in the query
            $_results = array();

            foreach ($_issues as $var => $val) {
                // Cake has a pretty specific way it stores data, and this is consistent
                // with the old query.  Here we start our results array so it's holding the
                // descriptions and a zeroed total
                $_results[$val['choices']['id']]['choices']['description'] = $val['choices']['description'];
                $_results[$val['choices']['id']][0]['total'] = 0; // default to nothing - this will get filled in later

                // Since we're already walking through this loop, we might as well build
                // up a query string to get our totals
                $_issue_ids .= empty($_issue_ids) ? $val['choices']['id'] : ',
                '.$val['choices']['id'];
            }

            $_query = "
                SELECT 
                    choices_results.choice_id, count(results.id) 
                AS 
                    total 
                FROM 
                    results 
                JOIN choices_results ON results.id = choices_results.result_id
                WHERE 
                    results.application_id = {$_application_id}
                AND 
                    choices_results.choice_id in ({$_issue_ids})
            ";

            if (!empty($params['start_date'])) {
                $_timestamp = strtotime($params['start_date']);

                if (!($_timestamp == -1) || $_timestamp == false) {
                    $_date = date('Y-m-d H:i:s', $_timestamp);//sql format
                    $_query.= " AND `results`.`created` >= '{$_date}'";
                }
            }

            if (!empty($params['end_date'])) {
                $_timestamp = strtotime($params['end_date']);

                if (!($_timestamp == -1) || $_timestamp == false) {
                    $_date = date('Y-m-d 23:59:59', $_timestamp);//sql format
                    $_query .= " AND `results`.`created` <= '{$_date}'";
                }
            }

            $_query .= " GROUP BY choices_results.choice_id";

            $ret = $this->query($_query);

            foreach ($ret as $var => $val) {
                // fill in the totals we retrieved
                $_results[$val['choices_results']['choice_id']][0]['total'] = $val[0]['total'];
            }
        } else {

            // We're in luck - use the cached table
            $_query = "
                SELECT 
                    choices.id, choices.description, cache_choices_results.results_total
                FROM
                    choices
                JOIN cache_choices_results ON cache_choices_results.choice_id = choices.id
                AND cache_choices_results.application_id = {$_application_id}
                AND cache_choices_results.collection_id = {$_collection_id['Collection']['id']}
                ORDER BY choices.pos ASC
                    ";

            $ret = $this->query($_query);

            foreach ($ret as $var => $val) {
                // fill in the totals we retrieved in a cake style array
                $_results[$val['choices']['id']]['choices']['description'] = $val['choices']['description'];
                $_results[$val['choices']['id']][0]['total'] = $val['cache_choices_results']['results_total'];
            }
        }

        return $_results;

    }

    /**
     * We've got complex info to save, so I'm overriding the default save method.
     * Too bad we can't leverage some cake awesomeness. :(
     *
     * @param array Big cake array, filled with juicy $_POST goodness.  It should be
     * the same structure as any other array created by the html helper.
     * @return boolean true on success, false on failure
     */
    function save($data)
    {
        // Apparently these are all escaped for us by cake.  It still makes me
        // nervous.
        $_application_id  = $data['Application']['id'];
        $_intention_id    = $data['Intention']['id']; // Doesn't quite conform to cake standards
        $_comments        = $data['Result']['comments'];
        $_issues_text     = $this->Sanitize->Sql($data['Issue']['text']);
        $_intention_text  = $this->Sanitize->Sql($data['Intention']['text']);
        // Joined for legacy reasons
            $_user_agent  = mysql_real_escape_string("{$data['ua'][0]} {$data['lang'][0]}");
        $_http_user_agent = mysql_real_escape_string($_SERVER['HTTP_USER_AGENT']);

        // Make sure our required variables are set and correct
        if (!is_numeric($_application_id)) {
            return false;
        }

        // Special cases for the "other" fields.  If their corresponding option isn't
        // set, we don't want the field values.
            // issue is determined below
            $this->Choice->unBindModel(array('hasAndBelongsToMany' => array('Result')));
            $this->Choice->unBindModel(array('hasAndBelongsToMany' => array('Collection')));
            //$_issue_array     = $this->Issue->findByDescription('other');
            $_conditions = "description LIKE 'Other' AND type='intention'";
            $_intention_array = $this->Choice->findAll($_conditions);

            $_conditions = "description LIKE 'Other' AND type='issue'";
            $_issue_array = $this->Choice->findAll($_conditions);

            if ($_intention_id != $_intention_array[0]['Choice']['id']) {
                $_intention_text = '';
            }

        $this->set('application_id', $_application_id);
        $this->set('comments', $_comments);
        $this->set('useragent', $_user_agent);
        $this->set('http_user_agent', $_http_user_agent);

        // We kinda overrode $this's save(), so we'll have to ask our guardians
        parent::save();

        $_result_id = $this->getLastInsertID();

        // Insert our intention
        if (!empty($_intention_id)) {
            $_query = "
                INSERT INTO choices_results(
                    result_id,
                    choice_id,
                    other
                ) VALUES (
                    {$_result_id},
                    {$_intention_id},
                    '{$_intention_text}'
                )
            ";
            $this->query($_query);
        }


        // The choices_results table isn't represented by a class in cake, so we have
        // to do the query manually.
        if (!empty($data['Issue']['id'])) {

            $_query     = '';

            foreach ($data['Issue']['id'] as $var => $val) {
                // This should never happen, but hey...
                if (!is_numeric($val)) {
                    continue;
                }

                // If the 'other' id matches the id we're putting in, add the issue text
                $_other_text = ($val == $_issue_array[0]['Choice']['id']) ? $_issues_text : '';

                $_query .= empty($_query) ? "({$_result_id},{$val},'{$_other_text}')" : ",({$_result_id},{$val},'{$_other_text}')";
            }

            $_query = "
            INSERT INTO choices_results(
                result_id,
                choice_id,
                other
            ) VALUES 
                {$_query}
            ";

            $this->query($_query);
        }

        return false;
    }

}
?>
