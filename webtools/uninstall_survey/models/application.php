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

uses('Sanitize');
class Application extends AppModel {
    var $name = 'Application';

    /* Rather than adding this association here, only add it when you need it.
     * Otherwise queries become very slow (since the result table is so large). Just
     * call the following before you need to join the tables:
     *      $this->bindModel(array('hasOne' => array('Result')));
     */
    //var $hasOne = array('Result');

    var $hasAndBelongsToMany = array(
                                'Collection' => array(  'className'  => 'Collection',
                                                        'joinTable' => 'applications_collections',
                                                        'foreignKey' => 'application_id',
                                                        'associationForeignKey' => 'collection_id'
                                                        )
                               );

    var $Sanitize;

    function Application() {
        parent::AppModel();

        $this->Sanitize = new Sanitize();
    }

    /**
     * Expecting to have cake's version of $_GET
     * Will return the default APP+Version if given invalid input.
     */
    function getIdFromUrl($params) 
    {
        $_conditions = array();

        // defaults
        if (empty($params['product'])) {
            array_push($_conditions, "`Application`.`name` LIKE '".DEFAULT_APP_NAME."'");
            array_push($_conditions, "`Application`.`version` LIKE '".DEFAULT_APP_VERSION."'");
        } else {
            // product's come in looking like:
            //      Mozilla Firefox 1.5.0.1
            $_exp = explode(' ',urldecode($params['product']));

            if(count($_exp) == 3) {
                $_product = $_exp[0].' '.$_exp[1];

                $_version = $_exp[2];

                array_push($_conditions, "`Application`.`name` LIKE '{$_product}'");
                array_push($_conditions, "`Application`.`version` LIKE '{$_version}'");
            } else {
                array_push($_conditions, "`Application`.`name` LIKE '".DEFAULT_APP_NAME."'");
                array_push($_conditions, "`Application`.`version` LIKE '".DEFAULT_APP_VERSION."'");
            }
        }

        $_application_id = $this->findAll($_conditions, 'Application.id');
        if (is_numeric($_application_id[0]['Application']['id'])) {
            return $_application_id[0]['Application']['id'];
        } else {
            return false;
        }
    }

    /**
     * @param int application id
     * @return array set of intentions
     */
    function getIntentions($id) {
        // this should never happen...
        if (!is_numeric($id)) {
            return array();
        }

        $_max_id = $this->getMaxCollectionId($id,'intention');
        if (!is_numeric($_max_id[0][0]['max'])) {
            return array();
        }

        $_query = "
            SELECT 
                * 
            FROM 
                applications 
            JOIN applications_collections ON application_id = applications.id 
            JOIN collections ON collections.id = applications_collections.collection_id 
            JOIN choices_collections ON choices_collections.collection_id = collections.id 
            JOIN choices ON choices.id = choices_collections.choice_id 
            WHERE 
                applications.id={$id} 
            AND 
                choices.type='intention'
            AND
                collections.id={$_max_id[0][0]['max']}
            ";

        return $this->query($_query);
    }

    /**
     * @param int application id
     * @return array set of issues
     */
    function getIssues($id) 
    {
        // this should never happen...
        if (!is_numeric($id)) {
            return array();
        }

        $_max_id = $this->getMaxCollectionId($id,'issue');
        if (!is_numeric($_max_id[0][0]['max'])) {
            return array();
        }


        $_query = "
            SELECT 
                * 
            FROM 
                applications 
            JOIN applications_collections ON application_id = applications.id 
            JOIN collections ON collections.id = applications_collections.collection_id 
            JOIN choices_collections ON choices_collections.collection_id = collections.id 
            JOIN choices ON choices.id = choices_collections.choice_id 
            WHERE 
                applications.id={$id} 
            AND 
                choices.type='issue'
            AND
                collections.id={$_max_id[0][0]['max']}
            ";

        return $this->query($_query);
    }

    function getMaxCollectionId($id, $type)
    {
        if (!is_numeric($id)) {
            return false;
        }
        $_type = $this->Sanitize->sql($type);

        $_query = "
            SELECT 
                MAX(collections.id) as max
            FROM 
                applications 
            JOIN applications_collections ON application_id = applications.id 
            JOIN collections ON collections.id = applications_collections.collection_id 
            JOIN choices_collections ON choices_collections.collection_id = collections.id 
            JOIN choices ON choices.id = choices_collections.choice_id 
            WHERE 
                applications.id={$id} 
            AND 
                choices.type='{$_type}'
            ";

        return $this->query($_query);
    }


    /**
     * @param array cake url array
     * @return array set of collections
     */
    function getCollectionsFromUrl($params, $type) 
    {
        // Clean parameters for inserting into SQL
        $params = $this->cleanArrayForSql($params);

        $_conditions = "1=1";
        // Firstly, determine our application
        if (!empty($params['product'])) {

            // product's come in looking like:
            //      Mozilla Firefox 1.5.0.1
            $_exp = explode(' ',urldecode($params['product']));
            
            if(count($_exp) == 3) {
                $_product = $_exp[0].' '.$_exp[1];

                $_version = $_exp[2];

                $_conditions .= " AND `Application`.`name` LIKE '{$_product}'";
                $_conditions .= " AND `Application`.`version` LIKE '{$_version}'";
            } else {
                // defaults I guess?
                $_conditions .= " AND `Application`.`name` LIKE '".DEFAULT_APP_NAME."'";
                $_conditions .= " AND `Application`.`version` LIKE '".DEFAULT_APP_VERSION."'";
            }
        } else {
            // I'm providing a default here, because otherwise all results will be
            // returned (across all applications) and that is not desired
            $_conditions .= " AND `Application`.`name` LIKE '".DEFAULT_APP_NAME."'";
            $_conditions .= " AND `Application`.`version` LIKE '".DEFAULT_APP_VERSION."'";
        }

        $_application_id = $this->findAll($_conditions, 'Application.id');

        if (is_numeric($_application_id[0]['Application']['id'])) {
            return $this->getCollections($_application_id[0]['Application']['id'], $type);
        } else {
            return false;
        }
    }

    /**
     * @param int application id
     * @param string choice type (either 'issue' or 'intention' right now)
     * @return array set of collections
     */
    function getCollections($id, $type) 
    {
        // this should never happen...
        if (!is_numeric($id)) {
            return array();
        }

        $_type = $this->Sanitize->sql($type);
        $_query = "
            SELECT 
                * 
            FROM 
                applications 
            JOIN applications_collections ON application_id = applications.id 
            JOIN collections ON collections.id = applications_collections.collection_id 
            JOIN choices_collections ON choices_collections.collection_id = collections.id 
            JOIN choices ON choices.id = choices_collections.choice_id 
            WHERE
                applications.id={$id} 
            AND 
                choices.type='{$_type}'
            GROUP BY collections.description
            ORDER BY collections.id DESC";

        return $this->query($_query);
    }

}
?>
