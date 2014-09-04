function countdown(prop) {
  var seconds = !isNaN(parseInt(prop.timeout, 10)) ? (prop.timeout / 1000) : 0,
      body = document.getElementsByTagName('body')[0],
      div = document.createElement('div'),
      timer, remaining_time, min, sec;

  add_completion_callback(function () {
    clearInterval(timer);
    body.removeChild(div);
  });
  
  div.innerHTML = 'This test will timeout in <span id="remaining_time">...</span>';
  body.appendChild(div);
    
  remaining_time = document.querySelector('#remaining_time');
  
  timer = setInterval(function() {
    function zeroPad(n) {
      return (n < 10) ? '0' + n : n;
    }
  
    if (seconds / 60 >= 1) {
      min = Math.floor(seconds / 60);
      sec = seconds % 60;
    } else {
      min = 0;
      sec = seconds;
    }
    
    if (seconds === 0) {
      div.innerHTML = 'Timeout exceeded. Increase the timeout and re-run.';
      clearInterval(timer);
    }
    
    remaining_time.textContent = zeroPad(min) + ':' + zeroPad(sec);
    seconds--;
  }, 1000);
}
