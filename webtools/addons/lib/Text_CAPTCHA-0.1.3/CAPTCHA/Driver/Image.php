<?php
/**
 * Text_CAPTCHA_Driver_Image - Text_CAPTCHA driver graphical CAPTCHAs
 *
 * Class to create a graphical Turing test 
 *
 * TODOs:
 * + refine the obfuscation algorithm :-)
 * + learn how to use Image_Text better (or remove dependency)
 * 
 * 
 * @license PHP License, version 3.0
 * @author Christian Wenz <wenz@php.net>
 */

class Text_CAPTCHA_Driver_Image extends Text_CAPTCHA
{

    /**
     * Image object
     *
     * @access private
     * @var resource
     */
    var $_im;

    /**
     * Image_Text object
     *
     * @access private
     * @var resource
     */
    var $_imt;

    /**
     * Width of CAPTCHA
     *
     * @access private
     * @var int
     */
    var $_width;

    /**
     * Height of CAPTCHA
     *
     * @access private
     * @var int
     */
    var $_height;

    /**
     * Further options (for Image_Text)
     *
     * @access private
     * @var array
     */
    var $_options;

    /**
     * init function
     *
     * Initializes the new Text_CAPTCHA_Driver_Image object and creates a GD image
     *
     * @param   int     $width      Width of image
     * @param   int     $height     Height of image
     * @param   string  $phrase     The "secret word" of the CAPTCHA
     * @param   array   $options    further options (for Image_Text)
     * @access public
     * @return  mixed        true upon success, PEAR error otherwise
     */

    function init($width = 200, $height = 80, $phrase = null, $options = null)
    {
        if (is_int($width) && is_int($height)) {
            $this->_width = $width;
            $this->_height = $height; 
            if (empty($phrase)) {
                $this->_createPhrase();
            } else {
			    $this->_phrase = $phrase;
			}
            if (empty($options)) {
                $this->_options = array(
                    'font_size' => 24,
                    'font_path' => './',
                    'font_file' => 'COUR.TTF'
                );
            } else {
                $this->_options = $options;
                if (!isset($this->_options['font_size'])) {
                    $this->_options['font_size'] = 24;
                }
                if (!isset($this->_options['font_path'])) {
                    $this->_options['font_path'] = './';
                }
                if (!isset($this->_options['font_file'])) {
                    $this->_options['font_file'] = 'COUR.TTF';
                }
            }
            $retval = $this->_createCAPTCHA();
            if (PEAR::isError($retval)) {
                return PEAR::raiseError($retval->getMessage());
            } else {
                return true;
            }
        } else {
            return PEAR::raiseError('Use numeric CAPTCHA dimensions!');
        }
    }

    /**
     * Create random CAPTCHA phrase, Image edition (with size check)
     *
     * This method creates a random phrase, maximum 8 characters or width / 25, whatever is smaller
     *
     * @access  private
     */
    function _createPhrase()
    {
        $len = intval(min(8, $this->_width / 25));
        $this->_phrase = Text_Password::create($len);
    }


    /**
     * Create CAPTCHA image
     *
     * This method creates a CAPTCHA image
     *
     * @access  private
     */
    function _createCAPTCHA()
    {
        $options['canvas'] = array(
            'width' => $this->_width,
            'height' => $this->_height
        ); 
        $options['width'] = $this->_width - 20;
        $options['height'] = $this->_height - 20; 
        $options['cx'] = ceil(($this->_width) / 2 + 10);
        $options['cy'] = ceil(($this->_height) / 2 + 10); 
        $options['angle'] = rand(0, 30) - 15;
        $options['font_size'] = $this->_options['font_size'];
        $options['font_path'] = $this->_options['font_path'];
        $options['font_file'] = $this->_options['font_file'];
        $options['color'] = array('#FFFFFF', '#000000');
        $options['max_lines'] = 1;
        $options['mode'] = 'auto';
        $this->_imt = new Image_Text( 
            $this->_phrase,
            $options
        );
        if (PEAR::isError($this->_imt->init())) {
            return PEAR::raiseError('Error initializing Image_Text (font missing?!)');
        }
        $this->_imt->measurize();
        $this->_imt->render(); 
        $this->_im =& $this->_imt->getImg(); 
        $white = imagecolorallocate($this->_im, 0xFF, 0xFF, 0xFF);
        //some obfuscation
        for ($i=0; $i<3; $i++) {
            $x1 = rand(0, $this->_width - 1);
            $y1 = rand(0, round($this->_height / 10, 0));
            $x2 = rand(0, round($this->_width / 10, 0));
            $y2 = rand(0, $this->_height - 1);
            imageline($this->_im, $x1, $y1, $x2, $y2, $white);
            $x1 = rand(0, $this->_width - 1);
            $y1 = $this->_height - rand(1, round($this->_height / 10, 0));
            $x2 = $this->_width - rand(1, round($this->_width / 10, 0));
            $y2 = rand(0, $this->_height - 1);
            imageline($this->_im, $x1, $y1, $x2, $y2, $white);
			$cx = rand(0, $this->_width - 50) + 25;
			$cy = rand(0, $this->_height - 50) + 25;
			$w = rand(1, 24);
			imagearc($this->_im, $cx, $cy, $w, $w, 0, 360, $white);
        }
    }


    /**
     * Return CAPTCHA as image resource
     *
     * This method returns the CAPTCHA as GD2 image resource
     *
     * @access  public
     * @return  im        image resource
     */
    function getCAPTCHA()
    {
        return $this->_im;
    }

    /**
     * Return CAPTCHA as PNG
     *
     * This method returns the CAPTCHA as PNG
     *
     * @access  public
     */
    function getCAPTCHAAsPNG()
    {
        if (is_resource($this->_im)) {
            ob_start();
            imagepng($this->_im);
            $data = ob_get_contents();
            ob_end_clean();
            return $data;
        } else {
            return PEAR::raiseError('Error creating CAPTCHA image (font missing?!)');        
        }
    }

    /**
     * Return CAPTCHA as JPEG
     *
     * This method returns the CAPTCHA as JPEG
     *
     * @access  public
     */
    function getCAPTCHAAsJPEG()
    {
        if (is_resource($this->_im)) {
            ob_start();
            imagejpeg($this->_im);
            $data = ob_get_contents();
            ob_end_clean();
            return $data;
        } else {
            return PEAR::raiseError('Error creating CAPTCHA image (font missing?!)');        
        }
    }

}
