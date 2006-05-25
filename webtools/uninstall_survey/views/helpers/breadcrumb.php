<?php

/**
 * The new layout uses breadcrumbs, so this is a helper to make using them easier.
 */
class BreadcrumbHelper
{

    /**
     * Will store all the links in an associative array
     */
    var $links = array();

    /**
     * Add a single link to the array
     * @param array in the form 'name' => 'url'
     */
    function addlink($newlink)
    {
        array_push($this->links,$newlink);
    }

    /**
     * Bulk add some links to the array
     * @param array in the form 'name' => 'url', ... , 'name' => 'url'
     */
    function addlinks($newlinks)
    {
        $this->links = $this->links + $newlinks;
    }

    /**
     * Walk through the links and build an html string to echo.  Note: this doesn't
     * actually echo the string, despite being a show* function.  I did this to be
     * inline with the rest of cake.
     *
     * @return string html to echo
     */
    function showLinks()
    {
        $_ret = '';

        foreach ($this->links as $name => $link) {
            $name = htmlspecialchars($name);
            $link = htmlspecialchars($link);

            if (empty($link)) {
                $_ret .= (empty($_ret)) ? "<span>{$name}</span>\n" : "&raquo; <span>{$name}</span>\n";
            } else {
                $href = $this->webroot.$link;
                $_ret .= (empty($_ret)) ? '<span><a href="'.$href.'">'.$name.'</a></span>'."\n" : '&raquo; <span><a href="'.$href.'">'.$name.'</a></span>'."\n";
            }
        }

        return $_ret;
    }


}

?>
