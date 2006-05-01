function toggleHelp(helpTitle,helpText) {
  var em = document.getElementById("help");
  if (toggleHelp.arguments.length < 1) {
     em.innerHTML="";
     em.style.display = 'none';
     return;
  }

  var closeLink = '<div class="closeLink"><a name="closeHelp" onClick="toggleHelp();"><img class="chrome" src="images/close.png" />Close Help</a></div>'
  em.innerHTML = '<div class="container"><div class="title">' + helpTitle + '</div><div class="content">' + helpText + '</div>' + closeLink + '</div>';
  em.style.display = 'block';
}



