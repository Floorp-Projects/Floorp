<?php
require_once(HEADER);
echo '<form action="'.$_SERVER['PHP_SELF'].'" method="post" id="surveyform">';

// Create intend block.
echo '<h2>How did you intend to use Thunderbird when you installed it?</h2>';
echo '<ul class="survey">';
foreach ($intends as $id=>$text) {
    echo '<li><input type="radio" name="intend_id" id="int'.$id.'" value="'.$id.'" /> <label for="int'.$id.'">'.$text.'</label></li>';
}
echo '<li><label for="int0"><input type="radio" name="intend_id" id="int0" value="0"/> Other, please specify:</label> <input type="text" name="intend_text" id="intother" /></li>';
echo '</ul>';

// Create issue block.
echo '<h2>Why did you uninstall Thunderbird? (select all that apply)</h2>';
echo '<ul class="survey">';
foreach ($issues as $id=>$text) {
    echo '<li><label for="iss'.$id.'"> <input type="checkbox" name="issue_id[]" id="iss'.$id.'" value="'.$id.'" />'.$text.'</label></li>';
}
echo '</ul>';

echo '<h2>How can we improve Thunderbird?</h2>';
echo '<p>Please share your ideas, suggestions or details about any issues below.</p>';
echo '<div><textarea name="comments" rows="7" cols="60"></textarea></div>';

echo '<div>';
echo '<input type="hidden" name="product" value="'.htmlentities(!empty($_GET['product'])?$_GET['product']:null).'"/>';
echo '<input type="hidden" name="useragent" value="'.htmlentities(!empty($_GET['useragent'])?$_GET['useragent']:null).'"/>';
echo '</div>';

echo '<div><input name="submit" type="submit" class="submit" value="Submit &raquo;"/></div>';
echo '</form>';
require_once(FOOTER);
?>
