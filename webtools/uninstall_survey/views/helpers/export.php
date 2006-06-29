<?php
/**
 * When we're exporting data, we end up having to pass all the parameters
 * via $_GET.  Since we're doing this a few times per page (each graph, and the
 * export hrefs) this helper should make that easier.
 */
class ExportHelper
{
    /**
     * Method to collect the current $_GET parameters and build another string from
     * them.
     *
     * @param string cake URL that you want prepended to the result.  eg: reports/graph/
     * @param array the array of GET variables to add.  These need to be
     *              pre-sanitized to print in html!  This is designed to 
     *              be used from the calling controller like:
     *                   $params = $this->sanitize->html($url_parameters);
     * @param string string to put between url and arguments (probably either '?' or
     *              '&')
     * @param array array of strings which will be ignored
     * @return string string with the url variables appeneded to it
     */
    function buildUrlString($url, $params, $seperator='?', $ignore=array('url'))
    {
        $arguments = '';

        foreach ($params as $var => $val) {
            if (!in_array($var, $ignore)) {
                $arguments .= empty($arguments) ? "{$var}={$val}" : "&amp;{$var}={$val}";
            }
        }

        return "{$this->webroot}{$url}{$seperator}{$arguments}";
    }

    function buildCsvExportString($params)
    {
        if (array_key_exists('product', $params)) {
            $filename = $params['product'];
        } else {
            $filename = DEFAULT_APP_NAME.' '.DEFAULT_APP_VERSION;
        }

        // Our filenames have underscores
        $filename = str_replace(' ','_',$filename);

        $filename = "export-{$filename}.csv";

        if (is_readable(ROOT.DS.APP_DIR.DS.WEBROOT_DIR.DS.'export'.DS.$filename)) {
            return "{$this->webroot}export/{$filename}";
        } else {
            return '';
        }
    }
}
?>
