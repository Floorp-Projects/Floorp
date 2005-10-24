<?php
/**
 * Session handling class.
 * This class circumvents basic PHP Sessions and uses MySQL to store all session data.
 * This was done was to preserve sessions across different LVS nodes by utilizing the application latyer.
 *
 * References:
 *  http://www.zend.com/zend/spotlight/code-gallery-wade8.php
 * 
 * Due to the realities of PHP 5.0.5+ having a new and upsetting
 *  order of destruction [1] we have to force a session write at
 *  __destruct or there'll be no [db] class to use. Not really an
 *  issue for AMO live, but annoying for devs using 5+
 *  
 *  [1] http://bugs.php.net/bug.php?id=33772
 * 
 * @package amo
 * @subpackage lib
 */
class AMO_Session 
{
   /**
    * SQL Pear::DB wrapper
    * @var SQL
    */
   var $db;
   
   /**
    * The table in which sessions are stored 
    * @var string
    */
   var $dbTable = 'PHPSESSION';
   
   /**
    * Constructor
    * @param   $aDb        object  the database handle to use for storage 
    */
   function __construct(&$aDb)
   {
       $this->db = $aDb;
   }

   /**
    * Destructor
    * Required to force a flush of the session data to the database
    * before the class disappears.
    * @link http://bugs.php.net/bug.php?id=33772
    */
   function __destruct()
   {
       session_write_close();
   }

   /**
    * Prepare variables for use in SQL
    * @param   aArray  array   hash of variables to clean
    * @return  array           a new hash with SQL cleaned values, FALSE on failure
    * 
    * TODO: should this be here or should we extend SQL to handle hashes like 
    *       this since it happens so often?
    */
   function _prepareForSql($in)
   {
       /**
        * If we don't have a database connection then we
        * can't really continue
        */
       if (empty($this->db))
           return FALSE;
           
       $out = array();
       foreach ($in as $key => $value)
       {
           // TODO: when the SQL class gets it's own escapeSimple then
           //  change this to use that instead
           $out[$key] = $this->db->db->escapeSimple($value);
       }

       return $out;
   }

   /**
    * Open a new session 
    * 
    * @param   $aPath  string  a path to be used to store the session
    * @param   $aName  string  a filename to be used to store the session
    * @return  boolean         TRUE on success, FALSE otherwise
    */
   function _open($aPath, $aName)
   {
       /**
        * Return FALSE if we don't have a database to use 
        */

       return empty($this->db) == FALSE;
   }

   /**
    * Close a session
    * @return  boolean         TRUE on success, FALSE otherwise
    */
   function _close()
   {
       return TRUE;
   }
   
   /**
    * Read some data from a session
    * @param   $aSessId    string  a session Id
    * @return  string              the data, or an empty string on failure 
    */
   function _read($aSessId)
   {
       if (! strlen($aSessId))
           return '';

       /**
        * If for some reason we don't have a database handle, then
        * we can't really continue here
        */
       if (empty($this->db))
           return '';
       
       /**
        * Clean the arguments for SQL
        */
       $clean = $this->_prepareForSql(array('SessId' => $aSessId));
       
       /**
        * We only want the data, don't care about anything else
        */ 
       $sql = sprintf("SELECT SESSIONDATA FROM %s WHERE SESSIONID='%s'", $this->dbTable, $clean['SessId']);
       $data = '';
       
       if ($this->db->query($sql, SQL_INIT, SQL_ASSOC))
       {
           /**
            * Anything other than 1 row should indicate an error, so don't
            * propagate this but rather handle it as if there was no data
            * at all. 
            */
           if ($this->db->result->numRows() == 1)
           {
               $data = $this->db->record['SESSIONDATA'];
           }
           $this->db->result->free();
       }

       return $data;
   }
   
   /**
    * Write some data to the session
    * @param   $aSessId    string  a session Id
    * @param   $aSessData  string  the encoded [serialized?] session data to store 
    */
   function _write($aSessId, $aSessData)
   {
       /**
        * If for some reason we don't have a database handle, then
        * we can't really continue here
        */
       if (empty($this->db))
           return FALSE;

       /**
        * Clean the arguments for SQL
        */
       $clean = $this->_prepareForSql(array('SessId' => $aSessId, 'SessData' => $aSessData));

       /**
        * Due to the handy REPLACE [INTO] syntax, we don't care if there is already
        * a record there or not, this one statment will insert or replace as needed.
        * HOWEVER:
        * Should we ever try to use the session Id as a foreign key in the database 
        * then this will cause a referential constraint failure, as the REPLACE will
        * include a DELETE in cases where the record already exists. Additionally, if
        * we store anything other than the SESSIONDATA in the table which doesn't have
        * an automatic default then it will get lost between writes unless it is included
        * in the statement.
        */
       $sql = sprintf("REPLACE %s SET SESSIONID='%s', SESSIONDATA='%s'", $this->dbTable, $clean['SessId'], $clean['SessData']);
       return $this->db->query($sql, SQL_NONE);
   }
   
   /**
    * Destroy a session
    * @param   $aSessId    string  a session Id
    * @return  boolean             TRUE on success, FALSE otherwise
    */
   function _destroy($aSessId)
   {
       /**
        * If for some reason we don't have a database handle, then
        * we can't really continue here
        */
       if (empty($this->db))
           return FALSE;
           
       /**
        * Clean the arguments for SQL
        */
       $clean = $this->_prepareForSql(array('SessId' => $aSessId));
       
       $sql = sprintf("DELETE FROM %s WHERE SESSIONID='%s'", $this->dbTable, $clean['SessId']);
       
       return $this->db->db->query($sql);
   }
   
   /**
    * Garbage collector
    * @param   $aLifeTime  number  maximum age of a session to retain
    * @return  boolean             TRUE on success, FALSE otherwise
    */
   function _gc($sLifeTime)
   {
       return TRUE;
   }
}
?>
