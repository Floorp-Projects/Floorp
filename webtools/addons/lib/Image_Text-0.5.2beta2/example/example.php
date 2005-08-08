<?php

	// ini_set('include_path', '.:/cvs/pear/Image_Text:/usr/share/pear');

    require_once 'Image/Text.php';
       
    function getmicrotime()
    {
        list($usec,$sec) = explode(' ', microtime());
        return ((float)$usec+(float)$sec);
    }

    $colors = array(
        0 => '#0d54e2',
        1 => '#e8ce7a',
        2 => '#7ae8ad'
    );
        
    $texts[] = "a passion for php"; // Short Text
    $texts[] = "a good computer is like a tipi - no windows, no gates and an apache inside"; // Normal Text
    $texts[] = "What is PEAR?\nThe fleshy pome, or fruit, of a rosaceous tree (Pyrus communis), cultivated in many varieties in temperate climates.\nPEAR is a framework and distribution system for reusable PHP components. PECL, being a subset of PEAR, is the complement for C extensions for PHP. See the FAQ and manual for more information."; // Long Text
    $texts[] = "EXTERIOR: DAGOBAH -- DAY\nWith Yoda\nstrapped to\n\nhis back, Luke climbs up one of the many thick vines that grow in the swamp until he reaches the Dagobah statistics lab. Panting heavily, he continues his exercises -- grepping, installing new packages, logging in as root, and writing replacements for two-year-old shell scripts in PHP.\nYODA: Code! Yes. A programmer's strength flows from code maintainability. But beware of Perl. Terse syntax... more than one way to do it... default variables. The dark side of code maintainability are they. Easily they flow, quick to join you when code you write. If once you start down the dark path, forever will it dominate your destiny, consume you it will.\nLUKE: Is Perl better than PHP?\nYODA: No... no... no. Orderless, dirtier, more seductive.\nLUKE: But how will I know why PHP is better than Perl?\nYODA: You will know. When your code you try to read six months from now...";
    $texts[] = "1. The idea:\nThe idea is very simple. I wanted to have 2 versions of PHP (4.3.4 and 5.0beta3) running together on 1 maschine, both as Apache 1.3 modules.\n2. Compiling PHP:\nI had a PHP 4.3.4 running on my devbox and compiled a PHP 5 version in addition. This can be managed in a bloody simple way. Download a copy of PHP 5, unpack the archive to any directory. Copy the './configure' statement from your installed PHP version and exchange/create the prefix option, e.g. '--prefix=/usr/php5' and change the '--with-config-file-path' to a different location (e.g. '/etc/php5').\n\nRun the './configure' statement inside the unpack directory using './configure '. You can get a complete list opf PHP 5 supported option with './configure --help'.\n\nPHP 5 make files had been created successfully (most errors shouls result from version conflicts of libraries and header files, you can easiely solve this by installing or compiling new versions of the required packages/development-packages). Now you should run the 'make' statement and get a coffee, during your first PHP 5 compile... ein Wort \nmehr";
    $texts[] = "1. The idea: \n\n\n\nThe idea is very simple. I wanted to have 2 versions of PHP (4.3.4 and 5.0beta3) running together on 1 maschine, both as Apache 1.3 modules. \n\n \n\n2. Compiling PHP: \n\n \n\nI had a PHP 4.3.4 running on my devbox and compiled a PHP 5 version in addition. This can be managed in a bloody simple way. Download a copy of PHP 5, unpack the archive to any directory. Copy the './configure' statement from your installed PHP version and exchange/create the prefix option, e.g. '--prefix=/usr/php5' and change the '--with-config-file-path' to a different location (e.g. '/etc/php5'). \n\n \n\nRun the './configure' statement inside the unpack directory using './configure '. You can get a complete list opf PHP 5 supported option with './configure --help'. \n\n \n\nPHP 5 make files had been created successfully (most errors shouls result from version conflicts of libraries and header files, you can easiely solve this by installing or compiling new versions of the required packages/development-packages). Now you should run the 'make' statement and get a coffee,\nduring your first PHP 5 compile...";
    $texts[] = "1. The idea: The idea is very simple. I wanted to have 2 versions of PHP (4.3.4 and 5.0beta3) running together on 1 maschine, both as Apache 1.3 modules. 2. Compiling PHP: I had a PHP 4.3.4 running on my devbox and compiled a PHP 5 version in addition. This can be managed in a bloody simple way. Download a copy of PHP 5, unpack the archive to any directory. Copy the './configure' statement from your installed PHP version and exchange/create the prefix option, e.g. '--prefix=/usr/php5' and change the '--with-config-file-path' to a different location (e.g. '/etc/php5'). Run the './configure' statement inside the unpack directory using './configure '. You can get a complete list opf PHP 5 supported option with './configure --help'. PHP 5 make files had been created successfully (most errors shouls result from version conflicts of libraries and header files, you can easiely solve this by installing or compiling new versions of the required packages/development-packages). Now you should run the 'make' statement and get a coffee, during your first PHP 5 compile...";

    $options = array(
                'cx'            => 300,
                'cy'            => 300,
                'canvas'        => array('width'=> 600,'height'=> 600),
                'width'         => 300,
                'height'        => 300,
                'line_spacing'  => 1,
                'angle'         => 0,
                'color'         => $colors,
                'max_lines'     => 100,
                'min_font_size' => 2,
                'max_font_size' => 50,
                'font_path'     => './',
                'antialias'     => true,
                'font_file'     => 'Vera.ttf',
                'halign'        => IMAGE_TEXT_ALIGN_LEFT
            );

    $text = $texts[array_rand($texts)];
            
    foreach ($_GET as $key => $val) {
        switch ($key) {
            case 'font_size':
                $options['font_size'] = (int)$val;
                break;
            case 'line_spacing':
                $options['line_spacing'] = $val;
                break;
            case 'halign':
                $options['halign'] = $val;
                break;
            case 'valign':
                $options['valign'] = $val;
                break;
            case 'angle':
                $options['angle'] = $val;
                break;
            case 'text':
                $text = $texts[$val];
                break;
        }
    }
            
    $start = getmicrotime();
    
    $itext = new Image_Text($text, $options);
    
    $itext->init();

    if (empty($options['font_size'])) {
        $itext->autoMeasurize();
    } else {
        $itext->measurize();
    }
   
    $itext->render();
    
    $end = getmicrotime();
    
    $time = $end - $start;
        
    if (isset($_GET['debug'])) {
        echo "<h1>-- DEBUGGING MODE --</h1>";
    } else {

        $img =& $itext->getImg();
        
        $red = imagecolorallocate($img, 255, 0, 0);
        
        imagefilledellipse($img, 300, 300, 10, 10, $red);
        
        if (isset($_GET['info'])) {
            imagettftext($img, 10, 0, 10, 550, $red, "./Vera.ttf", "$time sec.");
        }
       
        $itext->display();
    
    }

?>
