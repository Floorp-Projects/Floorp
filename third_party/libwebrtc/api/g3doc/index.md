<?% config.freshness.owner = 'hta' %?>
<?% config.freshness.reviewed = '2021-04-12' %?>

# The WebRTC API

The public API of the WebRTC library consists of the api/ directory and
its subdirectories. No other files should be depended on by webrtc users.

Before starting to code against the API, it is important to understand
some basic concepts, such as:

* Memory management, including webrtc's reference counted objects
* [Thread management](threading_design.md)
