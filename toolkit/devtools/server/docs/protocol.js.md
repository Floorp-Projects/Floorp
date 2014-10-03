Writing an Actor
================

A Simple Hello World
--------------------

Here's a simple Hello World actor.  It is a global actor (not associated with a given browser tab).

    let protocol = require("devtools/server/protocol");
    let {method, Arg, Option, RetVal} = protocol;

    // This will be called by the framework when you call DebuggerServer.
    // registerModule(), and adds the actor as a 'helloActor' property
    // on the root actor.
    exports.register = function(handle) {
        handle.addGlobalActor(HelloActor, "helloActor");
    }

    // This will be called by the framework during shutdown/unload.
    exports.unregister = function(handle) {
        handle.removeGlobalActor(HelloActor); // This is optional, all registered actors will be removed automatically
    }

    // Create the hello actor.  This uses addon-sdk's heritage module
    // behind the scenes in case you want to understand the class stuff
    // a bit better.

    let HelloActor = protocol.ActorClass({
        typeName: "helloWorld", // I'll explain types later, I promise.
        initialize: function(conn) {
            protocol.Actor.prototype.initialize.call(this, conn); // This is the worst part of heritage.
        },

        sayHello: method(function() {
           return "hello";
        }, {
          // The request packet template.  There are no arguments, so
          // it is empty.  The framework will add the "type" and "to"
          // request properties.
          request: {},

          // The response packet template.  The return value of the function
          // will be plugged in where the RetVal() appears in the template.
          response: {
            greeting: RetVal("string") // "string" is the return value type.
          }
        }
    });

This actor now supports a `sayHello` request.  A request/reply will look like this:

    -> { to: <actorID>, type: "sayHello" }
    <- { from: <actorID>, greeting: "hello" }

Now we can create a client side object.  We call these *front* objects.

Here's the front for the HelloActor:

    let HelloFront = protocol.FrontClass(HelloActor, {
      initialize: function(client, form) {
        protocol.Front.prototype.initialize.call(this, client, form);
      }
    });

Note that there is no `sayHello` method.  The FrontClass will generate a method on the Front object that matches the method declaration in the Actor class.

The generated methods will return a Promise.  That promise will resolve to the RetVal of the actor method.

So if we have a reference to a HelloFront object, we can issue a `sayHello` request:

  hello.sayHello().then(greeting => {
    console.log(greeting);
  });

How do you get an initial reference to the front?  That's a bit tricky, but basically there are two ways:

* Manually
* Magically

Manually - If you're using a DebuggerClient instance, you can discover the actorID manually and create a Front for it:
  let hello = HelloFront(this.client, { actorID: <hello actorID> });

Magically - Once you have an initial reference to a protocol.js object, it can return other protocol.js objects and fronts will automatically be created.

Arguments
---------

`sayHello` has no arguments, so let's add a method that does take arguments:

    echo: method(function(str) {
        return str + "... " + str + "...";
    }, {
      request: { echo: Arg(0, "string") },
      response: { echoed: RetVal("string") }
    })

This tells the library to place the 0th argument, which should be a string, in the `echo` property of the request packet.


This will generate a request handler whose request and response packets look like this:

    { to: <actorID>, type: "echo", echo: <str> }
    { from: <actorID>, echoed: <str> }

The client usage should be predictable:

    echo.echo("hello").then(str => { assert(str === "hello... hello...") })

The library tries hard to make using fronts feel like natural javascript (or as natural as you believe promises are, I guess).  When building the response it will put the return value of the function where RetVal() is specified in the response template, and on the client side it will use the value in that position when resolving the promise.

Returning JSON
--------------

Maybe your response is an object:

    addOneTwice: method(function(a, b) {
        return { a: a + 1, b: b + 1 };
    }, {
        request: { a: Arg(0, "number"), b: Arg(1, "number") },
        response: { ret: RetVal("json") }
    });

This will generate a response packet that looks like:

    { from: <actorID>, ret: { a: <number>, b: <number> } }

That's probably unnecessary nesting (if you're sure you won't be returning an object with 'from' as a key!), so you can just replace `response` with:

    response: RetVal("json")

and now your packet will look like:

    { from: <actorID>, a: <number>, b: <number> }

Types and Marshalling
---------------------

Things have been pretty simple up to this point - all the arguments we've passed in have been javascript primitives.  But for some types (most importantly Actor types, which I'll get to eventually), we can't just copy them into a JSON packet and expect it to work, we need to marshal things ourselves.

Again, the protocol lib tries hard to provide a natural API to actors and clients, and sometime that natural API might involve object APIs. I'm going to use a wickedly contrived example, bear with me.  Let's say I have a small object that contains a number and has a few methods associated with it:

    let Incrementor = function(i) {
      this.value = value;
    }
    Incrementor.prototype = {
        increment: function() { this.value++ },
        decrement: function() { this.value-- }
    };


and I want to return it from a backend function:

    getIncrementor: method(function(i) {
        return new Incrementor(i)
    }, {
        request: { number: Arg(0, "number") },
        response: { value: RetVal("incrementor") } // We'll define "incrementor" below.
    });

I want that response to look like `{ from: <actorID>, value: <number> }`, but the client side needs to know to return an Incrementor, not a primitive number.  So let's tell the protocol lib about Incrementors:

    protocol.types.addType("incrementor", {
        // When writing to a protocol packet, just send the value
        write: (v) => v.value,

        // When reading from a protocol packet, wrap with an Incrementor
        // object.
        read: (v) => new Incrementor(v)
    });

And now our client can use the API as expected:

    front.getIncrementor(5).then(incrementor => {
        incrementor.increment();
        assert(incrementor.value === 6);
    });

You can do the same thing with arguments:

    passIncrementor: method(function(inc) {
        w.increment();
        assert(incrementor.value === 6);
    }, {
        request: { Arg(0, "incrementor") },
    });

    front.passIncrementor(new Incrementor(5));

The library provides primitiive `boolean`, `number`, `string`, and `json` types.

Moving right along, let's say you want to pass/return an array of Incrementors.  You can just prepend `array:` to the type name:

    incrementAll: method(function(incrementors) {
        incrementors.forEach(incrementor => {
          incrementor.increment();
        }
        return incrementors;
    }, {
        request: { incrementors: Arg(0, "array:incrementor") },
        response: { incrementors: RetVal("array:incrementor") }
    })

Or maybe you want to return a dictionary where one item is a incrementor.  To do this you need to tell the type system which members of the dictionary need custom marshallers:

    protocol.types.addDictType("contrivedObject", {
        incrementor: "incrementor",
        incrementorArray: "array:incrementor"
    });

    reallyContrivedExample: method(function() {
        return {
            /* a and b are primitives and so don't need to be called out specifically in addDictType */
            a: "hello", b: "world",
            incrementor: new Incrementor(1), incrementorArray: [new Incrementor(2), new Incrementor(3)]
        }
    }, {
        response: RetVal("contrivedObject")
    });

    front.reallyContrivedExample().then(obj => {
        assert(obj.a == "hello");
        assert(obj.b == "world");
        assert(incrementor.i == 1);
        assert(incrementorArray[0].i == 2);
        assert(incrementorArray[1].i == 3);
    });

Nullables
---------

If an argument, return value, or dict property can be null/undefined, you can prepend `nullable:` to the type name:

    "nullable:incrementor",  // Can be null/undefined or an incrementor
    "array:nullable:incrementor", // An array of incrementors that can have holes.
    "nullable:array:incrementor" // Either null/undefined or an array of incrementors without holes.


Actors
------

Probably the most common objects that need custom martialing are actors themselves.  These are more interesting than the Incrementor object, but by default they're somewhat easy to work with.  Let's add a ChildActor implementation that will be returned by the HelloActor (which is rapidly becoming the OverwhelminglyComplexActor):

    let ChildActor = protocol.ActorClass({
        actorType: "childActor",
        initialize: function(conn, id) {
            protocol.Actor.prototype.initialize.call(this, conn);
            this.greeting = "hello from " + id;
        },
        getGreeting: method(function() {
            return this.greeting;
        }, {
            response: { greeting: RetVal("string") },
        }
    });

    let ChildFront = protocol.FrontClass(ChildActor, {
        initialize: function(client, form) {
            protocol.Front.prototype.initialize.call(this, client, form);
        },
    });

The library will register a marshaller for the actor type itself, using typeName as its tag.

So we can now add the following code to HelloActor:

    getChild: method(function(id) {
        return ChildActor(this.conn, id);
    }, {
        request: { id: Arg(0, "string") },
        response: { child: RetVal("childActor") }
    });

    front.getChild("child1").then(childFront => {
        return childFront.getGreeting();
    }).then(greeting => {
        assert(id === "hello from child1");
    });

The conversation will look like this:

    { to: <actorID>, type: "getChild", id: "child1" }
    { from: <actorID>, child: { actor: <childActorID> }}
    { to: <childActorID>, type: "getGreeting" }
    { from: <childActorID>, greeting: "hello from child1" }

But the ID is the only interesting part of this made-up example.  You're never going to want a reference to a ChildActor without checking its ID.  Making an extra request just to get that id is wasteful.  You really want the first response to look like `{ from: <actorID>, child: { actor: <childActorID>, greeting: "hello from child1" } }`

You can customize the marshalling of an actor by providing a `form` method in the `ChildActor` class:

    form: function() {
        return {
            actor: this.actorID,
            greeting: this.greeting
        }
    },

And you can demarshal in the `ChildFront` class by implementing a matching `form` method:

    form: function(form) {
        this.actorID = form.actor;
        this.greeting = form.greeting;
    }

Now you can use the id immediately:

    front.getChild("child1").then(child => { assert(child.greeting === "child1) });

You may come across a situation where you want to customize the output of a `form` method depending on the operation being performed.  For example, imagine that ChildActor is a bit more complex, with a, b, c, and d members:

    ChildActor:
        form: function() {
            return {
                actor: this.actorID,
                greeting: this.greeting,
                a: this.a,
                b: this.b,
                c: this.c,
                d: this.d
            }
        }
    ChildFront:
        form: function(form) {
            this.actorID = form.actorID;
            this.id = form.id;
            this.a = form.a;
            this.b = form.b;
            this.c = form.c;
            this.d = form.d;
        }

And imagine you want to change 'c' and return the object:

    // Oops!  If a type is going to return references to itself or any other
    // type that isn't fully registered yet, you need to predeclare the type.
    types.addActorType("childActor");

    ...

    changeC: method(function(newC) {
        c = newC;
        return this;
    }, {
        request: { newC: Arg(0) },
        response: { self: RetVal("childActor") }
    });

    ...

    childFront.changeC('hello').then(ret => { assert(ret === childFront); assert(childFront.c === "hello") });

Now our response will look like:

    { from: <childActorID>, self: { actor: <childActorID>, greeting: <id>, a: <a>, b: <b>, c: "hello", d: <d> }

But that's wasteful.  Only c changed.  So we can provide a *detail* to the type using `#`:

    response: { self: RetVal("childActor#changec") }

and update our form methods to make use of that data:

    ChildActor:
    form: function(detail) {
        if (detail === "changec") {
            return { actor: this.actorID, c: this.c }
        }
        ... // the rest of the form method stays the same.
    }

    ChildFront:
    form: function(form, detail) {
        if (detail === "changec") {
            this.actorID = form.actor;
            this.c = form.c;
            return;
        }
        ... // the rest of the form method stays the same.
    }

Now the packet looks like a much more reasonable `{ from: <childActorID>, self: { actor: <childActorID>, c: "hello" } }`

Lifetimes
---------

No, I don't want to talk about lifetimes quite yet.

Events
------

Your actor has great news!

Actors are subclasses of jetpack `EventTarget`, so you can just emit:

    let event = require("sdk/core/event");

    giveGoodNews: method(function(news) {
        event.emit(this, "good-news", news);
    }, {
        request: { news: Arg(0) }
    });

... but nobody will really care, because that's not going over the protocol.  But you can describe the packet in an `events` member, the same way you would specify a request:

    events: {
        "good-news": {
            type: "goodNews", // event target naming and packet naming are at odds, and we want both to be natural!
            news: Arg(0)
        }
    }

And now you can listen to events on a front:

    front.on("good-news", news => {
        console.log("Got good news: " + news + "\n");
    });
    front.giveGoodNews().then(() => { console.log("request returned.") });

You might want to update your front's state when an event is fired, before emitting it against the front.  You can use `preEvent` in the front definition for that:

    countGoodNews: protocol.preEvent("good-news", function(news) {
        this.amountOfGoodNews++;
    });

On a somewhat related note, not every method needs to be request/response.  Just like an actor can emit a one-way event, a method can be marked as a one-way request.  Maybe we don't care about giveGoodNews returning anything:

    giveGoodNews: method(function(news) {
        emit(this, "good-news", news);
    }, {
        request: { news: Arg(0, "string") },
        oneway: true
    });

Lifetimes
---------

No, let's talk about custom front methods instead.

Custom Front Methods
--------------------

You might have some bookkeeping to do before issuing a request.  Let's say you're calling `echo`, but you want to count the number of times you issue that request.  Just use the `custom` tag in your front implementation:

    echo: custom(function(str) {
        this.numEchos++;
        return this._echo(str);
    }, {
        impl: "_echo"
    })

This puts the generated implementation in `_echo` instead of `echo`, letting you implement `echo` as needed.  If you leave out the `impl`, it just won't generate the implementation at all.  You're on your own.

Lifetimes
---------

OK, I can't think of any more ways to put this off.  The remote debugging protocol has the concept of a *parent* for each actor.  This is to make distributed memory management a bit easier.  Basically, any descendents of an actor will be destroyed if the actor is destroyed.

Other than that, the basic protocol makes no guarantees about lifetime.  Each interface defined in the protocol will need to discuss and document its approach to lifetime management (although there are a few common patterns).

The protocol library will maintain the child/parent relationships for you, but it needs some help deciding what the child/parent relationships are.

The default parent of an object is the first object that returns it after it is created.  So to revisit our earlier HelloActor `getChild` implementation:

    getChild: method(function(id) {
        return new ChildActor(this.conn, id);
    }, {
        request: { id: Arg(0) },
        response: { child: RetVal("childActor") }
    });

The ChildActor's parent is the HelloActor, because it's the one that created it.

You can customize this behavior in two ways.  The first is by defining a `marshallPool` property in your actor.  Imagine a new ChildActor method:

    getSibling: method(function(id) {
        return new ChildActor(this.conn, id);
    }, {
        request: { id: Arg(0) },
        response: { child: RetVal("childActor") }
    });

This creates a new child actor owned by the current child actor.  But in this example we want all actors created by the child to be owned by the HelloActor.  So we can define a `defaultParent` property that makes use of the `parent` proeprty provided by the Actor class:

    get marshallPool() { return this.parent }

The front needs to provide a matching `defaultParent` property that returns an owning front, to make sure the client and server lifetimes stay synced.

For more complex situations, you can define your own lifetime properties.  Take this new pair of HelloActor methods:

    // When the "temp" lifetime is specified, look for the _temporaryParent attribute as the owner.
    types.addLifetime("temp", "_temporaryParent");

    getTemporaryChild: method(function(id) {
        if (!this._temporaryParent) {
            // Create an actor to serve as the parent for all temporary children and explicitly
            // add it as a child of this actor.
            this._temporaryParent = this.manage(new Actor(this.conn));
        }
        return new ChildActor(this.conn, id);
    }, {
        request: { id: Arg(0) },
        response: {
            child: RetVal("temp:childActor") // use the lifetime name here to specify the expected lifetime.
        }
    });

    clearTemporaryChildren: method(function(id) {
        if (this._temporaryParent) {
            this._temporaryParent.destroy();
            delete this._temporaryParent;
        }
    });

This will require some matching work on the front:

    getTemporaryChild: protocol.custom(function(id) {
        if (!this._temporaryParent) {
            this._temporaryParent = this.manage(new Front(this.client));
        }
        return this._getTemporaryChild(id);
    }, {
        impl: "_getTemporaryChild"
    }),

    clearTemporaryChildren: protocol.custom(function(id) {
        if (this._temporaryParent) {
            this._temporaryParent.destroy();
            delete this._temporaryParent;
        }
        return this._clearTemporaryChildren();
    }, {
        impl: "_clearTemporaryChildren"
    })

Telemetry
---------

You can specify a telemetry probe id in your method spec:

    echo: method(function(str) {
        return str;
    }, {
        request: { str: Arg(0) },
        response: { str: RetVal() },
        telemetry: "ECHO"
    });

... and the time to execute that request will be included as a telemetry probe.
