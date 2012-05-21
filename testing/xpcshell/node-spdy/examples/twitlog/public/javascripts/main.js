/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

!function() {
  var socket = io.connect(),
      container = document.getElementById('tweets'),
      tweets = [],
      first;

  socket.emit('reqTweets');
  socket.on('tweet', function(tweet) {
    if (tweets.length > 18) {
      // Remove first tweet
      container.removeChild(tweets.shift());
    }
    tweets.push(createTweet(tweet));
  });

  function createTweet(tweet) {
    var elem = document.createElement('article'),
        avatar = document.createElement('img'),
        textContainer = document.createElement('div'),
        text = document.createElement('div'),
        clear = document.createElement('div');

    avatar.className = 'avatar';
    avatar.src = tweet.user.image.replace(
        /^http:\/\/a2\.twimg\.com\//,
        '//twimg0-a.akamaihd.net/'
    );
    avatar.title = avatar.alt = tweet.user.name;

    text.className = 'text';
    text.innerHTML = tweet.text;

    clear.className = 'clear';

    textContainer.className = 'text-container';
    textContainer.appendChild(text);
    textContainer.appendChild(clear);

    elem.className = 'tweet';
    elem.appendChild(avatar);
    elem.appendChild(textContainer);

    if (first) {
      container.insertBefore(elem, first);
    } else {
      container.appendChild(elem);
    }
    first = elem;

    return elem;
  };
}();
