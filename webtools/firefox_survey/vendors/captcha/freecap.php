<?
/************************************************************\
*
*		freeCap v1.4.1 Copyright 2005 Howard Yeend
*		www.puremango.co.uk
*
*    This file is part of freeCap.
*
*    freeCap is free software; you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation; either version 2 of the License, or
*    (at your option) any later version.
*
*    freeCap is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with freeCap; if not, write to the Free Software
*    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*
\************************************************************/

session_name('CAKEPHP');
session_start();

//////////////////////////////////////////////////////
////// User Defined Vars:
//////////////////////////////////////////////////////

// try to avoid the 'free p*rn' method of CAPTCHA circumvention
// see www.wikipedia.com/captcha for more info
//$site_tags[0] = "";
// or more simply:
//$site_tags[0] = "for use only on puremango.co.uk";
// reword or add lines as you please
// or if you don't want any text:
$site_tags = null;

// where to write the above:
// 0=top
// 1=bottom
// 2=both
$tag_pos = 1;

// functions to call for random number generation
// mt_rand produces 'better' random numbers
// but if your server doesn't support it, it's fine to use rand instead
$rand_func = "mt_rand";
$seed_func = "mt_srand";

// which type of hash to use?
// possible values: "sha1", "md5", "crc32"
// sha1 supported by PHP4.3.0+
// md5 supported by PHP3+
// crc32 supported by PHP4.0.1+
$hash_func = "sha1";
// store in session so can validate in form processor
$_SESSION['hash_func'] = $hash_func;

// image type:
// possible values: "jpg", "png", "gif"
// jpg doesn't support transparency (transparent bg option ends up white)
// png isn't supported by old browsers (see http://www.libpng.org/pub/png/pngstatus.html)
// gif may not be supported by your GD Lib.
$output = "jpg";

// 0=generate pseudo-random string, 1=use dictionary
// dictionary is easier to recognise
// - both for humans and computers, so use random string if you're paranoid.
$use_dict = 0;
// if your server is NOT set up to deny web access to files beginning ".ht"
// then you should ensure the dictionary file is kept outside the web directory
// eg: if www.foo.com/index.html points to c:\website\www\index.html
// then the dictionary should be placed in c:\website\dict.txt
// test your server's config by trying to access the dictionary through a web browser
// you should NOT be able to view the contents.
// can leave this blank if not using dictionary
$dict_location = "./.ht_freecap_words";

// used to calculate image width, and for non-dictionary word generation
$max_word_length = 6;

// text colour
// 0=one random colour for all letters
// 1=different random colour for each letter
$col_type = 1;

// maximum times a user can refresh the image
// on a 6500 word dictionary, I think 15-50 is enough to not annoy users and make BF unfeasble.
// further notes re: BF attacks in "avoid brute force attacks" section, below
// on the other hand, those attempting OCR will find the ability to request new images
// very useful; if they can't crack one, just grab an easier target...
// for the ultra-paranoid, setting it to <5 will still work for most users
$max_attempts = 20000;

// list of fonts to use
// font size should be around 35 pixels wide for each character.
// you can use my GD fontmaker script at www.puremango.co.uk to create your own fonts
// There are other programs to can create GD fonts, but my script allows a greater
// degree of control over exactly how wide each character is, and is therefore
// recommended for 'special' uses. For normal use of GD fonts,
// the GDFontGenerator @ http://www.philiplb.de is excellent for convering ttf to GD

// the fonts included with freeCap *only* include lowercase alphabetic characters
// so are not suitable for most other uses
// to increase security, you really should add other fonts
$font_locations = Array(
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_font1.gdf",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_font2.gdf",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_font3.gdf",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_font4.gdf",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_font5.gdf"
);

// background:
// 0=transparent (if jpg, white)
// 1=white bg with grid
// 2=white bg with squiggles
// 3=morphed image blocks
// 'random' background from v1.3 didn't provide any extra security (according to 2 independent experts)
// many thanks to http://ocr-research.org.ua and http://sam.zoy.org/pwntcha/ for testing
// for jpgs, 'transparent' is white
$bg_type = 1;
// should we blur the background? (looks nicer, makes text easier to read, takes longer)
$blur_bg = false;
// for bg_type 3, which images should we use?
// if you add your own, make sure they're fairly 'busy' images (ie a lot of shapes in them)
$bg_images = Array(
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_im1.jpg",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_im2.jpg",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_im3.jpg",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_im4.jpg",
ROOT.DS.APP_DIR.DS."vendors/captcha/.ht_freecap_im6.jpg"
);
// for non-transparent backgrounds only:
	// if 0, merges CAPTCHA with bg
	// if 1, write CAPTCHA over bg
	$merge_type = 0;
	// should we morph the bg? (recommend yes, but takes a little longer to compute)
	$morph_bg = true;

// you shouldn't need to edit anything below this, but it's extensively commented if you do want to play
// have fun, and email me with ideas, or improvements to the code (very interested in speed improvements)
// hope this script saves some spam :-)



//////////////////////////////////////////////////////
////// Create Images + initialise a few things
//////////////////////////////////////////////////////

// seed random number generator
// PHP 4.2.0+ doesn't need this, but lower versions will
$seed_func(make_seed());

// how faded should the bg be? (100=totally gone, 0=bright as the day)
// to test how much protection the bg noise gives, take a screenshot of the freeCap image
// and take it into a photo editor. play with contrast and brightness.
// If you can remove most of the bg, then it's not a good enough percentage
switch($bg_type)
{
	case 0:
		break;
	case 1:
	case 2:
		$bg_fade_pct = 65;
		break;
	case 3:
		$bg_fade_pct = 50;
		break;
}
// slightly randomise the bg fade
$bg_fade_pct += $rand_func(-2,2);

// read each font and get font character widths
$font_widths = Array();
for($i=0 ; $i<sizeof($font_locations) ; $i++)
{
	$handle = fopen($font_locations[$i],"r");
	// read header of GD font, up to char width
	$c_wid = fread($handle,11);
	$font_widths[$i] = ord($c_wid{8})+ord($c_wid{9})+ord($c_wid{10})+ord($c_wid{11});
	fclose($handle);
}

// modify image width depending on maximum possible length of word
// you shouldn't need to use words > 6 chars in length really.
$width = ($max_word_length*(array_sum($font_widths)/sizeof($font_widths))+75);
$height = 90;

$im = ImageCreate($width, $height);
$im2 = ImageCreate($width, $height);



//////////////////////////////////////////////////////
////// Avoid Brute Force Attacks:
//////////////////////////////////////////////////////
if(empty($_SESSION['freecap_attempts']))
{
	$_SESSION['freecap_attempts'] = 1;
} else {
	$_SESSION['freecap_attempts']++;

	// if more than ($max_attempts) refreshes, block further refreshes
	// can be negated by connecting with new session id
	// could get round this by storing num attempts in database against IP
	// could get round that by connecting with different IP (eg, using proxy servers)
	// in short, there's little point trying to avoid brute forcing
	// the best way to protect against BF attacks is to ensure the dictionary is not
	// accessible via the web or use random string option
/*
If this becomes a problem, I'll reenable this, but I don't really see it. --clouser
	if($_SESSION['freecap_attempts']>$max_attempts)
	{
		$_SESSION['freecap_word_hash'] = false;

		$bg = ImageColorAllocate($im,255,255,255);
		ImageColorTransparent($im,$bg);

		$red = ImageColorAllocate($im, 255, 0, 0);
		// depending on how rude you want to be :-)
		//ImageString($im,5,0,20,"bugger off you spamming bastards!",$red);
		ImageString($im,5,15,20,"service no longer available",$red);

		sendImage($im);
	}
*/
}





//////////////////////////////////////////////////////
////// Functions:
//////////////////////////////////////////////////////
function make_seed() {
// from http://php.net/srand
    list($usec, $sec) = explode(' ', microtime());
    return (float) $sec + ((float) $usec * 100000);
}

function rand_color() {
	global $bg_type,$rand_func;
	if($bg_type==3)
	{
		// needs darker colour..
		return $rand_func(10,100);
	} else {
		return $rand_func(0,80);
		//return $rand_func(60,170);
	}
}

function myImageBlur($im)
{
	// w00t. my very own blur function
	// in GD2, there's a gaussian blur function. bunch of bloody show-offs... :-)

	$width = imagesx($im);
	$height = imagesy($im);

	$temp_im = ImageCreateTrueColor($width,$height);
	$bg = ImageColorAllocate($temp_im,150,150,150);

	// preserves transparency if in orig image
	ImageColorTransparent($temp_im,$bg);

	// fill bg
	ImageFill($temp_im,0,0,$bg);

	// anything higher than 3 makes it totally unreadable
	// might be useful in a 'real' blur function, though (ie blurring pictures not text)
	$distance = 1;
	// use $distance=30 to have multiple copies of the word. not sure if this is useful.

	// blur by merging with itself at different x/y offsets:
	ImageCopyMerge($temp_im, $im, 0, 0, 0, $distance, $width, $height-$distance, 70);
	ImageCopyMerge($im, $temp_im, 0, 0, $distance, 0, $width-$distance, $height, 70);
	ImageCopyMerge($temp_im, $im, 0, $distance, 0, 0, $width, $height, 70);
	ImageCopyMerge($im, $temp_im, $distance, 0, 0, 0, $width, $height, 70);
	// remove temp image
	ImageDestroy($temp_im);

	return $im;
}

function sendImage($pic)
{
	// output image with appropriate headers
	global $output,$im,$im2,$im3;
	header(base64_decode("WC1DYXB0Y2hhOiBmcmVlQ2FwIDEuNCAtIHd3dy5wdXJlbWFuZ28uY28udWs="));
	switch($output)
	{
		// add other cases as desired
		case "jpg":
			header("Content-Type: image/jpeg");
			ImageJPEG($pic);
			break;
		case "gif":
			header("Content-Type: image/gif");
			ImageGIF($pic);
			break;
		case "png":
		default:
			header("Content-Type: image/png");
			ImagePNG($pic);
			break;
	}

	// kill GD images (removes from memory)
	ImageDestroy($im);
	ImageDestroy($im2);
	ImageDestroy($pic);
	if(!empty($im3))
	{
		ImageDestroy($im3);
	}
	exit();
}




//////////////////////////////////////////////////////
////// Choose Word:
//////////////////////////////////////////////////////
if($use_dict==1)
{
	// load dictionary and choose random word
	$words = @file($dict_location);
	$word = strtolower($words[$rand_func(0,sizeof($words)-1)]);
	// cut off line endings/other possible odd chars
	$word = ereg_replace("[^a-z]","",$word);
	// might be large file so forget it now (frees memory)
	$words = "";
	unset($words);
} else {
	// based on code originally by breakzero at hotmail dot com
	// (http://uk.php.net/manual/en/function.rand.php)
	// generate pseudo-random string
	// doesn't use ijtf as are easily mistaken

	// I'm not using numbers because the custom fonts I've created don't support anything other than
	// lowercase or space (but you can download new fonts or create your own using my GD fontmaker script)
	$consonants = 'bcdghklmnpqrsvwxyz';
	$vowels = 'euo';
	$word = "";

	$wordlen = $rand_func(5,$max_word_length);

	for($i=0 ; $i<$wordlen ; $i++)
	{
		// don't allow to start with 'vowel'
		if($rand_func(0,4)>=2 && $i!=0)
		{
			$word .= $vowels{$rand_func(0,strlen($vowels)-1)};
		} else {
			$word .= $consonants{$rand_func(0,strlen($consonants)-1)};
		}
	}
}

// save hash of word for comparison
// using hash so that if there's an insecurity elsewhere (eg on the form processor),
// an attacker could only get the hash
// also, shared servers usually give all users access to the session files
// echo `ls /tmp`; and echo `more /tmp/someone_elses_session_file`; usually work
// so even if your site is 100% secure, someone else's site on your server might not be
// hence, even if attackers can read the session file, they can't get the freeCap word
// (though most hashes are easy to brute force for simple strings)
$_SESSION['freecap_word_hash'] = $hash_func($word);




//////////////////////////////////////////////////////
////// Fill BGs and Allocate Colours:
//////////////////////////////////////////////////////

// set tag colour
// have to do this before any distortion
// (otherwise colour allocation fails when bg type is 1)
$tag_col = ImageColorAllocate($im,10,10,10);
$site_tag_col2 = ImageColorAllocate($im2,0,0,0);

// set debug colours (text colours are set later)
$debug = ImageColorAllocate($im, 255, 0, 0);
$debug2 = ImageColorAllocate($im2, 255, 0, 0);

// set background colour (can change to any colour not in possible $text_col range)
// it doesn't matter as it'll be transparent or coloured over.
// if you're using bg_type 3, you might want to try to ensure that the color chosen
// below doesn't appear too much in any of your background images.
$bg = ImageColorAllocate($im, 254, 254, 254);
$bg2 = ImageColorAllocate($im2, 254, 254, 254);

// set transparencies
ImageColorTransparent($im,$bg);
// im2 transparent to allow characters to overlap slightly while morphing
ImageColorTransparent($im2,$bg2);

// fill backgrounds
ImageFill($im,0,0,$bg);
ImageFill($im2,0,0,$bg2);

if($bg_type!=0)
{
	// generate noisy background, to be merged with CAPTCHA later
	// any suggestions on how best to do this much appreciated
	// sample code would be even better!
	// I'm not an OCR expert (hell, I'm not even an image expert; puremango.co.uk was designed in MsPaint)
	// so the noise models are based around my -guesswork- as to what would make it hard for an OCR prog
	// ideally, the character obfuscation would be strong enough not to need additional background noise
	// in any case, I hope at least one of the options given here provide some extra security!

	$im3 = ImageCreateTrueColor($width,$height);
	$temp_bg = ImageCreateTrueColor($width*1.5,$height*1.5);
	$bg3 = ImageColorAllocate($im3,255,255,255);
	ImageFill($im3,0,0,$bg3);
	$temp_bg_col = ImageColorAllocate($temp_bg,255,255,255);
	ImageFill($temp_bg,0,0,$temp_bg_col);

	// we draw all noise onto temp_bg
	// then if we're morphing, merge from temp_bg to im3
	// or if not, just copy a $widthx$height portion of $temp_bg to $im3
	// temp_bg is much larger so that when morphing, the edges retain the noise.

	if($bg_type==1)
	{
		// grid bg:

		// draw grid on x
		for($i=$rand_func(6,20) ; $i<$width*2 ; $i+=$rand_func(10,25))
		{
			ImageSetThickness($temp_bg,$rand_func(2,6));
			$text_r = $rand_func(100,150);
			$text_g = $rand_func(100,150);
			$text_b = $rand_func(100,150);
			$text_colour3 = ImageColorAllocate($temp_bg, $text_r, $text_g, $text_b);

			ImageLine($temp_bg,$i,0,$i,$height*2,$text_colour3);
		}
		// draw grid on y
		for($i=$rand_func(6,20) ; $i<$height*2 ; $i+=$rand_func(10,25))
		{
			ImageSetThickness($temp_bg,$rand_func(2,6));
			$text_r = $rand_func(100,150);
			$text_g = $rand_func(100,150);
			$text_b = $rand_func(100,150);
			$text_colour3 = ImageColorAllocate($temp_bg, $text_r, $text_g, $text_b);

			ImageLine($temp_bg,0,$i,$width*2, $i ,$text_colour3);
		}
	} else if($bg_type==2) {
		// draw squiggles!

		$bg3 = ImageColorAllocate($im3,255,255,255);
		ImageFill($im3,0,0,$bg3);
		ImageSetThickness($temp_bg,4);

		for($i=0 ; $i<strlen($word)+1 ; $i++)
		{
			$text_r = $rand_func(100,150);
			$text_g = $rand_func(100,150);
			$text_b = $rand_func(100,150);
			$text_colour3 = ImageColorAllocate($temp_bg, $text_r, $text_g, $text_b);

			$points = Array();
			// draw random squiggle for each character
			// the longer the loop, the more complex the squiggle
			// keep random so OCR can't say "if found shape has 10 points, ignore it"
			// each squiggle will, however, be a closed shape, so OCR could try to find
			// line terminations and start from there. (I don't think they're that advanced yet..)
			for($j=1 ; $j<$rand_func(5,10) ; $j++)
			{
				$points[] = $rand_func(1*(20*($i+1)),1*(50*($i+1)));
				$points[] = $rand_func(30,$height+30);
			}

			ImagePolygon($temp_bg,$points,intval(sizeof($points)/2),$text_colour3);
		}

	} else if($bg_type==3) {
		// take random chunks of $bg_images and paste them onto the background

		for($i=0 ; $i<sizeof($bg_images) ; $i++)
		{
			// read each image and its size
			$temp_im[$i] = ImageCreateFromJPEG($bg_images[$i]);
			$temp_width[$i] = imagesx($temp_im[$i]);
			$temp_height[$i] = imagesy($temp_im[$i]);
		}

		$blocksize = $rand_func(20,60);
		for($i=0 ; $i<$width*2 ; $i+=$blocksize)
		{
			// could randomise blocksize here... hardly matters
			for($j=0 ; $j<$height*2 ; $j+=$blocksize)
			{
				$image_index = $rand_func(0,sizeof($temp_im)-1);
				$cut_x = $rand_func(0,$temp_width[$image_index]-$blocksize);
				$cut_y = $rand_func(0,$temp_height[$image_index]-$blocksize);
				ImageCopy($temp_bg, $temp_im[$image_index], $i, $j, $cut_x, $cut_y, $blocksize, $blocksize);
			}
		}
		for($i=0 ; $i<sizeof($temp_im) ; $i++)
		{
			// remove bgs from memory
			ImageDestroy($temp_im[$i]);
		}

		// for debug:
		//sendImage($temp_bg);
	}

	// for debug:
	//sendImage($im3);

	if($morph_bg)
	{
		// morph background
		// we do this separately to the main text morph because:
		// a) the main text morph is done char-by-char, this is done across whole image
		// b) if an attacker could un-morph the bg, it would un-morph the CAPTCHA
		// hence bg is morphed differently to text
		// why do we morph it at all? it might make it harder for an attacker to remove the background
		// morph_chunk 1 looks better but takes longer

		// this is a different and less perfect morph than the one we do on the CAPTCHA
		// occasonally you get some dark background showing through around the edges
		// it doesn't need to be perfect as it's only the bg.
		$morph_chunk = $rand_func(1,5);
		$morph_y = 0;
		for($x=0 ; $x<$width ; $x+=$morph_chunk)
		{
			$morph_chunk = $rand_func(1,5);
			$morph_y += $rand_func(-1,1);
			ImageCopy($im3, $temp_bg, $x, 0, $x+30, 30+$morph_y, $morph_chunk, $height*2);
		}

		ImageCopy($temp_bg, $im3, 0, 0, 0, 0, $width, $height);

		$morph_x = 0;
		for($y=0 ; $y<=$height; $y+=$morph_chunk)
		{
			$morph_chunk = $rand_func(1,5);
			$morph_x += $rand_func(-1,1);
			ImageCopy($im3, $temp_bg, $morph_x, $y, 0, $y, $width, $morph_chunk);

		}
	} else {
		// just copy temp_bg onto im3
		ImageCopy($im3,$temp_bg,0,0,30,30,$width,$height);
	}

	ImageDestroy($temp_bg);

	if($blur_bg)
	{
		myImageBlur($im3);
	}
}
// for debug:
//sendImage($im3);




//////////////////////////////////////////////////////
////// Write Word
//////////////////////////////////////////////////////

// write word in random starting X position
$word_start_x = $rand_func(5,32);
// y positions jiggled about later
$word_start_y = 15;

if($col_type==0)
{
	$text_r = rand_color();
	$text_g = rand_color();
	$text_b = rand_color();
	$text_colour2 = ImageColorAllocate($im2, $text_r, $text_g, $text_b);
}

// write each char in different font
for($i=0 ; $i<strlen($word) ; $i++)
{
	if($col_type==1)
	{
		$text_r = rand_color();
		$text_g = rand_color();
		$text_b = rand_color();
		$text_colour2 = ImageColorAllocate($im2, $text_r, $text_g, $text_b);
	}

	$j = $rand_func(0,sizeof($font_locations)-1);
	$font = ImageLoadFont($font_locations[$j]);
	ImageString($im2, $font, $word_start_x+($font_widths[$j]*$i), $word_start_y, $word{$i}, $text_colour2);
}
// use last pixelwidth
$font_pixelwidth = $font_widths[$j];

// for debug:
//sendImage($im2);





//////////////////////////////////////////////////////
////// Morph Image:
//////////////////////////////////////////////////////

// calculate how big the text is in pixels
// (so we only morph what we need to)
$word_pix_size = $word_start_x+(strlen($word)*$font_pixelwidth);

// firstly move each character up or down a bit:
for($i=$word_start_x ; $i<$word_pix_size ; $i+=$font_pixelwidth)
{
	// move on Y axis
	// deviates at least 4 pixels between each letter
	$prev_y = $y_pos;
	do{
		$y_pos = $rand_func(-5,5);
	} while($y_pos<$prev_y+2 && $y_pos>$prev_y-2);
	ImageCopy($im, $im2, $i, $y_pos, $i, 0, $font_pixelwidth, $height);

	// for debug:
	//ImageRectangle($im,$i,$y_pos+10,$i+$font_pixelwidth,$y_pos+70,$debug);
}

// for debug:
//sendImage($im);

ImageFilledRectangle($im2,0,0,$width,$height,$bg2);

// randomly morph each character individually on x-axis
// this is where the main distortion happens
// massively improved since v1.2
$y_chunk = 1;
$morph_factor = .1;
$morph_x = 0;
for($j=0 ; $j<strlen($word) ; $j++)
{
	$y_pos = 0;
	for($i=0 ; $i<=$height; $i+=$y_chunk)
	{
		$orig_x = $word_start_x+($j*$font_pixelwidth);
		// morph x += so that instead of deviating from orig x each time, we deviate from where we last deviated to
		// get it? instead of a zig zag, we get more of a sine wave.
		// I wish we could deviate more but it looks crap if we do.
		$morph_x += $rand_func(-$morph_factor,$morph_factor);
		// had to change this to ImageCopyMerge when starting using ImageCreateTrueColor
		// according to the manual; "when (pct is) 100 this function behaves identically to imagecopy()"
		// but this is NOT true when dealing with transparencies...
		ImageCopyMerge($im2, $im, $orig_x+$morph_x, $i+$y_pos, $orig_x, $i, $font_pixelwidth, $y_chunk, 100);

		// for debug:
		//ImageLine($im2, $orig_x+$morph_x, $i, $orig_x+$morph_x+1, $i+$y_chunk, $debug2);
		//ImageLine($im2, $orig_x+$morph_x+$font_pixelwidth, $i, $orig_x+$morph_x+$font_pixelwidth+1, $i+$y_chunk, $debug2);
	}
}

// for debug:
//sendImage($im2);

ImageFilledRectangle($im,0,0,$width,$height,$bg);
// now do the same on the y-axis
// (much easier because we can just do it across the whole image, don't have to do it char-by-char)
$y_pos = 0;
$x_chunk = 1;
for($i=0 ; $i<=$width ; $i+=$x_chunk)
{
	// can result in image going too far off on Y-axis;
	// not much I can do about that, apart from make image bigger
	// again, I wish I could do 1.5 pixels
	$y_pos += $rand_func(-1,1);
	ImageCopy($im, $im2, $i, $y_pos, $i, 0, $x_chunk, $height);

	// for debug:
	//ImageLine($im,$i+$x_chunk,0,$i+$x_chunk,100,$debug);
	//ImageLine($im,$i,$y_pos+25,$i+$x_chunk,$y_pos+25,$debug);
}

// for debug:
//sendImage($im);

// blur edges:
// doesn't really add any security, but looks a lot nicer, and renders text a little easier to read
// for humans (hopefully not for OCRs, but if you know better, feel free to disable this function)
// (and if you do, let me know why)
myImageBlur($im);

// for debug:
//sendImage($im);

if($output!="jpg" && $bg_type==0)
{
	// make background transparent
	ImageColorTransparent($im,$bg);
}





//////////////////////////////////////////////////////
////// Try to avoid 'free p*rn' style CAPTCHA re-use
//////////////////////////////////////////////////////
// ('*'ed to stop my site coming up for certain keyword searches on google)

// can obscure CAPTCHA word in some cases..

// write site tags 'shining through' the morphed image
ImageFilledRectangle($im2,0,0,$width,$height,$bg2);
if(is_array($site_tags))
{
	for($i=0 ; $i<sizeof($site_tags) ; $i++)
	{
		// ensure tags are centered
		$tag_width = strlen($site_tags[$i])*6;
		// write tag is chosen position
		if($tag_pos==0 || $tag_pos==2)
		{
			// write at top
			ImageString($im2, 2, intval($width/2)-intval($tag_width/2), (10*$i), $site_tags[$i], $site_tag_col2);
		}
		if($tag_pos==1 || $tag_pos==2)
		{
			// write at bottom
			ImageString($im2, 2, intval($width/2)-intval($tag_width/2), ($height-34+($i*10)), $site_tags[$i], $site_tag_col2);
		}
	}
}
ImageCopyMerge($im2,$im,0,0,0,0,$width,$height,80);
ImageCopy($im,$im2,0,0,0,0,$width,$height);
// for debug:
//sendImage($im);




//////////////////////////////////////////////////////
////// Merge with obfuscated background
//////////////////////////////////////////////////////

if($bg_type!=0)
{
	// merge bg image with CAPTCHA image to create smooth background

	// fade bg:
	if($bg_type!=3)
	{
		$temp_im = ImageCreateTrueColor($width,$height);
		$white = ImageColorAllocate($temp_im,255,255,255);
		ImageFill($temp_im,0,0,$white);
		ImageCopyMerge($im3,$temp_im,0,0,0,0,$width,$height,$bg_fade_pct);
		// for debug:
		//sendImage($im3);
		ImageDestroy($temp_im);
		$c_fade_pct = 50;
	} else {
		$c_fade_pct = $bg_fade_pct;
	}

	// captcha over bg:
	// might want to not blur if using this method
	// otherwise leaves white-ish border around each letter
	if($merge_type==1)
	{
		ImageCopyMerge($im3,$im,0,0,0,0,$width,$height,100);
		ImageCopy($im,$im3,0,0,0,0,$width,$height);
	} else {
		// bg over captcha:
		ImageCopyMerge($im,$im3,0,0,0,0,$width,$height,$c_fade_pct);
	}
}
// for debug:
//sendImage($im);


//////////////////////////////////////////////////////
////// Write tags, remove variables and output!
//////////////////////////////////////////////////////

// tag it
// feel free to remove/change
// but if it's not essential I'd appreciate you leaving it
// after all, I've put a lot of work into this and am giving it away for free
// the least you could do is give me credit (or buy me stuff from amazon!)
// but I understand that in professional environments, your boss might not like this tag
// so that's cool.
//      sorry, but I gotta comment it out :) --clouser
//$tag_str = "freeCap v1.41 - puremango.co.uk";
// for debug:
//$tag_str = "[".$word."]";

// ensure tag is right-aligned
$tag_width = strlen($tag_str)*6;
// write tag
ImageString($im, 2, $width-$tag_width, $height-13, $tag_str, $tag_col);

// unset all sensetive vars
// in case someone include()s this file on a shared server
// you might think this unneccessary, as it exit()s
// but by using register_shutdown_function
// on a -very- insecure shared server, they -might- be able to get the word
unset($word);
// the below aren't really essential, but might aid an OCR attack if discovered.
// so we unset them
unset($use_dict);
unset($dict_location);
unset($max_word_length);
unset($bg_type);
unset($bg_images);
unset($merge_type);
unset($bg_fade_pct);
unset($morph_bg);
unset($col_type);
unset($max_attempts);
unset($font_locations);


// output final image :-)
sendImage($im);
// (sendImage also destroys all used images)
?>
