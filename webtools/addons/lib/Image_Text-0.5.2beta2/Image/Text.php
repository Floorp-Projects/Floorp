<?php

/**
 * Image_Text - Advanced text maipulations in images
 *
 * Image_Text provides advanced text manipulation facilities for GD2
 * image generation with PHP. Simply add text clippings to your images,
 * let the class automatically determine lines, rotate text boxes around
 * their center or top left corner. These are only a couple of features
 * Image_Text provides.
 * @package Image_Text
 * @license The PHP License, version 3.0
 * @author Tobias Schlitt <toby@php.net>
 * @category images
 */


/**
 *
 * Require PEAR file for error handling.
 *
 */

require_once 'PEAR.php';

/**
 * Regex to match HTML style hex triples.
 */

define("IMAGE_TEXT_REGEX_HTMLCOLOR", "/^[#|]([a-f0-9]{2})?([a-f0-9]{2})([a-f0-9]{2})([a-f0-9]{2})$/i", true);

/**
 * Defines horizontal alignment to the left of the text box. (This is standard.)
 */
define("IMAGE_TEXT_ALIGN_LEFT", "left", true);
/**
 * Defines horizontal alignment to the center of the text box.
 */
define("IMAGE_TEXT_ALIGN_RIGHT", "right", true);
/**
 * Defines horizontal alignment to the center of the text box.
 */
define("IMAGE_TEXT_ALIGN_CENTER", "center", true);

/**
 * Defines vertical alignment to the to the top of the text box. (This is standard.)
 */
define("IMAGE_TEXT_ALIGN_TOP", "top", true);
/**
 * Defines vertical alignment to the to the middle of the text box.
 */
define("IMAGE_TEXT_ALIGN_MIDDLE", "middle", true);
/**
 * Defines vertical alignment to the to the bottom of the text box.
 */
define("IMAGE_TEXT_ALIGN_BOTTOM", "bottom", true);

/**
 * TODO: This constant is useless until now, since justified alignment does not work yet
 */
define("IMAGE_TEXT_ALIGN_JUSTIFY", "justify", true);


/**
 * Image_Text - Advanced text maipulations in images
 *
 * Image_Text provides advanced text manipulation facilities for GD2
 * image generation with PHP. Simply add text clippings to your images,
 * let the class automatically determine lines, rotate text boxes around
 * their center or top left corner. These are only a couple of features
 * Image_Text provides.
 *
 * @package Image_Text
 */
 
/*  
    // This is a simple example script, showing Image_Text's facilities.
    
    require_once 'Image/Text.php';

    $colors = array(
        0 => '#0d54e2',
        1 => '#e8ce7a',
        2 => '#7ae8ad'
    );
        
    $text = "EXTERIOR: DAGOBAH -- DAY\nWith Yoda\nstrapped to\n\nhis back, Luke climbs up one of the many thick vines that grow in the swamp until he reaches the Dagobah statistics lab. Panting heavily, he continues his exercises -- grepping, installing new packages, logging in as root, and writing replacements for two-year-old shell scripts in PHP.\nYODA: Code! Yes. A programmer's strength flows from code maintainability. But beware of Perl. Terse syntax... more than one way to do it... default variables. The dark side of code maintainability are they. Easily they flow, quick to join you when code you write. If once you start down the dark path, forever will it dominate your destiny, consume you it will.\nLUKE: Is Perl better than PHP?\nYODA: No... no... no. Orderless, dirtier, more seductive.\nLUKE: But how will I know why PHP is better than Perl?\nYODA: You will know. When your code you try to read six months from now...";

    $options = array(
                'canvas'        => array('width'=> 600,'height'=> 600), // Generate a new image 600x600 pixel
                'cx'            => 300,     // Set center to the middle of the canvas
                'cy'            => 300,
                'width'         => 300,     // Set text box size
                'height'        => 300,
                'line_spacing'  => 1,       // Normal linespacing
                'angle'         => 45,      // Text rotated by 45°
                'color'         => $colors, // Predefined colors
                'max_lines'     => 100,     // Maximum lines to render
                'min_font_size' => 2,       // Minimal/Maximal font size (for automeasurize)
                'max_font_size' => 50,
                'font_path'     => './',    // Settings for the font file 
                'font_file'     => 'Vera.ttf',
                'antialias'     => true,    // Antialiase font rendering
                'halign'        => IMAGE_TEXT_ALIGN_RIGHT,  // Alignment to the right and middle
                'valign'        => IMAGE_TEXT_ALIGN_MIDDLE
            );
            
    // Generate a new Image_Text object
    $itext = new Image_Text($text, $options);
    
    // Initialize and check the settings
    $itext->init();

    // Automatically determine optimal font size
    $itext->autoMeasurize();
   
    // Render the image
    $itext->render();
     
    // Display it
    $itext->display();
*/
 

class Image_Text {

    /**
     * Options array. these options can be set through the constructor or the set() method.
     *
     * Possible options to set are:
     * <pre>
     *
     *      'x'                 | This sets the top left coordinates (using x/y) or the center point
     *      'y'                 | coordinates (using cx/cy) for your text box. The values from
     *      'cx'                | cx/cy will overwrite x/y.
     *      'cy'                |
     *
     *      'canvas'            | You can set different values as a canvas:
     *                          |   - A gd image resource.
     *                          |   - An array with 'width' and 'height'.
     *                          |   - Nothing (the canvas will be measured after the real text size).
     *
     *      'antialias'         | This is usually true. Set it to false to switch antialiasing off.
     *
     *      'width'             | The width and height for your text box.
     *      'height'            |
     *
     *      'halign'            | Alignment of your text inside the textbox. Use alignmnet constants to define
     *      'valign'            | vertical and horizontal alignment.
     *
     *      'angle'             | The angle to rotate your text box.
     *
     *      'color'             | An array of color values. Colors will be rotated in the mode you choose (linewise
     *                          | or paragraphwise). Can be in the following formats:
     *                          |   - String representing HTML style hex couples (+ unusual alpha couple in the first place, optional).
     *                          |   - Array of int values using 'r', 'g', 'b' and optionally 'a' as keys.
     *
     *      'color_mode'        | The color rotation mode for your color sets. Does only apply if you
     *                          | defined multiple colors. Use 'line' or 'paragraph'.
     *
     *      'font_path'         | Location of the font to use. The path only gives the directory path (ending with a /).
     *      'font_file'         | The fontfile is given in the 'font_file' option.
     *
     *      'font_size'         | The font size to render text in (will be overwriten, if you use automeasurize).
     *
     *      'line_spacing'      | Measure for the line spacing to use. Default is 0.5.
     *
     *      'min_font_size'     | Automeasurize settings. Try to keep this area as small as possible to get better
     *      'max_font_size'     | performance.
     *
     *      'image_type'        | The type of image (use image type constants). Is default set to PNG.
     *
     *      'dest_file'         | The destination to (optionally) save your file.
     * </pre>
     *
     * @access public
     * @var array
     * @see Image_Text::Image_Text(), Image_Text::set()
     */

    var $options = array(
            // orientation
            'x'                 => 0,
            'y'                 => 0,

            // surface
            'canvas'            => null,
            'antialias'         => true,

            // text clipping
            'width'             => 0,
            'height'            => 0,

            // text alignement inside the clipping
            'halign'             => IMAGE_TEXT_ALIGN_LEFT,
            'valign'             => IMAGE_TEXT_ALIGN_TOP,

            // angle to rotate the text clipping
            'angle'             => 0,

            // color settings
            'color'             => array( '#000000' ),

            'color_mode'        => 'line',

            // font settings
            'font_path'         => "./",
            'font_file'         => null,
            'font_size'         => 2,
            'line_spacing'      => 0.5,

            // automasurizing settings
            'min_font_size'     => 1,
            'max_font_size'     => 100,

            // misc settings
            'image_type'        => IMAGETYPE_PNG,
            'dest_file'         => ''
        );

    /**
     * Contains option names, which can cause re-initialization force.
     *
     * @var array
     * @access private
     */

    var $_reInits = array('width', 'height', 'canvas', 'angle', 'font_file', 'font_path', 'font_size');

    /**
     * The text you want to render.
     *
     * @access private
     * @var string
     */

    var $_text;

    /**
     * Resource ID of the image canvas.
     *
     * @access private
     * @var ressource
     */

    var $_img;

    /**
     * Tokens (each word).
     *
     * @access private
     * @var array
     */

    var $_tokens = array();

    /**
     * Fullpath to the font.
     *
     * @access private
     * @var string
     */

    var $_font;

    /**
     * Contains the bbox of each rendered lines.
     *
     * @access private
     * @var array
     */

    var $bbox = array();

    /**
     * Defines in which mode the canvas has be set.
     *
     * @access private
     * @var array
     */

    var $_mode = '';

    /**
     * Color indeces returned by imagecolorallocatealpha.
     *
     * @access public
     * @var array
     */

    var $colors = array();

    /**
     * Width and height of the (rendered) text.
     *
     * @access private
     * @var array
     */

    var $_realTextSize = array('width' => false, 'height' => false);

    /**
     * Measurized lines.
     *
     * @access private
     * @var array
     */

    var $_lines = false;

    /**
     * Fontsize for which the last measuring process was done.
     *
     * @access private
     * @var array
     */

    var $_measurizedSize = false;

    /**
     * Is the text object initialized?
     *
     * @access private
     * @var bool
     */

    var $_init = false;

    /**
     * Constructor
     *
     * Set the text and options. This initializes a new Image_Text object. You must set your text
     * here. Optinally you can set all options here using the $options parameter. If you finished switching
     * all options you have to call the init() method first befor doing anything further! See Image_Text::set()
     * for further information.
     *
     * @param   string  $text       Text to print.
     * @param   array   $options    Options.
     * @access public
     * @see Image_Text::set(), Image_Text::construct(), Image_Text::init()
     */

    function Image_Text($text, $options = null)
    {
        $this->set('text', $text);
        if (!empty($options)) {
            $this->options = array_merge($this->options, $options);
        }
    }

    /**
     * Construct and initialize an Image_Text in one step.
     * This method is called statically and creates plus initializes an Image_Text object.
     * Beware: You will have to recall init() if you set an option afterwards manually.
     *
     * @param   string  $text       Text to print.
     * @param   array   $options    Options.
     * @access public
     * @static
     * @see Image_Text::set(), Image_Text::Image_Text(), Image_Text::init()
     */
    
    function &construct ( $text, $options ) {
        $itext = new Image_Text($text, $options);
        $res = $itext->init();
        if (PEAR::isError($res)) {
            return $res;
        }
        return $itext;
    }
    
    /**
     * Set options
     *
     * Set a single or multiple options. It may happen that you have to reinitialize the Image_Text
     * object after changing options. For possible options, please take a look at the class options
     * array!
     *
     *
     * @param   mixed   $option     A single option name or the options array.
     * @param   mixed   $value      Option value if $option is string.
     * @return  bool                True on success, otherwise PEAR::Error.
     * @access public
     * @see Image_Text::Image_Text()
     */

    function set($option, $value=null)
    {
        $reInits = array_flip($this->_reInits);
        if (!is_array($option)) {
            if (!isset($value)) {
                return PEAR::raiseError('No value given.');
            }
            $option = array($option => $value);
        }
        foreach ($option as $opt => $val) {
            switch ($opt) {
             case 'color':
                $this->setColors($val);
                break;
             case 'text':
                if (is_array($val)) {
                    $this->_text = implode('\n', $val);
                } else {
                    $this->_text = $val;
                }
                break;
             default:
                $this->options[$opt] = $val;
                break;
            }
            if (isset($reInits[$opt])) {
                $this->_init = false;
            }
        }
        return true;
    }

    /**
     * Set the color-set
     *
     * Using this method you can set multiple colors for your text.
     * Use a simple numeric array to determine their order and give
     * it to this function. Multiple colors will be
     * cycled by the options specified 'color_mode' option. The given array
     * will overwrite the existing color settings!
     *
     * The following colors syntaxes are understood by this method:
     * - "#ffff00" hexadecimal format (HTML style), with and without #.
     * - "#08ffff00" hexadecimal format (HTML style) with alpha channel (08), with and without #.
     * - array with 'r','g','b' and (optionally) 'a' keys, using int values.
     * - a GD color special color (tiled,...).
     *
     * A single color or an array of colors are allowed here.
     *
     * @param   mixed  $colors       Single color or array of colors.
     * @return  bool                 True on success, otherwise PEAR::Error.
     * @access  public
     * @see Image_Text::setColor(), Image_Text::set()
     */

    function setColors($colors)
    {
        $i = 0;
        if (is_array($colors) &&
            (is_string($colors[0]) || is_array($colors[0]))
           ) {
            foreach ($colors as $color) {
                $res = $this->setColor($color,$i++);
                if (PEAR::isError($res)) {
                    return $res;
                }
            }
        } else {
            return $this->setColor($colors, $i);
        }
        return true;
    }

    /**
     * Set a color
     *
     * This method is used to set a color at a specific color ID inside the
     * color cycle.
     *
     * The following colors syntaxes are understood by this method:
     * - "#ffff00" hexadecimal format (HTML style), with and without #.
     * - "#08ffff00" hexadecimal format (HTML style) with alpha channel (08), with and without #.
     * - array with 'r','g','b' and (optionally) 'a' keys, using int values.
     *
     * @param   mixed    $color        Color value.
     * @param   mixed    $id           ID (in the color array) to set color to.
     * @return  bool                True on success, otherwise PEAR::Error.
     * @access  public
     * @see Image_Text::setColors(), Image_Text::set()
     */

    function setColor($color, $id=0)
    {
        if(is_array($color)) {
            if (isset($color['r']) && isset($color['g']) && isset($color['b'])) {
                $color['a'] = isset($color['a']) ? $color['a'] : 0;
                $this->options['colors'][$id] = $color;
            } else if (isset($color[0]) && isset($color[1]) && isset($color[2])) {
                $color['r'] = $color[0];
                $color['g'] = $color[1];
                $color['b'] = $color[2];
                $color['color']['a'] = isset($color[3]) ? $color[3] : 0;
                $this->options['colors'][$id] = $color;
            } else {
                return PEAR::raiseError('Use keys 1,2,3 (optionally) 4 or r,g,b and (optionally) a.');
            }
        } elseif (is_string($color)) {
            $color = $this->_convertString2RGB($color);
            if ($color) {
                $this->options['color'][$id] = $color;
            } else {
                return PEAR::raiseError('Invalid color.');
            }
        }
        if ($this->_img) {
            $aaFactor = ($this->options['antialias']) ? 1 : -1;
            if (function_exists('imagecolorallocatealpha') && isset($color['a'])) {
                $this->colors[$id] = $aaFactor * imagecolorallocatealpha($this->_img,
                                $color['r'],$color['g'],$color['b'],$color['a']);
            } else {
                $this->colors[$id] = $aaFactor * imagecolorallocate($this->_img,
                                $color['r'],$color['g'],$color['b']);
            }
        }
        return true;
    }

    /**
     * Initialiaze the Image_Text object.
     *
     * This method has to be called after setting the options for your Image_Text object.
     * It initializes the canvas, normalizes some data and checks important options.
     * Be shure to check the initialization after you switched some options. The
     * set() method may force you to reinitialize the object.
     *
     * @access  public
     * @return  bool  True on success, otherwise PEAR::Error.
     * @see Image_Text::set()
     */

    function init()
    {
        // Does the fontfile exist and is readable?
        // todo: with some versions of the GD-library it's also possible to leave font_path empty, add strip ".ttf" from
        //        the fontname; the fontfile will then be automatically searched for in library-defined directories
        //        however this does not yet work if at this point we check for the existance of the fontfile
        if (!is_file($this->options['font_path'].$this->options['font_file']) || !is_readable($this->options['font_path'].$this->options['font_file'])) {
            return PEAR::raiseError('Fontfile not found or not readable.');
        } else {
            $this->_font = $this->options['font_path'].$this->options['font_file'];
        }

        // Is the font size to small?
        if ($this->options['width'] < 1) {
            return PEAR::raiseError('Width too small. Has to be > 1.');
        }

        // Check and create canvas
        
        switch (true) {
            case (empty($this->options['canvas'])):
                
                // Create new image from width && height of the clipping
                $this->_img = imagecreatetruecolor(
                            $this->options['width'], $this->options['height']);
                if (!$this->_img) {
                    return PEAR::raiseError('Could not create image canvas.');
                }
                break;
            
            case (is_resource($this->options['canvas']) &&
                    get_resource_type($this->options['canvas'])=='gd'):
                // The canvas is an image resource
                $this->_img = $this->options['canvas'];
                break;
            
            case (is_array($this->options['canvas']) && 
                    isset($this->options['canvas']['width']) &&
                    isset($this->options['canvas']['height'])):
                    
                // Canvas must be a width and height measure
                $this->_img = imagecreatetruecolor(
                    $this->options['canvas']['width'],
                    $this->options['canvas']['height']
                );
                break;
            
            
            case (is_array($this->options['canvas']) && 
                    isset($this->options['canvas']['size']) &&
                    ($this->options['canvas']['size'] = 'auto')):
                    
            case (is_string($this->options['canvas']) &&
                     ($this->options['canvas'] = 'auto')):
                $this->_mode = 'auto';
                break;
                
            default:
                return PEAR::raiseError('Could not create image canvas.');
            
        }
        
        
        
        if ($this->_img) {
            $this->options['canvas'] = array();
            $this->options['canvas']['height'] = imagesx($this->_img);
            $this->options['canvas']['width'] = imagesy($this->_img);
        }

        // Save and repair angle
        $angle = $this->options['angle'];
        while($angle < 0) {
            $angle += 360;
        }
        if($angle > 359) {
            $angle = $angle % 360;
        }
        $this->options['angle'] = $angle;

        // Set the color values
        $res = $this->setColors($this->options['color']);
        if (PEAR::isError($res)) {
            return $res;
        }

        $this->_lines = null;

        // Initialization is complete
        $this->_init = true;
        return true;
    }

    /**
     * Auto measurize text
     *
     * Automatically determines the greatest possible font size to
     * fit the text into the text box. This method may be very resource
     * intensive on your webserver. A good tweaking point are the $start
     * and $end parameters, which specify the range of font sizes to search
     * through. Anyway, the results should be cached if possible. You can
     * optionally set $start and $end here as a parameter or the settings of
     * the options array are used.
     *
     * @access public
     * @param  int      $start  Fontsize to start testing with.
     * @param  int      $end    Fontsize to end testing with.
     * @return int              Fontsize measured or PEAR::Error.
     * @see Image_Text::measurize()
     */

    function autoMeasurize($start=false, $end=false)
    {
        if (!$this->_init) {
            return PEAR::raiseError('Not initialized. Call ->init() first!');
        }

        $start = (empty($start)) ? $this->options['min_font_size'] : $start;
        $end = (empty($end)) ? $this->options['max_font_size'] : $end;

        $res = false;
        // Run through all possible font sizes until a measurize fails
        // Not the optimal way. This can be tweaked!
        for ($i = $start; $i <= $end; $i++) {
            $this->options['font_size'] = $i;
            $res = $this->measurize();

            if ($res === false) {
                if ($start == $i) {
                    $this->options['font_size'] = -1;
                    return PEAR::raiseError("No possible font size found");
                }
                $this->options['font_size'] -= 1;
                $this->_measurizedSize = $this->options['font_size'];
                break;
            }
            // Always the last couple of lines is stored here.
            $this->_lines = $res;
        }
        return $this->options['font_size'];
    }

    /**
     * Measurize text into the text box
     *
     * This method makes your text fit into the defined textbox by measurizing the
     * lines for your given font-size. You can do this manually before rendering (or use
     * even Image_Text::autoMeasurize()) or the renderer will do measurizing
     * automatically.
     *
     * @access public
     * @param  bool  $force  Optionally, default is false, set true to force measurizing.
     * @return array         Array of measured lines or PEAR::Error.
     * @see Image_Text::autoMeasurize()
     */

    function measurize($force=false)
    {
        if (!$this->_init) {
            return PEAR::raiseError('Not initialized. Call ->init() first!');
        }
        $this->_processText();

        // Precaching options
        $font = $this->_font;
        $size = $this->options['font_size'];

        $line_spacing = $this->options['line_spacing'];
        $space = (1 + $this->options['line_spacing']) * $this->options['font_size'];

        $max_lines = (int)$this->options['max_lines'];

        if (($max_lines<1) && !$force) {
            return false;
        }

        $block_width = $this->options['width'];
        $block_height = $this->options['height'];

        $colors_cnt = sizeof($this->colors);
        $c = $this->colors[0];

        $text_line = '';

        $lines_cnt = 0;
        $tokens_cnt = sizeof($this->_tokens);

        $lines = array();

        $text_height = 0;
        $text_width = 0;

        $i = 0;
        $para_cnt = 0;

        $beginning_of_line = true;

        // Run through tokens and order them in lines
        foreach($this->_tokens as $token) {
            // Handle new paragraphs
            if ($token=="\n") {
                $bounds = imagettfbbox($size, 0, $font, $text_line);
                if ((++$lines_cnt>=$max_lines) && !$force) {
                    return false;
                }
                if ($this->options['color_mode']=='paragraph') {
                    $c = $this->colors[$para_cnt%$colors_cnt];
                    $i++;
                } else {
                    $c = $this->colors[$i++%$colors_cnt];
                }
                $lines[]  = array(
                                'string'        => $text_line,
                                'width'         => $bounds[2]-$bounds[0],
                                'height'        => $bounds[1]-$bounds[7],
                                'bottom_margin' => $bounds[1],
                                'left_margin'   => $bounds[0],
                                'color'         => $c
                            );
                $text_width = max($text_width, ($bounds[2]-$bounds[0]));
                $text_height += (int)$space;
                if (($text_height > $block_height) && !$force) {
                    return false;
                }
                $para_cnt++;
                $text_line = '';
                $beginning_of_line = true;
                continue;
            }

            // Usual lining up

            if ($beginning_of_line) {
                $text_line = '';
                $text_line_next = $token;
                $beginning_of_line = false;
            } else {
                $text_line_next = $text_line.' '.$token;
            }
            $bounds = imagettfbbox($size, 0, $font, $text_line_next);
            $prev_width = isset($prev_width)?$width:0;
            $width = $bounds[2]-$bounds[0];

            // Handling of automatic new lines
            if ($width>$block_width) {
                if ((++$lines_cnt>=$max_lines) && !$force) {
                    return false;
                }
                if ($this->options['color_mode']=='line') {
                    $c = $this->colors[$i++%$colors_cnt];
                } else {
                    $c = $this->colors[$para_cnt%$colors_cnt];
                    $i++;
                }

                $lines[]  = array(
                                'string'    => $text_line,
                                'width'     => $prev_width,
                                'height'    => $bounds[1]-$bounds[7],
                                'bottom_margin' => $bounds[1],
                                'left_margin'   => $bounds[0],
                                'color'         => $c
                            );
                $text_width = max($text_width, ($bounds[2]-$bounds[0]));
                $text_height += (int)$space;
                if (($text_height > $block_height) && !$force) {
                    return false;
                }

                $text_line      = $token;
                $bounds = imagettfbbox($size, 0, $font, $text_line);
                $width = $bounds[2]-$bounds[0];
                $beginning_of_line = false;
            } else {
                $text_line = $text_line_next;
            }
        }
        // Store remaining line
        $bounds = imagettfbbox($size, 0, $font,$text_line);
        if ($this->options['color_mode']=='line') {
            $c = $this->colors[$i++%$colors_cnt];
        }
        $lines[]  = array(
                        'string'=> $text_line,
                        'width' => $bounds[2]-$bounds[0],
                        'height'=> $bounds[1]-$bounds[7],
                        'bottom_margin' => $bounds[1],
                        'left_margin'   => $bounds[0],
                        'color'         => $c
                    );

        // add last line height, but without the line-spacing
        $text_height += $this->options['font_size'];

        $text_width = max($text_width, ($bounds[2]-$bounds[0]));

        if (($text_height > $block_height) && !$force) {
            return false;
        }

        $this->_realTextSize = array('width' => $text_width, 'height' => $text_height);
        $this->_measurizedSize = $this->options['font_size'];

        return $lines;
    }

    /**
     * Render the text in the canvas using the given options.
     *
     * This renders the measurized text or automatically measures it first. The $force parameter
     * can be used to switch of measurizing problems (this may cause your text being rendered
     * outside a given text box or destroy your image completely).
     *
     * @access public
     * @param  bool     $force  Optional, initially false, set true to silence measurize errors.
     * @return bool             True on success, otherwise PEAR::Error.
     */

    function render($force=false)
    {
        if (!$this->_init) {
            return PEAR::raiseError('Not initialized. Call ->init() first!');
        }

        if (!$this->_tokens) {
            $this->_processText();
        }

        if (empty($this->_lines) || ($this->_measurizedSize != $this->options['font_size'])) {
            $this->_lines = $this->measurize( $force );
        }
        $lines = $this->_lines;

        if (PEAR::isError($this->_lines)) {
            return $this->_lines;
        }

        if ($this->_mode === 'auto') {
            $this->_img = imagecreatetruecolor(
                        $this->_realTextSize['width'],
                        $this->_realTextSize['height']
                    );
            if (!$this->_img) {
                return PEAR::raiseError('Could not create image cabvas.');
            }
            $this->_mode = '';
            $this->setColors($this->_options['color']);
        }

        $block_width = $this->options['width'];
        $block_height = $this->options['height'];

        $max_lines = $this->options['max_lines'];

        $angle = $this->options['angle'];
        $radians = round(deg2rad($angle), 3);

        $font = $this->_font;
        $size = $this->options['font_size'];

        $line_spacing = $this->options['line_spacing'];

        $align = $this->options['halign'];

        $im = $this->_img;

        $offset = $this->_getOffset();

        $start_x = $offset['x'];
        $start_y = $offset['y'];

        $end_x = $start_x + $block_width;
        $end_y = $start_y + $block_height;

        $sinR = sin($radians);
        $cosR = cos($radians);

        switch ($this->options['valign']) {
            case IMAGE_TEXT_ALIGN_TOP:
                $valign_space = 0;
                break;
            case IMAGE_TEXT_ALIGN_MIDDLE:
                $valign_space = ($this->options['height'] - $this->_realTextSize['height']) / 2;
                break;
            case IMAGE_TEXT_ALIGN_BOTTOM:
                $valign_space = $this->options['height'] - $this->_realTextSize['height'];
                break;
            default:
                $valign_space = 0;
        }

        $space = (1 + $line_spacing) * $size;

        // Adjustment of align + translation of top-left-corner to bottom-left-corner of first line
        $new_posx = $start_x + ($sinR * ($valign_space + $size));
        $new_posy = $start_y + ($cosR * ($valign_space + $size));

        $lines_cnt = min($max_lines,sizeof($lines));

        // Go thorugh lines for rendering
        for($i=0; $i<$lines_cnt; $i++){

            // Calc the new start X and Y (only for line>0)
            // the distance between the line above is used
            if($i > 0){
                $new_posx += $sinR * $space;
                $new_posy += $cosR * $space;
            }

            // Calc the position of the 1st letter. We can then get the left and bottom margins
            // 'i' is really not the same than 'j' or 'g'.
            $bottom_margin  = $lines[$i]['bottom_margin'];
            $left_margin    = $lines[$i]['left_margin'];
            $line_width     = $lines[$i]['width'];

            // Calc the position using the block width, the current line width and obviously
            // the angle. That gives us the offset to slide the line.
            switch($align) {
                case IMAGE_TEXT_ALIGN_LEFT:
                    $hyp = 0;
                    break;
                case IMAGE_TEXT_ALIGN_RIGHT:
                    $hyp = $block_width - $line_width - $left_margin;
                    break;
                case IMAGE_TEXT_ALIGN_CENTER:
                    $hyp = ($block_width-$line_width)/2 - $left_margin;
                    break;
                default:
                    $hyp = 0;
                    break;
            }

            $posx = $new_posx + $cosR * $hyp;
            $posy = $new_posy - $sinR * $hyp;

            $c = $lines[$i]['color'];

            // Render textline
            $bboxes[] = imagettftext ($im, $size, $angle, $posx, $posy, $c, $font, $lines[$i]['string']);
        }
        $this->bbox = $bboxes;
        return true;
    }

    /**
     * Return the image ressource.
     *
     * Get the image canvas.
     *
     * @access public
     * @return resource Used image resource
     */

    function &getImg()
    {
        return $this->_img;
    }

    /**
     * Display the image (send it to the browser).
     *
     * This will output the image to the users browser. You can use the standard IMAGETYPE_*
     * constants to determine which image type will be generated. Optionally you can save your
     * image to a destination you set in the options.
     *
     * @param   bool  $save  Save or not the image on printout.
     * @param   bool  $free  Free the image on exit.
     * @return  bool         True on success, otherwise PEAR::Error.
     * @access public
     * @see Image_Text::save()
     */

    function display($save=false, $free=false)
    {
        if (!headers_sent()) {
            header("Content-type: " .image_type_to_mime_type($this->options['image_type']));
        } else {
            PEAR::raiseError('Header already sent.');
        }
        switch ($this->options['image_type']) {
            case IMAGETYPE_PNG:
                $imgout = 'imagepng';
                break;
            case IMAGETYPE_JPEG:
                $imgout = 'imagejpeg';
                break;
            case IMAGETYPE_BMP:
                $imgout = 'imagebmp';
                break;
            default:
                return PEAR::raiseError('Unsupported image type.');
                break;
        }
        if ($save) {
            $imgout($this->_img);
            $res = $this->save();
            if (PEAR::isError($res)) {
                return $res;
            }
        } else {
           $imgout($this->_img);
        }

        if ($free) {
            $res = imagedestroy($this->image);
            if (!$res) {
                PEAR::raiseError('Destroying image failed.');
            }
        }
        return true;
    }

    /**
     * Save image canvas.
     *
     * Saves the image to a given destination. You can leave out the destination file path,
     * if you have the option for that set correctly. Saving is possible with the save()
     * method, too.
     *
     * @param   string  $destFile   The destination to save to (optional, uses options value else).
     * @return  bool                True on success, otherwise PEAR::Error.
     * @see Image_Text::display()
     */

    function save($dest_file=false)
    {
        if (!$dest_file) {
            $dest_file = $this->options['dest_file'];
        }
        if (!$dest_file) {
            return PEAR::raiseError("Invalid desitination file.");
        }
        
        switch ($this->options['image_type']) {
            case IMAGETYPE_PNG:
                $imgout = 'imagepng';
                break;
            case IMAGETYPE_JPEG:
                $imgout = 'imagejpeg';
                break;
            case IMAGETYPE_BMP:
                $imgout = 'imagebmp';
                break;
            default:
                return PEAR::raiseError('Unsupported image type.');
                break;
        }
        
        $res = $imgout($this->_img, $dest_file);
        if (!$res) {
            PEAR::raiseError('Saving file failed.');
        }
        return true;
    }

    /**
     * Get completely translated offset for text rendering.
     *
     * Get completely translated offset for text rendering. Important
     * for usage of center coords and angles
     *
     * @access private
     * @return array    Array of x/y coordinates.
     */

    function _getOffset()
    {
        // Presaving data
        $width = $this->options['width'];
        $height = $this->options['height'];
        $angle = $this->options['angle'];
        $x = $this->options['x'];
        $y = $this->options['y'];
        // Using center coordinates
        if (!empty($this->options['cx']) && !empty($this->options['cy'])) {
            $cx = $this->options['cx'];
            $cy = $this->options['cy'];
            // Calculation top left corner
            $x = $cx - ($width / 2);
            $y = $cy - ($height / 2);
            // Calculating movement to keep the center point on himslf after rotation
            if ($angle) {
                $ang = deg2rad($angle);
                // Vector from the top left cornern ponting to the middle point
                $vA = array( ($cx - $x), ($cy - $y) );
                // Matrix to rotate vector
                // sinus and cosinus
                $sin = round(sin($ang), 14);
                $cos = round(cos($ang), 14);
                // matrix
                $mRot = array(
                    $cos, (-$sin),
                    $sin, $cos
                );
                // Multiply vector with matrix to get the rotated vector
                // This results in the location of the center point after rotation
                $vB = array (
                    ($mRot[0] * $vA[0] + $mRot[2] * $vA[0]),
                    ($mRot[1] * $vA[1] + $mRot[3] * $vA[1])
                );
                // To get the movement vector, we subtract the original middle
                $vC = array (
                    ($vA[0] - $vB[0]),
                    ($vA[1] - $vB[1])
                );
                // Finally we move the top left corner coords there
                $x += $vC[0];
                $y += $vC[1];
            }
        }
        return array ('x' => (int)round($x, 0), 'y' => (int)round($y, 0));
    }

    /**
     * Convert a color to an array.
     *
     * The following colors syntax must be used:
     * "#08ffff00" hexadecimal format with alpha channel (08)
     * array with 'r','g','b','a'(optionnal) keys
     * A GD color special color (tiled,...)
     * Only one color is allowed
     * If $id is given, the color index $id is used
     *
     * @param   mixed  $colors       Array of colors.
     * @param   mixed  $id           Array of colors.
     * @access private
     */
    function _convertString2RGB($scolor)
    {
        if (preg_match(IMAGE_TEXT_REGEX_HTMLCOLOR, $scolor, $matches)) {
            return array(
                           'r' => hexdec($matches[2]),
                           'g' => hexdec($matches[3]),
                           'b' => hexdec($matches[4]),
                           'a' => hexdec(!empty($matches[1])?$matches[1]:0),
                           );
        }
        return false;
    }

    /**
     * Extract the tokens from the text.
     *
     * @access private
     */
    function _processText()
    {
        if (!isset($this->_text)) {
            return false;
        }
        $this->_tokens = array();

        // Normalize linebreak to "\n"
        $this->_text = preg_replace("[\r\n]", "\n", $this->_text);

        // Get each paragraph
        $paras = explode("\n",$this->_text);

        // loop though the paragraphs
        // and get each word (token)
        foreach($paras as $para) {
            $words = explode(' ',$para);
            foreach($words as $word) {
                $this->_tokens[] = $word;
            }
            // add a "\n" to mark the end of a paragraph
            $this->_tokens[] = "\n";
        }
        // we do not need an end paragraph as the last token
        array_pop($this->_tokens);
    }
}


