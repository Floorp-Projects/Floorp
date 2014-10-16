Working with React JSX files
============================

The about:webrtc page uses [React](http://facebook.github.io/react/).
The UI is written in JSX files and transpiled to JS before we
commit. You need to install the JSX compiler using npm in order to
compile the .jsx files into regular .js ones:

    npm install -g react-tools

Run the following command:

   jsx -w -x jsx . .

jsx can also be do a one-time compile pass instead of watching if the
-w argument is omitted. Be sure to commit any transpiled files at the
same time as changes to their sources.

IMPORTANT: do not modify the generated files, only their JSX
counterpart.

