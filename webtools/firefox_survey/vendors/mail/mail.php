<?php

class mail
{

    var $from;
    var $to;
    var $subject;
    var $message;

    /**
     * Everything coming in via $params should be validated already
     */
    function mail($params)
    {
        if (array_key_exists('from', $params)) {
            $this->from = $params['from'];
        }
        if (array_key_exists('to', $params)) {
            $this->to = $params['to'];
        }
        if (array_key_exists('subject', $params)) {
            $this->subject = $params['subject'];
        }
        if (array_key_exists('message', $params)) {
            $this->message = $params['message'];
        }
    }

    function make_headers($type='html')
    {
        $headers = '';

        switch($type)
        {
            case 'html':
                $headers .= 'MIME-Version: 1.0' . "\r\n";
                $headers .= 'Content-type: text/html; charset=iso-8859-1' . "\r\n";
            break;
        }

        if (!empty($this->from)) {
            $headers .= "From: {$this->from}\r\n";
            $headers .= "Reply-To: {$this->from}\r\n";
        }
        return $headers;
    }
    function make_additional_parameters()
    {
        if (!empty($this->from)) {
            return '-f'.$this->from;
        }
    }
    function send() 
    {
        mail($this->to, $this->subject, $this->message, $this->make_headers(), $this->make_additional_parameters());
    }
}


?>
