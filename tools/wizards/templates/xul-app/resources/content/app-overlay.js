${license_cpp}

function start_${app_name_short}() 
{
    toOpenWindowByType("mozapp:${app_name_short}", "${chrome_main_xul_url}");
}
