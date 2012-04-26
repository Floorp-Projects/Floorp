var twitter = new (require('ntwitter'))({
  consumer_key: '',
  consumer_secret: '',
  access_token_key: '',
  access_token_secret: ''
});

exports.init = function(io) {
  var tweets = [];
  twitter.search(
    '#spdytwitlog',
    {
      result_type: 'recent',
      include_entities: 1
    },
    function(err, result) {
      if (err) return console.error(err);
      result.results.sort(function(a, b) {
        return (+new Date(a.created_at)) -
               (+new Date(b.created_at));
      }).forEach(receive);
    }
  );

  function watchStream(method, query) {
    twitter.stream(method, query, function(stream) {
      stream.on('data', receive);

      stream.on('end', retry);
      stream.on('destroy', retry);

      var once = false;
      function retry() {
        if (once) return;
        once = true;

        setTimeout(function() {
          watchStream(method, query);
        }, 5000);
      }
    });
  }

  watchStream('statuses/filter', {
    track: '#spdytwitlog',
    include_entities: 1
  });

  function receive(tweet) {
    if (tweet.entities && tweet.entities.urls) {
      tweet.entities.urls.sort(function(a, b) {
        return b.indices[0] - a.indices[0];
      }).forEach(function(url) {
        tweet.text = tweet.text.slice(0, url.indices[0]) +
                     url.display_url.link(url.expanded_url || url.url) +
                     tweet.text.slice(url.indices[1]);
      });
    }
    tweet = {
      text: tweet.text,
      user: tweet.user ? {
        name: tweet.user.screen_name,
        image: tweet.user.profile_image_url
      } : {
        name: tweet.from_user,
        image: tweet.profile_image_url
      }
    };
    io.sockets.emit('tweet', tweet);

    tweets.push(tweet);
    // remember only last 18 tweets
    tweets = tweets.slice(tweets.length - 18, tweets.length);
  };

  io.sockets.on('connection', function(socket) {
    socket.on('reqTweets', function() {
      tweets.forEach(function(tweet) {
        socket.emit('tweet', tweet);
      });
    });
  });
};
