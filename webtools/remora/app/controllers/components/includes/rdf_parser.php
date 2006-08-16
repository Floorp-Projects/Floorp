<?php
// ##################################################################################
// Title                     : Class Rdf_parser
// Version                   : 1.0
// Author                    : Jason Diammond -repat RDF parser-
//                           : Luis Argerich -PHP version of repat- (lrargerich@yahoo.com)
// Last modification date    : 06-13-2002
// Description               : A port to PHP of the Repat an RDF parser.
//                             This parser based on expat parses RDF files producing events
//                             proper of RDF documents.
// ##################################################################################
// History:
// 06-13-2002                : First version of this class.
// 07-17-2002                  Minor bugfix (Leandro Mariano Lopez)
// ##################################################################################
// To-Dos:
//
// ##################################################################################
// How to use it:
// Read the documentation in rdf_parser.html
// ##################################################################################

if(defined("_class_rdf_is_included")) {
  // do nothing since the class is already included
} else {
  define("_class_rdf_is_included",1);

/* First we define some constants */
define("XML_NAMESPACE_URI","http://www.w3.org/XML/1998/namespace" );
define("XML_LANG","lang");
define("RDF_NAMESPACE_URI","http://www.w3.org/1999/02/22-rdf-syntax-ns#" );
define("RDF_RDF","RDF");
define("RDF_DESCRIPTION","Description");
define("RDF_ID","ID");
define("RDF_ABOUT","about");
define("RDF_ABOUT_EACH","aboutEach");
define("RDF_ABOUT_EACH_PREFIX","aboutEachPrefix");
define("RDF_BAG_ID","bagID");
define("RDF_RESOURCE","resource");
define("RDF_VALUE","value");
define("RDF_PARSE_TYPE","parseType");
define("RDF_PARSE_TYPE_LITERAL","Literal");
define("RDF_PARSE_TYPE_RESOURCE","Resource");
define("RDF_TYPE","type");
define("RDF_BAG","Bag");
define("RDF_SEQ","Seq");
define("RDF_ALT","Alt");
define("RDF_LI","li");
define("RDF_STATEMENT","Statement");
define("RDF_SUBJECT","subject");
define("RDF_PREDICATE","predicate");
define("RDF_OBJECT","object");

define("NAMESPACE_SEPARATOR_CHAR",'^');
define("NAMESPACE_SEPARATOR_STRING","^");
//define("FALSE",0);
//define("TRUE",1);
define("IN_TOP_LEVEL",0);
define("IN_RDF",1);
define("IN_DESCRIPTION",2);
define("IN_PROPERTY_UNKNOWN_OBJECT",3);
define("IN_PROPERTY_RESOURCE",4);
define("IN_PROPERTY_EMPTY_RESOURCE",5);
define("IN_PROPERTY_LITERAL",6);
define("IN_PROPERTY_PARSE_TYPE_LITERAL",7);
define("IN_PROPERTY_PARSE_TYPE_RESOURCE",8);
define("IN_XML",9);
define("IN_UNKNOWN",10);
//define("IN_PROPERTY_PARSE_TYPE_LITERAL",9);

define("RDF_SUBJECT_TYPE_URI",0);
define("RDF_SUBJECT_TYPE_DISTRIBUTED",1);
define("RDF_SUBJECT_TYPE_PREFIX",2);
define("RDF_SUBJECT_TYPE_ANONYMOUS",3);

define("RDF_OBJECT_TYPE_RESOURCE",0);
define("RDF_OBJECT_TYPE_LITERAL",1);
define("RDF_OBJECT_TYPE_XML",2);

class Rdf_parser {

var $rdf_parser;

/* Private Methods */

function _new_element()
{
    $e["parent"]=Array();  // Parent is a blank Array
    //$this->clear_element($e["parent"]);
    $e["state"]=0;
    $e["has_property_atributes"]=0;
    $e["has_member_attributes"]=0;
    $e["subject_type"]=0;
    $e["subject"]='';
    $e["predicate"]='';
    $e["ordinal"]=0;
    $e["members"]=0;
    $e["data"]='';
    $e["xml_lang"]='';
    $e["bag_id"]='';
    $e["statements"]=0;
    $e["statement_id"]='';

    return $e;
}

function _copy_element($source, &$destination )
{
    if( $source )
    {
        $destination["parent"] = $source;
        $destination["state"] = $source["state"];
        $destination["xml_lang"] = $source["xml_lang"];
    }
}

function _clear_element(&$e)
{
        $e["subject"]='';
        $e["predicate"]='';
        $e["data"]='';
        $e["bag_id"]='';
        $e["statement_id"]='';

        if(isset($e["parent"])) {
          if( $e["parent"] )
          {
              if( $e["parent"]["xml_lang"] != $e["xml_lang"] )
              {
                  $e["xml_lang"]='';
              }
          }
          else
          {
              $e["xml_lang"]='';
          }
        } else {
            $e["xml_lang"]='';
        }
        //memset( e, 0, strlen( _rdf_element ) );
        $e["parent"]=Array();
        $e["state"]=0;
        $e["has_property_attributes"]=0;
        $e["has_member_attributes"]=0;
        $e["subject_type"]=0;
        $e["subject"]='';
        $e["predicate"]='';
        $e["ordinal"]=0;
        $e["members"]=0;
        $e["data"]='';
        $e["xml_lang"]='';
        $e["bag_id"]='';
        $e["statements"]=0;
        $e["statement_id"]='';

}

function _push_element()
{
    if(!isset($this->rdf_parser["free"])) {
        $this->rdf_parser["free"]=Array();
    }
    if(count($this->rdf_parser["free"])>0)
    {
        $e = $this->rdf_parser["free"];
        if(isset($e["parent"])) {
          $this->rdf_parser["free"] = $e["parent"];
        } else {
          $this->rdf_parser["free"]=$this->_new_element();
        }
    }
    else
    {
        $e = $this->_new_element();
    }
    if(!isset($this->rdf_parser["top"])) {
      $this->rdf_parser["top"]=Array();
    }
    $this->_copy_element( $this->rdf_parser["top"], $e );
    $this->rdf_parser["top"] = $e;
}


function _pop_element()
{
    $e = $this->rdf_parser["top"];
    $this->rdf_parser["top"] = $e["parent"];
    $this->_clear_element( $e );
    /*
    if(isset($this->rdf_parser["free"])) {
      $e["parent"] = $this->rdf_parser["free"];
    } else {
      $e["parent"]=0;
    }
    */
    $this->rdf_parser["free"] = $e;
}

function _delete_elements()
{
}


function _is_rdf_property_attribute_resource($local_name )
{
    return ( $local_name == RDF_TYPE );
}

function _is_rdf_property_attribute_literal($local_name )
{
    return ( $local_name == RDF_VALUE );
}

function _is_rdf_ordinal( $local_name )
{
    $ordinal = -1;

    //if($local_name{0}=='_')
    if( $local_name{0} ==  '_'  )
    {
        $ordinal =  substr($local_name,1) + 1 ;
    }

    return ( $ordinal > 0 ) ? $ordinal : 0;
}

function _is_rdf_property_attribute( $local_name )
{
    return $this->_is_rdf_property_attribute_resource( $local_name )
        || $this->_is_rdf_property_attribute_literal( $local_name );
}

function _is_rdf_property_element( $local_name )
{
    return (  $local_name == RDF_TYPE  )
        || (  $local_name == RDF_SUBJECT )
        || (  $local_name == RDF_PREDICATE )
        || (  $local_name == RDF_OBJECT )
        || (  $local_name == RDF_VALUE )
        || (  $local_name == RDF_LI )
        || (  $local_name{0} == '_'  );
}

function _istalnum($val) {
  return ereg("[A-Za-z0-9]",$val);
}

function _istalpha($val) {
  return ereg("[A-Za-z]",$val);
}


function _is_absolute_uri($uri )
{
    $result = false;
        $uri_p=0;
    if( $uri && $this->_istalpha( $uri{$uri_p} ) )
    {
        ++$uri_p;

        while( ($uri_p<strlen($uri))
            && ( $this->_istalnum( $uri{$uri_p} )
                || ( $uri{$uri_p} ==  '+'  )
                || ( $uri{$uri_p} == '-'  )
                || ( $uri{$uri_p} ==  '.'  ) ) )
        {
                ++$uri_p;
        }

        $result = ( $uri{$uri_p} == ':'  );
    }
    return $result;
}


/*
This function returns an associative array returning any of the various components of the URL that are present. This includes the
$arr=parse_url($url)
scheme - e.g. http
host
port
user
pass
path
query - after the question mark ?
fragment - after the hashmark #
*/
function _parse_uri($uri,$buffer,$len,&$scheme,&$authority,&$path,&$query,&$fragment ) {
  $parsed=parse_url($uri);
  if(isset($parsed["scheme"])) {
    $scheme=$parsed["scheme"];} else {
    $scheme='';
  }
  if(isset($parsed["host"])) {
    $host=$parsed["host"];} else {
    $host='';
  }
  if(isset($parsed["host"])) {
    $authority=$parsed["host"];} else {
    $authority='';
  }
  if(isset($parsed["path"])) {
    $path=$parsed["path"];} else {
    $path='';
  }
  if(isset($parsed["query"])) {
    $query=$parsed["query"];} else {
    $query='';
  }
  if(isset($parsed["fragment"])) {
    $fragment=$parsed["fragment"];} else {
    $fragment='';
  }

}


function _resolve_uri_reference($base_uri,$reference_uri,&$buffer,$length )
{
    $base_buffer='';
    $reference_buffer='';
    $path_buffer='';

    $buffer = '';

    $this->_parse_uri($reference_uri,$reference_buffer,strlen( $reference_buffer ),$reference_scheme,$reference_authority,
        $reference_path,$reference_query,$reference_fragment );

    if( $reference_scheme == ''
        && $reference_authority == ''
        && $reference_path == ''
        && $reference_query == '' )
    {
        $buffer=$base_uri;

        if( $reference_fragment != '' )
        {
            $buffer.= "#" ;
            $buffer.=$reference_fragment;
        }
    }
    else if( $reference_scheme != '' )
    {
        $buffer=$reference_uri;
    }
    else
    {
        $this->_parse_uri(
            $base_uri,
            $base_buffer,
            strlen( $base_buffer ),
            $base_scheme,
            $base_authority,
            $base_path,
            $base_query,
            $base_fragment );

        $result_scheme = $base_scheme;

        if( $reference_authority != '' )
        {
            $result_authority = $reference_authority;
        }
        else
        {
            $result_authority = $base_authority;

            if( $reference_path != ''
                && ( ($reference_path{0} == '/')
                    || ($reference_path{0} ==  '\\')  ) )
            {
                $result_path = $reference_path;
            }
            else
            {
                $p = '';

                $result_path = $path_buffer;

                $path_buffer='';

                $p = strstr( $base_path,  '/'  );

                if( !$p )
                {
                    $p = strstr( $base_path, '\\'  );
                }

                if( $p )
                {

                    $path_buffer.=$base_path;

                    //while( s <= p )
                    //{
                    //  *d++ = *s++;
                    //}

                    //*d++ = 0;

                }

                if( $reference_path != '' )
                {
                    $path_buffer.=$reference_path;
                }

                {
                    //remove all occurrences of "./"
                    //print($path_buffer);
                    $path_buffer=preg_replace("/\/\.\//","/",$path_buffer);
                    $path_buffer=preg_replace("/\/([^\/\.])*\/..$/","/",$path_buffer);
                    while(preg_match("/\.\./",$path_buffer)) {
                      $path_buffer=preg_replace("/\/([^\/\.]*)\/..\//","/",$path_buffer);
                    }
                    $path_buffer=preg_replace("/\.$/","",$path_buffer);

                }

            }
        }

        // This replaces the C pointer assignament

        $result_path = $path_buffer;
        if( $result_scheme != '' )
        {
            $buffer=$result_scheme;
            $buffer.=":";
        }

        if( $result_authority != '' )
        {
            $buffer.="//";
            $buffer.=$result_authority;
        }

        if( $result_path != '' )
        {

            $buffer.=$result_path;
        }

        if( $reference_query != '' )
        {
            $buffer.="?";
            $buffer.=$reference_query;
        }

        if( $reference_fragment != '' )
        {
            $buffer.="#";
            $buffer.=$reference_fragment;
        }
    }
}



function is_valid_id($id )
{
    $result = false;
    $p = $id;
    $p_p=0;

    if( $id != '' )
    {
        if( $this->_istalpha( $p )
            || $p{0} == '_'
            || $p{0} == ':'  )
        {
            $result = true;

            while( $result != false && ( $p{++$p_p}  != 0 ) )
            {
                if( ! ( $this->_istalnum( $p{$p_p} )
                        || $p{$p_p} == '.'
                        || $p{$p_p} == '-'
                        || $p{$p_p} == '_'
                        || $p{$p_p} == ':'  ) )
                {
                    $result = false;
                }
            }
        }
    }

    return $result;
}

function _resolve_id($id,&$buffer,$length )
{
    $id_buffer='';

    if( $this->is_valid_id( $id ) == true )
    {
        $id_buffer="#$id";
    }
    else
    {
        $this->report_warning( "bad ID attribute: ".$id_buffer."#_bad_ID_attribute_");
    }

    $this->_resolve_uri_reference( $this->rdf_parser["base_uri"], $id_buffer, $buffer, $length );
}

function _split_name($name, &$buffer, $len,&$namespace_uri, &$local_name )
{

    static $nul = 0;
    $buffer=$name;




        if(  strstr( $buffer, NAMESPACE_SEPARATOR_CHAR ) )
        {
            $cosas=explode(NAMESPACE_SEPARATOR_CHAR,$buffer);
            $namespace_uri = $cosas[0];
            $local_name = $cosas[1];
        }
        else
        {
            if( ( $buffer{ 0 } ==  'x'  )
                && ( $buffer{ 1 } ==  'm'  )
                && ( $buffer{ 2 } ==  'l'  )
                && ( $buffer{ 3 } ==  ':'  ) )
            {
                $namespace_uri = XML_NAMESPACE_URI;
                $local_name = substr($buffer,4);
            }
            else
            {
                $namespace_uri = '';
                $local_name = $buffer;
            }
        }


}

function _generate_anonymous_uri(&$buf, $len )
{
    $id='';
    if(!isset($this->rdf_parser["anonymous_id"])) {
      $this->rdf_parser["anonymous_id"]=0;
    }
    $this->rdf_parser["anonymous_id"]++;

    $id="#genid".$this->rdf_parser["anonymous_id"];
    $this->_resolve_uri_reference( $this->rdf_parser["base_uri"], $id, $buf, $len );

}

function _report_statement( $subject_type, $subject, $predicate, $ordinal, $object_type,  $object, $xml_lang, $bag_id, $statements, $statement_id )
{
    $statement_id_type = RDF_SUBJECT_TYPE_URI;
    $statement_id_buffer='';
    $predicate_buffer='';

    if( $this->rdf_parser["statement_handler"] )
    {
        $this->rdf_parser["statement_handler"]($this->rdf_parser["user_data"],$subject_type,$subject,$predicate,$ordinal,$object_type,$object,$xml_lang );

        if( $bag_id )
        {
            if( $statements == '' )
            {
                $this->_report_statement(RDF_SUBJECT_TYPE_URI,
                    $bag_id,
                    RDF_NAMESPACE_URI.RDF_TYPE,
                    0,
                    RDF_OBJECT_TYPE_RESOURCE,
                    RDF_NAMESPACE_URI.RDF_BAG,
                    '',
                    '',
                    '',
                    '' );
            }

            if( ! $statement_id )
            {
                $statement_id_type = RDF_SUBJECT_TYPE_ANONYMOUS;
                $this->_generate_anonymous_uri(
                    $statement_id_buffer,
                    strlen( $statement_id_buffer ) );
                $statement_id = $statement_id_buffer;
            }
                        $statements++;
            $predicate_buffer="RDF_NAMESPACE_URI_".$statements;

            $this->_report_statement(
                RDF_SUBJECT_TYPE_URI,
                $bag_id,
                $predicate_buffer,
                $statements,
                RDF_OBJECT_TYPE_RESOURCE,
                $statement_id,
                '',
                '',
                '',
                '' );
        }

        if( $statement_id )
        {
            // rdf:type = rdf:Statement
            $this->_report_statement(
                $statement_id_type,
                $statement_id,
                RDF_NAMESPACE_URI.RDF_TYPE,
                0,
                RDF_OBJECT_TYPE_RESOURCE,
                RDF_NAMESPACE_URI.RDF_STATEMENT,
                '',
                '',
                '',
                '' );

            // rdf:subject
            $this->_report_statement(
                $statement_id_type,
                $statement_id,
                RDF_NAMESPACE_URI.RDF_SUBJECT,
                0,
                RDF_OBJECT_TYPE_RESOURCE,
                $subject,
                '',
                '',
                '',
                '' );

            // rdf:predicate
            $this->_report_statement(
                $statement_id_type,
                $statement_id,
                RDF_NAMESPACE_URI.RDF_PREDICATE,
                0,
                RDF_OBJECT_TYPE_RESOURCE,
                $predicate,
                '',
                '',
                '',
                '' );

            // rdf:object
            $this->_report_statement(
                $statement_id_type,
                $statement_id,
                RDF_NAMESPACE_URI.RDF_OBJECT,
                0,
                $object_type,
                $object,
                '',
                '',
                '',
                '' );
        }
    }
}

function _report_start_parse_type_literal()
{
    if( $this->rdf_parser["start_parse_type_literal_handler"] )
    {
         $this->rdf_parser["start_parse_type_literal_handler"](
            $this->rdf_parser["user_data"] );
    }
}

function _report_end_parse_type_literal()
{
    if( $this->rdf_parser["end_parse_type_literal_handler"] )
    {
        $this->rdf_parser["end_parse_type_literal_handler"](
            $this->rdf_parser["user_data"] );
    }
}

function _handle_property_attributes($subject_type, $subject, $attributes, $xml_lang, $bag_id, $statements )
{
    $i=0;

    $attribute='';
    $predicate='';

    $attribute_namespace_uri='';
    $attribute_local_name='';
    $attribute_value='';

    $ordinal=0;

    for( $i = 0; isset($attributes[ $i ]); $i += 2 )
    {
        $this->_split_name(
            $attributes[ $i ],
            $attribute,
            strlen( $attribute ),
            $attribute_namespace_uri,
            $attribute_local_name );

        $attribute_value = $attributes[ $i + 1 ];

        $predicate=$attribute_namespace_uri;
        $predicate.=$attribute_local_name;

        if( RDF_NAMESPACE_URI == $attribute_namespace_uri )
        {
            if( $this->_is_rdf_property_attribute_literal( $attribute_local_name ) )
            {
                $this->_report_statement(
                    $subject_type,
                    $subject,
                    $predicate,
                    0,
                    RDF_OBJECT_TYPE_LITERAL,
                    $attribute_value,
                    $xml_lang,
                    $bag_id,
                    $statements,
                    '' );
            }
            else if( $this->_is_rdf_property_attribute_resource( $attribute_local_name ) )
            {
                $this->_report_statement(
                    $subject_type,
                    $subject,
                    $predicate,
                    0,
                    RDF_OBJECT_TYPE_RESOURCE,
                    $attribute_value,
                    '',
                    $bag_id,
                    $statements,
                    '' );
            }
            else if( ( $ordinal = $this->_is_rdf_ordinal( $attribute_local_name ) ) != 0 )
            {
                $this->_report_statement(
                    $subject_type,
                    $subject,
                    $predicate,
                    $ordinal,
                    RDF_OBJECT_TYPE_LITERAL,
                    $attribute_value,
                    $xml_lang,
                    $bag_id,
                    $statements,
                    '' );
            }
        }
        else if( XML_NAMESPACE_URI == $attribute_namespace_uri )
        {
            //do nothing
        }
        else if( $attribute_namespace_uri )
        {
            // is it required that property attributes be in an explicit namespace?

            $this->_report_statement(
                $subject_type,
                $subject,
                $predicate,
                0,
                RDF_OBJECT_TYPE_LITERAL,
                $attribute_value,
                $xml_lang,
                $bag_id,
                $statements,
                '' );
        }
    }
}

function _report_start_element(  $name, $attributes )
{
    if( isset($this->rdf_parser["start_element_handler"]) )
    {
         $this->rdf_parser["start_element_handler"](
            $this->rdf_parser["user_data"],
            $name,
            $attributes );
    }
}

function _report_end_element( $name )
{
    if( isset($this->rdf_parser["end_element_handler"]) )
    {
        $this->rdf_parser["end_element_handler"](
            $this->rdf_parser["user_data"],
            $name );
    }
}

function _report_character_data($s,$len )
{
    if( isset($this->rdf_parser["character_data_handler"]) )
    {
        $this->rdf_parser["character_data_handler"](
            $this->rdf_parser["user_data"],
            $s,
            $len );
    }
}

function _report_warning( $warning)
{

    // rdf_parser->top->state = IN_UNKNOWN;

    if( isset($this->rdf_parser["warning_handler"]) )
    {
            $this->rdf_parser["warning_handler"] (
                $warning);
    }
}

function _handle_resource_element( $namespace_uri, $local_name, $attributes, $parent )
{
    $subjects_found = 0;
    $aux=$attributes;
    $aux2=Array();
    foreach($attributes as $atkey=>$atvalue) {
      $aux2[]=$atkey;
      $aux2[]=$atvalue;
    }
    $attributes=$aux2;
    $id = '';
    $about = '';
    $about_each = '';
    $about_each_prefix = '';

    $bag_id = '';

    $i=0;

    $attribute='';

    $attribute_namespace_uri='';
    $attribute_local_name='';
    $attribute_value='';

    $id_buffer='';

    $type='';

    $this->rdf_parser["top"]["has_property_attributes"] = false;
    $this->rdf_parser["top"]["has_member_attributes"] = false;

    // examine each attribute for the standard RDF "keywords"
    for( $i = 0; isset($attributes[$i]); $i += 2 )
    {
        $this->_split_name(
            $attributes[ $i ],
            $attribute,
            strlen( $attribute ),
            $attribute_namespace_uri,
            $attribute_local_name );

        $attribute_value = $attributes[ $i + 1 ];

        // if the attribute is not in any namespace
        //   or the attribute is in the RDF namespace
        if( ( $attribute_namespace_uri == '' )
            || (  $attribute_namespace_uri == RDF_NAMESPACE_URI  ))
        {
            if( $attribute_local_name == RDF_ID )
            {
                $id = $attribute_value;
                ++$subjects_found;
            }
            else if( $attribute_local_name == RDF_ABOUT )
            {
                $about = $attribute_value;
                ++$subjects_found;
            }
            else if(  $attribute_local_name == RDF_ABOUT_EACH )
            {
                $about_each = $attribute_value;
                ++$subjects_found;
            }
            else if( $attribute_local_name == RDF_ABOUT_EACH_PREFIX )
            {
                $about_each_prefix = $attribute_value;
                ++$subjects_found;
            }
            else if( $attribute_local_name == RDF_BAG_ID)
            {
                $bag_id = $attribute_value;
            }
            else if( $this->_is_rdf_property_attribute( $attribute_local_name ) )
            {
                $this->rdf_parser["top"]["has_property_attributes"] = true;
            }
            else if( $this->_is_rdf_ordinal( $attribute_local_name ) )
            {
                $this->rdf_parser["top"]["has_property_attributes"] = true;
                $this->rdf_parser["top"]["has_member_attributes"] = true;
            }
            else
            {
                $this->_report_warning(
                    "unknown or out of context rdf attribute:".$attribute_local_name );
            }
        }
        else if(  $attribute_namespace_uri == XML_NAMESPACE_URI )
        {
            if( $attribute_local_name == XML_LANG )
            {
                $this->rdf_parser["top"]["xml_lang"] = $attribute_value;
            }
        }
        else if( $attribute_namespace_uri )
        {
            $this->rdf_parser["top"]["has_property_attributes"] = true;
        }
    }

    // if no subjects were found, generate one.
    if( $subjects_found == 0 )
    {
        $this->_generate_anonymous_uri(  $id_buffer, strlen( $id_buffer ) );
        $this->rdf_parser["top"]["subject"]=$id_buffer;
        $this->rdf_parser["top"]["subject_type"] = RDF_SUBJECT_TYPE_ANONYMOUS;
    }
    else if( $subjects_found > 1 )
    {
        $this->_report_warning(
            "ID, about, aboutEach, and aboutEachPrefix are mutually exclusive" );
        return;
    }
    else if( $id )
    {
        $this->_resolve_id( $id, $id_buffer, strlen( $id_buffer ) );
        $this->rdf_parser["top"]["subject_type"] = RDF_SUBJECT_TYPE_URI;
        $this->rdf_parser["top"]["subject"]=$id_buffer;
    }
    else if( $about )
    {
    $this->_resolve_uri_reference( $this->rdf_parser["base_uri"], $about, $id_buffer, strlen( $id_buffer ) );
        $this->rdf_parser["top"]["subject_type"] = RDF_SUBJECT_TYPE_URI;
        $this->rdf_parser["top"]["subject"]=$id_buffer;
    }
    else if( $about_each )
    {
        $this->rdf_parser["top"]["subject_type"] = RDF_SUBJECT_TYPE_DISTRIBUTED;
        $this->rdf_parser["top"]["subject"]=$about_each;
    }
    else if( $about_each_prefix )
    {
        $this->rdf_parser["top"]["subject_type"] = RDF_SUBJECT_TYPE_PREFIX;
        $this->rdf_parser["top"]["subject"]=$about_each_prefix;
    }

    // if the subject is empty, assign it the document uri
    if( $this->rdf_parser["top"]["subject"] == '' )
    {
        $len = 0;

        $this->rdf_parser["top"]["subject"]=$this->rdf_parser["base_uri"];

        // now remove the trailing '#'

        $len = strlen( $this->rdf_parser["top"]["subject"]);

        if( $len > 0 )
        {
            //$rdf_parser["top"]["subject"][" len - 1 "] = 0;
        }
    }

    if( $bag_id )
    {
        $this->_resolve_id(  $bag_id, $id_buffer, strlen( $id_buffer ) );
        $this->rdf_parser["top"]["bag_id"]=$id_buffer;
    }

    // only report the type for non-rdf:Description elements.
    if( ($local_name != RDF_DESCRIPTION )
        || ( $namespace_uri != RDF_NAMESPACE_URI ) )
    {
        $type=$namespace_uri;
        $type.=$local_name;

        $this->_report_statement(
            $this->rdf_parser["top"]["subject_type"],
            $this->rdf_parser["top"]["subject"],
            RDF_NAMESPACE_URI.RDF_TYPE,
            0,
            RDF_OBJECT_TYPE_RESOURCE,
            $type,
            '',
            $this->rdf_parser["top"]["bag_id"],
            $this->rdf_parser["top"]["statements"],
            '' );

    }

    // if this element is the child of some property,
    //   report the appropriate statement.
    if( $parent )
    {
        $this->_report_statement(
            $parent["parent"]["subject_type"],
            $parent["parent"]["subject"],
            $parent["predicate"],
            $parent["ordinal"],
            RDF_OBJECT_TYPE_RESOURCE,
            $this->rdf_parser["top"]["subject"],
            '',
            $parent["parent"]["bag_id"],
            $parent["parent"]["statements"],
            $parent["statement_id"] );

    }

    if( $this->rdf_parser["top"]["has_property_attributes"] )
    {
        $this->_handle_property_attributes(
            $this->rdf_parser["top"]["subject_type"],
            $this->rdf_parser["top"]["subject"],
            $attributes,
            $this->rdf_parser["top"]["xml_lang"],
            $this->rdf_parser["top"]["bag_id"],
            $this->rdf_parser["top"]["statements"] );
    }
}

function _handle_property_element( &$namespace_uri, &$local_name, &$attributes )
{
    $buffer='';

    $i=0;

    $aux=$attributes;
    $aux2=Array();
    foreach($attributes as $atkey=>$atvalue) {
      $aux2[]=$atkey;
      $aux2[]=$atvalue;
    }
    $attributes=$aux2;

    $attribute_namespace_uri='';
    $attribute_local_name='';
    $attribute_value = '';

    $resource = '';
    $statement_id = '';
    $bag_id = '';
    $parse_type = '';

    $this->rdf_parser["top"]["ordinal"] = 0;

    if( $namespace_uri == RDF_NAMESPACE_URI )
    {
        if( ($this->rdf_parser["top"]["ordinal"] = ( $this->_is_rdf_ordinal( $local_name ) ) != 0 ) )
        {
            if( $this->rdf_parser["top"]["ordinal"] > $this->rdf_parser["top"]["parent"]["members"] )
            {
                $this->rdf_parser["top"]["parent"]["members"] = $this->rdf_parser["top"]["ordinal"];
            }
        }
        else if( ! $this->_is_rdf_property_element( $local_name ) )
        {
            $this->_report_warning(
                        "unknown or out of context rdf property element: ".$local_name );
            return;
        }
    }

    $buffer=$namespace_uri;

    if( ( $namespace_uri == RDF_NAMESPACE_URI )
        &&  ( $local_name == RDF_LI ) )
    {
        //$ordinal='';
        $this->rdf_parser["top"]["parent"]["members"]++;
        $this->rdf_parser["top"]["ordinal"] = $this->rdf_parser["top"]["parent"]["members"];


        $this->rdf_parser["top"]["ordinal"]=$this->rdf_parser["top"]["ordinal"];
        //$ordinal{ 0 } =  '_' ;

        $buffer.='_'.$this->rdf_parser["top"]["ordinal"];
    }
    else
    {
        $buffer.=$local_name;
    }

    $this->rdf_parser["top"]["predicate"]=$buffer;

    $this->rdf_parser["top"]["has_property_attributes"] = false;
    $this->rdf_parser["top"]["has_member_attributes"] = false;

    for( $i = 0; isset($attributes[$i]); $i += 2 )
    {
        $this->_split_name(
            $attributes[$i],
            $buffer,
            strlen( $buffer ),
            $attribute_namespace_uri,
            $attribute_local_name );

        $attribute_value = $attributes[$i + 1];

        // if the attribute is not in any namespace
        //   or the attribute is in the RDF namespace
        if( ( $attribute_namespace_uri == '' )
            || (  $attribute_namespace_uri == RDF_NAMESPACE_URI ) )
        {
            if( ( $attribute_local_name == RDF_ID )  )
            {
                $statement_id = $attribute_value;
            }
            else if( $attribute_local_name == RDF_PARSE_TYPE )
            {
                $parse_type = $attribute_value;
            }
            else if(  $attribute_local_name == RDF_RESOURCE )
            {
                $resource = $attribute_value;
            }
            else if(  $attribute_local_name == RDF_BAG_ID )
            {
                $bag_id = $attribute_value;
            }
            else if( $this->_is_rdf_property_attribute( $attribute_local_name ) )
            {
                $this->rdf_parser["top"]["has_property_attributes"] = true;
            }
            else
            {
                $this->_report_warning(
                                        "unknown rdf attribute: ".$attribute_local_name );
                return;
            }
        }
        else if(  $attribute_namespace_uri == XML_NAMESPACE_URI  )
        {
            if( $attribute_local_name == XML_LANG  )
            {
                $this->rdf_parser["top"]["xml_lang"] = $attribute_value;
            }
        }
        else if( $attribute_namespace_uri )
        {
            $this->rdf_parser["top"]["has_property_attributes"] = true;
        }
    }

    // this isn't allowed by the M&S but I think it should be
    if( $statement_id && $resource )
    {
        $this->_report_warning(
                        "rdf:ID and rdf:resource are mutually exclusive" );
        return;
    }

    if( $statement_id )
    {
    $this->_resolve_id($statement_id, $buffer, strlen( $buffer ) );
        $this->rdf_parser["top"]["statement_id"]=$buffer;
    }

    if( $parse_type )
    {
        if( $resource )
        {
            $this->_report_warning(
                                "property elements with rdf:parseType do not allow rdf:resource" );
            return;
        }

        if( $bag_id )
        {
            $this->_report_warning(
                                "property elements with rdf:parseType do not allow rdf:bagID" );
            return;
        }

        if( $this->rdf_parser["top"]["has_property_attributes"] )
        {
            $this->_report_warning(
                                "property elements with rdf:parseType do not allow property attributes");
            return;
        }

        if(  $attribute_value == RDF_PARSE_TYPE_RESOURCE )
        {
            $this->_generate_anonymous_uri( $buffer, strlen( $buffer ) );

            // since we are sure that this is now a resource property we can report it
            $this->_report_statement(
                $this->rdf_parser["top"]["parent"]["subject_type"],
                $this->rdf_parser["top"]["parent"]["subject"],
                $this->rdf_parser["top"]["predicate"],
                0,
                RDF_OBJECT_TYPE_RESOURCE,
                $buffer,
                '',
                $this->rdf_parser["top"]["parent"]["bag_id"],
                $this->rdf_parser["top"]["parent"]["statements"],
                $statement_id );

            $this->_push_element( );

            $this->rdf_parser["top"]["state"] = IN_PROPERTY_PARSE_TYPE_RESOURCE;
            $this->rdf_parser["top"]["subject_type"] = RDF_SUBJECT_TYPE_ANONYMOUS;
            $this->rdf_parser["top"]["subject"]=$buffer;
            $this->rdf_parser["top"]["bag_id"]='';
        }
        else
        {
            $this->_report_statement(
                $this->rdf_parser["top"]["parent"]["subject_type"],
                $this->rdf_parser["top"]["parent"]["subject"],
                $this->rdf_parser["top"]["predicate"],
                0,
                RDF_OBJECT_TYPE_XML,
                '',
                '',
                $this->rdf_parser["top"]["parent"]["bag_id"],
                $this->rdf_parser["top"]["parent"]["statements"],
                $statement_id );

            $this->rdf_parser["top"]["state"] = IN_PROPERTY_PARSE_TYPE_LITERAL;
            $this->_report_start_parse_type_literal();
        }
    }
    else if( $resource || $bag_id || $this->rdf_parser["top"]["has_property_attributes"] )
    {


        if( $resource != '' )
        {
            $subject_type = RDF_SUBJECT_TYPE_URI;
            $this->_resolve_uri_reference( $this->rdf_parser["base_uri"], $resource, $buffer, strlen( $buffer ) );
        }
        else
        {
            $subject_type = RDF_SUBJECT_TYPE_ANONYMOUS;
            $this->_generate_anonymous_uri( buffer, strlen( $buffer ) );
        }

        $this->rdf_parser["top"]["state"] = IN_PROPERTY_EMPTY_RESOURCE;

        // since we are sure that this is now a resource property we can report it.
        $this->_report_statement(
            $this->rdf_parser["top"]["parent"]["subject_type"],
            $this->rdf_parser["top"]["parent"]["subject"],
            $this->rdf_parser["top"]["predicate"],
            $this->rdf_parser["top"]["ordinal"],
            RDF_OBJECT_TYPE_RESOURCE,
            $buffer,
            '',
            $this->rdf_parser["top"]["parent"]["bag_id"],
            $this->rdf_parser["top"]["parent"]["statements"],
            '' ); // should we allow IDs?

        if( $bag_id )
        {
            $this->_resolve_id( $bag_id, $buffer, strlen( $buffer ) );
            $this->rdf_parser["top"]["bag_id"]=$buffer;
        }

        if( $this->rdf_parser["top"]["has_property_attributes"] )
        {
            $this->_handle_property_attributes(
                $subject_type,
                $buffer,
                $attributes,
                $this->rdf_parser["top"]["xml_lang"],
                $this->rdf_parser["top"]["bag_id"],
                $this->rdf_parser["top"]["statements"] );
        }
    }
}


function _start_element_handler($parser, $name, $attributes )
{


    $buffer='';

    $namespace_uri='';
    $local_name='';

/*
    if( rdf_parser->top != '' && rdf_parser->top->state != IN_TOP_LEVEL )
    {
        ++rdf_parser->anonymous_id;
    }
*/

    $this->_push_element();


    $this->_split_name(
        $name,
        $buffer,
        strlen( $buffer ),
        $namespace_uri,
        $local_name );

    switch( $this->rdf_parser["top"]["state"] )
    {
    case IN_TOP_LEVEL:
        if( RDF_NAMESPACE_URI.NAMESPACE_SEPARATOR_STRING.RDF_RDF == $name )
        {
            $this->rdf_parser["top"]["state"] = IN_RDF;
        }
        else
        {
            $this->_report_start_element( $name, $attributes );
        }
        break;
    case IN_RDF:
        $this->rdf_parser["top"]["state"] = IN_DESCRIPTION;
        $this->_handle_resource_element( $namespace_uri, $local_name, $attributes, '' );
        break;
    case IN_DESCRIPTION:
    case IN_PROPERTY_PARSE_TYPE_RESOURCE:
        $this->rdf_parser["top"]["state"] = IN_PROPERTY_UNKNOWN_OBJECT;
        $this->_handle_property_element( $namespace_uri, $local_name, $attributes );
        break;
    case IN_PROPERTY_UNKNOWN_OBJECT:
        /* if we're in a property with an unknown object type and we encounter
           an element, the object must be a resource, */
        $this->rdf_parser["top"]["data"]='';
        $this->rdf_parser["top"]["parent"]["state"] = IN_PROPERTY_RESOURCE;
        $this->rdf_parser["top"]["state"] = IN_DESCRIPTION;
        $this->_handle_resource_element(
            $namespace_uri,
            $local_name,
            $attributes,
            $this->rdf_parser["top"]["parent"] );
        break;
    case IN_PROPERTY_LITERAL:
        $this->_report_warning( "no markup allowed in literals" );
        break;
    case IN_PROPERTY_PARSE_TYPE_LITERAL:
        $this->rdf_parser["top"]["state"] = IN_XML;
        /* fall through */
    case IN_XML:
        $this->_report_start_element( $name, $attributes );
        break;
    case IN_PROPERTY_RESOURCE:
        $this->_report_warning(
                     "only one element allowed inside a property element" );
        break;
    case IN_PROPERTY_EMPTY_RESOURCE:
        $this->_report_warning(
                    "no content allowed in property with rdf:resource, rdf:bagID, or property attributes" );
        break;
    case IN_UNKNOWN:
        break;
    }
}

/*
    this is only called when we're in the IN_PROPERTY_UNKNOWN_OBJECT state.
    the only time we won't know what type of object a statement has is
    when we encounter property statements without property attributes or
    content:

        <foo:property />
        <foo:property ></foo:property>
        <foo:property>    </foo:property>

    notice that the state doesn't switch to IN_PROPERTY_LITERAL when
    there is only whitespace between the start and end tags. this isn't
    a very useful statement since the object is anonymous and can't
    have any statements with it as the subject but it is allowed.
*/

function _end_empty_resource_property()
{
    $buffer='';

    $this->_generate_anonymous_uri($buffer, strlen( $buffer ) );

    $this->_report_statement(
        $this->rdf_parser["top"]["parent"]["subject_type"],
        $this->rdf_parser["top"]["parent"]["subject"],
        $this->rdf_parser["top"]["predicate"],
        $this->rdf_parser["top"]["ordinal"],
        RDF_OBJECT_TYPE_RESOURCE,
        $buffer,
        $this->rdf_parser["top"]["xml_lang"],
        $this->rdf_parser["top"]["parent"]["bag_id"],
        $this->rdf_parser["top"]["parent"]["statements"],
        $this->rdf_parser["top"]["statement_id"] );
}

/*
    property elements with text only as content set the state to
    IN_PROPERTY_LITERAL. as character data is received from expat,
    it is saved in a buffer and reported when the end tag is
    received.
*/
function _end_literal_property()
{
    if(!isset($this->rdf_parser["top"]["statement_id"])) {
      $this->rdf_parser["top"]["statement_id"]='';
    }
    if(!isset($this->rdf_parser["top"]["parent"]["subject_type"])) {
      $this->rdf_parser["top"]["parent"]["subject_type"]='';
    }
    if(!isset($this->rdf_parser["top"]["parent"]["subject"])) {
      $this->rdf_parser["top"]["parent"]["subject"]='';
    }
    if(!isset($this->rdf_parser["top"]["parent"]["bag_id"])) {
      $this->rdf_parser["top"]["parent"]["bag_id"]='';
    }
    if(!isset($this->rdf_parser["top"]["parent"]["statements"])) {
      $this->rdf_parser["top"]["parent"]["statements"]=0;
    }
    if(!isset($this->rdf_parser["top"]["predicate"])) {
      $this->rdf_parser["top"]["predicate"]='';
    }
    if(!isset($this->rdf_parser["top"]["ordinal"])) {
      $this->rdf_parser["top"]["ordinal"]=0;
    }
    $this->_report_statement(
        $this->rdf_parser["top"]["parent"]["subject_type"],
        $this->rdf_parser["top"]["parent"]["subject"],
        $this->rdf_parser["top"]["predicate"],
        $this->rdf_parser["top"]["ordinal"],
        RDF_OBJECT_TYPE_LITERAL,
        $this->rdf_parser["top"]["data"],
        $this->rdf_parser["top"]["xml_lang"],
        $this->rdf_parser["top"]["parent"]["bag_id"],
        $this->rdf_parser["top"]["parent"]["statements"],
        $this->rdf_parser["top"]["statement_id"] );
}

function _end_element_handler( $parser, $name )
{


    switch( $this->rdf_parser["top"]["state"] )
    {
    case IN_TOP_LEVEL:
        /* fall through */
    case IN_XML:
        $this->_report_end_element( $name );
        break;
    case IN_PROPERTY_UNKNOWN_OBJECT:
        $this->_end_empty_resource_property();
        break;
    case IN_PROPERTY_LITERAL:
        $this->_end_literal_property( );
        break;
    case IN_PROPERTY_PARSE_TYPE_RESOURCE:
        $this->_pop_element(  );
        break;
    case IN_PROPERTY_PARSE_TYPE_LITERAL:
        $this->_report_end_parse_type_literal();
        break;
    case IN_RDF:
    case IN_DESCRIPTION:
    case IN_PROPERTY_RESOURCE:
    case IN_PROPERTY_EMPTY_RESOURCE:
    case IN_UNKNOWN:
        break;
    }

    $this->_pop_element();
}

function _character_data_handler( $parser,$s)
{
    $len=strlen($s);
    switch( $this->rdf_parser["top"]["state"] )
    {
    case IN_PROPERTY_LITERAL:
    case IN_PROPERTY_UNKNOWN_OBJECT:
        if( isset($this->rdf_parser["top"]["data"]) )
        {
            $n = strlen( $this->rdf_parser["top"]["data"] );
            $this->rdf_parser["top"]["data"].= $s;

        }
        else
        {
            $this->rdf_parser["top"]["data"]=$s;
        }

        if( $this->rdf_parser["top"]["state"] == IN_PROPERTY_UNKNOWN_OBJECT )
        {
            /* look for non-whitespace */
            for( $i = 0; (( $i < $len ) && (  ereg(" |\n|\t",$s{ $i }) )); $i++ );
            $i++;
            /* if we found non-whitespace, this is a literal */
            if( $i < $len )
            {
                $this->rdf_parser["top"]["state"] = IN_PROPERTY_LITERAL;
            }
        }

        break;
    case IN_TOP_LEVEL:
    case IN_PROPERTY_PARSE_TYPE_LITERAL:
    case IN_XML:
        $this->_report_character_data(
            $s,
            strlen($s) );
        break;
    case IN_RDF:
    case IN_DESCRIPTION:
    case IN_PROPERTY_RESOURCE:
    case IN_PROPERTY_EMPTY_RESOURCE:
    case IN_PROPERTY_PARSE_TYPE_RESOURCE:
    case IN_UNKNOWN:
        break;
    }
}

/* public functions */


function rdf_parser_create( $encoding )
{

    $parser = xml_parser_create_ns( $encoding, NAMESPACE_SEPARATOR_CHAR );
    xml_parser_set_option($parser,XML_OPTION_CASE_FOLDING,0);
    $this->rdf_parser["xml_parser"] = $parser;

    xml_set_object($this->rdf_parser["xml_parser"], $this);
    xml_set_element_handler( $this->rdf_parser["xml_parser"], "_start_element_handler", "_end_element_handler" );
    xml_set_character_data_handler( $this->rdf_parser["xml_parser"], "_character_data_handler" );

    return $this->rdf_parser;
}

function rdf_parser_free( )
{
    $z=3;
//    xml_parser_free( $this->rdf_parser["xml_parser"] );

    $this->rdf_parser["base_uri"]='';

    $this->_delete_elements( $this->rdf_parser );

    unset( $this->rdf_parser );
}

function rdf_set_user_data( &$user_data )
{
    $this->rdf_parser["user_data"] = &$user_data;
}

function rdf_get_user_data( )
{
    return ( $this->rdf_parser["$user_data"] );
}

function rdf_set_statement_handler($handler )
{
    $this->rdf_parser["statement_handler"] = $handler;
}

function rdf_set_parse_type_literal_handler($start,$end )
{
    $this->rdf_parser["start_parse_type_literal_handler"] = $start;
    $this->rdf_parser["end_parse_type_literal_handler"] = $end;
}

function rdf_set_element_handler($start,$end)
{
    $this->rdf_parser["_start_element_handler"] = $start;
    $this->rdf_parser["_end_element_handler"] = $end;
}

function rdf_set_character_data_handler( $handler)
{
    $this->rdf_parser["_character_data_handler"] = $handler;
}

function rdf_set_warning_handler($handler )
{
    $this->rdf_parser["warning_handler"] = $handler;
}

function rdf_parse( $s, $len, $is_final )
{
    return XML_Parse( $this->rdf_parser["xml_parser"], $s, $is_final );
}

function rdf_get_xml_parser()
{
    return ( $this->rdf_parser["xml_parser"]);
}

function rdf_set_base($base )
{

    //tcscpy( buffer, base );

/*
    if( buffer[" tcslen( buffer ) - 1 "] != T( '#' ) )
    {
        tcscat( buffer, T( "#" ) );
    }
*/

    /* check for out of memory */
    $this->rdf_parser["base_uri"]=$base;

    return 0;
}

function rdf_get_base()
{
     return $this->rdf_parser["base_uri"];
}

function rdf_resolve_uri($uri_reference,&$buffer)
{
    _resolve_uri_reference( $this->rdf_parser["base_uri"], $uri_reference, $buffer, strlen($buffer) );
}

}









}
?>