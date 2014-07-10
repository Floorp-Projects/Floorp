
# Writing Types

Commands are a fundamental building block because they are what the users
directly interacts with, however they are built on ``Type``s. There are a
number of built in types:

* string. This is a JavaScript string
* number. A JavaScript number
* boolean. A Javascript boolean
* selection. This is an selection from a number of alternatives
* delegate. This type could change depending on other factors, but is well
  defined when one of the conversion routines is called.

There are a number of additional types defined by Pilot and GCLI as
extensions to the ``selection`` and ``delegate`` types

* setting. One of the defined settings
* settingValue. A value that can be applied to an associated setting.
* command. One of the defined commands

Most of our types are 'static' e.g. there is only one type of 'string', however
some types like 'selection' and 'delegate' are customizable.

All types must inherit from Type and have the following methods:

    /**
     * Convert the given <tt>value</tt> to a string representation.
     * Where possible, there should be round-tripping between values and their
     * string representations.
     */
    stringify: function(value) { return 'string version of value'; },

    /**
     * Convert the given <tt>str</tt> to an instance of this type.
     * Where possible, there should be round-tripping between values and their
     * string representations.
     * @return Conversion
     */
    parse: function(str) { return new Conversion(...); },

    /**
     * The plug-in system, and other things need to know what this type is
     * called. The name alone is not enough to fully specify a type. Types like
     * 'selection' and 'delegate' need extra data, however this function returns
     * only the name, not the extra data.
     * <p>In old bespin, equality was based on the name. This may turn out to be
     * important in Ace too.
     */
    name: 'example',

In addition, defining the following functions can be helpful, although Type
contains default implementations:
* increment(value)
* decrement(value)

Type, Conversion and Status are all declared by commands.js.

The values produced by the parse function can be of any type, but if you are
producing your own, you are strongly encouraged to include properties called
``name`` and ``description`` where it makes sense. There are a number of
places in GCLI where the UI will be able to provide better help to users if
your values include these properties.


# Writing Fields

Fields are visual representations of types. For simple types like string it is
enough to use ``<input type=...>``, however more complex types we may wish to
provide a custom widget to allow the user to enter values of the given type.

This is an example of a very simple new password field type:

    function PasswordField(doc) {
      this.doc = doc;
    }

    PasswordField.prototype = Object.create(Field.prototype);

    PasswordField.prototype.createElement = function(assignment) {
      this.assignment = assignment;
      this.input = dom.createElement(this.doc, 'input');
      this.input.type = 'password';
      this.input.value = assignment.arg ? assignment.arg.text : '';

      this.onKeyup = function() {
          this.assignment.setValue(this.input.value);
      }.bind(this);
      this.input.addEventListener('keyup', this.onKeyup, false);

      this.onChange = function() {
          this.input.value = this.assignment.arg.text;
      };
      this.assignment.onAssignmentChange.add(this.onChange, this);

      return this.input;
    };

    PasswordField.prototype.destroy = function() {
      this.input.removeEventListener('keyup', this.onKeyup, false);
      this.assignment.onAssignmentChange.remove(this.onChange, this);
    };

    PasswordField.claim = function(type) {
      return type.name === 'password' ? Field.claim.MATCH : Field.claim.NO_MATCH;
    };
