
/**
 * Keyword dropdown actions.  [Netscape Commercial Specific]
 **/
function executeKeyword( aKeyword )
  {
    switch( aKeyword )
      {
        case "quote":
          gURLBar.focus();
          gURLBar.value = bundle.GetStringFromName("quoteKeyword");
          gURLBar.setSelectionRange(14,33);
          break;
        case 'local':
          gURLBar.focus();
          gURLBar.value = bundle.GetStringFromName("localKeyword");
          gURLBar.setSelectionRange(12,27);
          break;
        case 'shop':
          gURLBar.focus();
          gURLBar.value = bundle.GetStringFromName("shopKeyword");
          gURLBar.setSelectionRange(13,22);
          break;
        case 'career':
          gURLBar.focus();
          gURLBar.value = bundle.GetStringFromName("careerKeyword");
          gURLBar.setSelectionRange(8,19);
          break;
        case 'webmail':
          appCore.loadUrl(bundle.GetStringFromName("webmailKeyword"));
          break;
        case 'list':
          appCore.loadUrl(bundle.GetStringFromName("keywordList"));
          break;
      }
  }

function executeKeywordCommand( aEvent )
  {
    var keyword = aEvent.target.getAttribute( "keyword" );
    if( keyword )
      {
        executeKeyword( keyword );
        return;
      }
  }        