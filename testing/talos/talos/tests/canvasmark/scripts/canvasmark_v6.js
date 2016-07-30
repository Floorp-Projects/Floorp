/**
 * Game class library, utility functions and globals.
 * 
 * @author Kevin Roast
 * 
 * 30/04/09 Initial version.
 * 12/05/09 Refactored to remove globals into GameHandler instance and added FPS controller game loop.
 * 17/01/11 Full screen resizable canvas
 * 26/01/11 World to screen transformation - no longer unit=pixel
 * 03/08/11 Modified version for CanvasMark usage
 */

var KEY = { SHIFT:16, CTRL:17, ESC:27, RIGHT:39, UP:38, LEFT:37, DOWN:40, SPACE:32,
            A:65, D:68, E:69, G:71, L:76, P:80, R:82, S:83, W:87, Z:90, OPENBRACKET:219, CLOSEBRACKET:221 };
var iOS = (navigator.userAgent.indexOf("iPhone;") != -1 ||
           navigator.userAgent.indexOf("iPod;") != -1 ||
           navigator.userAgent.indexOf("iPad;") != -1);

/**
 * Game Handler.
 * 
 * Singleton instance responsible for managing the main game loop and
 * maintaining a few global references such as the canvas and frame counters.
 */
var GameHandler =
{
   /**
    * The single Game.Main derived instance
    */
   game: null,
   
   /**
    * True if the game is in pause state, false if running
    */
   paused: false,
   
   /**
    * The single canvas play field element reference
    */
   canvas: null,
   
   /**
    * Width of the canvas play field
    */
   width: 0,
   
   /**
    * Height of the canvas play field
    */
   height: 0,
   
   offsetX: 0,
   
   offsetY: 0,
   
   /**
    * Frame counter
    */
   frameCount: 0,
   
   sceneStartTime: 0,
   benchmarkScoreCount: 0,
   benchmarkScores: [],
   benchmarkLabels: [],

   FPSMS: 60,
   FRAME_TIME_MAX: 1000/30,
   MAX_GLITCH_COUNT: 10,
   
   /**
    * Debugging output
    */
   maxfps: 0,
   frametime: 0,
   frameInterval: 0,
   
   /**
    * Init function called once by your window.onload handler
    */
   init: function()
   {
      this.canvas = document.getElementById('canvas');
      this.width = this.canvas.height;
      this.height = this.canvas.width;
      
      var me = GameHandler;
      var el = me.canvas, x = 0, y = 0; 
      do
      {
         y += el.offsetTop;
         x += el.offsetLeft;
      } while (el = el.offsetParent);
      
      // compute canvas offset including page view position
      me.offsetX = x - window.pageXOffset;
      me.offsetY = y - window.pageYOffset;
   },
   
   /**
    * Game start method - begins the main game loop.
    * Pass in the object that represent the game to execute.
    * Also called each frame by the main game loop unless paused.
    * 
    * @param {Game.Main} game main derived object handler
    */
   start: function(game)
   {
      if (game) this.game = game;
      GameHandler.game.frame();
   },
   
   /**
    * Game pause toggle method.
    */
   pause: function()
   {
      if (this.paused)
      {
         this.paused = false;
         GameHandler.game.frame();
      }
      else
      {
         this.paused = true;
      }
   }
};


/**
 * Game root namespace.
 *
 * @namespace Game
 */
if (typeof Game == "undefined" || !Game)
{
   var Game = {};
}


/**
 * Transform a vector from world coordinates to screen
 * 
 * @method worldToScreen
 * @return Vector or null if non visible
 */
Game.worldToScreen = function worldToScreen(vector, world, radiusx, radiusy)
{
   // transform a vector from the world to the screen
   radiusx = (radiusx ? radiusx : 0);
   radiusy = (radiusy ? radiusy : radiusx);
   var screenvec = null,
       viewx = vector.x - world.viewx,
       viewy = vector.y - world.viewy;
   if (viewx < world.viewsize + radiusx && viewy < world.viewsize + radiusy &&
       viewx > -radiusx && viewy > -radiusy)
   {
      screenvec = new Vector(viewx, viewy).scale(world.scale);
   }
   return screenvec;
};


/**
 * Game main loop class.
 * 
 * @namespace Game
 * @class Game.Main
 */
(function()
{
   Game.Main = function()
   {
      var me = this;
      
      document.onkeydown = function(event)
      {
         var keyCode = (event === null ? window.event.keyCode : event.keyCode);
         
         if (me.sceneIndex !== -1)
         {
            if (me.scenes[me.sceneIndex].onKeyDownHandler(keyCode))
            {
               // if the key is handled, prevent any further events
               if (event)
               {
                  event.preventDefault();
                  event.stopPropagation();
               }
            }
         }
      };
      
      document.onkeyup = function(event)
      {
         var keyCode = (event === null ? window.event.keyCode : event.keyCode);
         
         if (me.sceneIndex !== -1)
         {
            if (me.scenes[me.sceneIndex].onKeyUpHandler(keyCode))
            {
               // if the key is handled, prevent any further events
               if (event)
               {
                  event.preventDefault();
                  event.stopPropagation();
               }
            }
         }
      };
   };
   
   Game.Main.prototype =
   {
      scenes: [],
      
      startScene: null,
      
      endScene: null,
      
      currentScene: null,
      
      sceneIndex: -1,
      
      lastFrameStart: 0,
      
      interval: null,
      
      /**
       * Game frame method - called by window timeout.
       */
      frame: function frame()
      {
         var frameStart = Date.now();
         GameHandler.frameInterval = frameStart - GameHandler.frameStart;
         if (GameHandler.frameInterval === 0) GameHandler.frameInterval = 1;
         
         // calculate scene transition and current scene
         var currentScene = this.currentScene;
         if (currentScene === null)
         {
            // set to scene zero (game init)
            this.sceneIndex = 0;
            currentScene = this.scenes[0];
            currentScene._onInitScene();
            currentScene.onInitScene();
         }
         
         if ((currentScene.interval === null || currentScene.interval.complete) && currentScene.isComplete())
         {
            if (this.sceneIndex === 0)
            {
               // reset total score recorded during the benchmark
               GameHandler.benchmarkScoreCount = 0;
            }
            this.sceneIndex++;
            if (this.sceneIndex < this.scenes.length)
            {
               currentScene = this.scenes[this.sceneIndex];
            }
            else
            {
               this.sceneIndex = 0;
               currentScene = this.scenes[0];
            }
            currentScene._onInitScene();
            currentScene.onInitScene();
         }
         
         // get canvas context for a render pass
         var ctx = GameHandler.canvas.getContext('2d');
         
         // calculate viewport transform and offset against the world
         // we want to show a fixed number of world units in our viewport
         // so calculate the scaling factor to transform world to view
         currentScene.world.scale = GameHandler.width / currentScene.world.viewsize;
         
         // render the game and current scene
         if (currentScene.interval === null || currentScene.interval.complete)
         {
            currentScene.onBeforeRenderScene(currentScene._onBeforeRenderScene());
            currentScene.onRenderScene(ctx);
         }
         else
         {
            // for the benchmark we just clear the canvas
            ctx.clearRect(0, 0, GameHandler.width, GameHandler.height);
            currentScene.interval.intervalRenderer.call(currentScene, currentScene.interval, ctx);
         }
         
         // update global frame counter and current scene reference
         this.currentScene = currentScene;
         GameHandler.frameCount++;
         
         // calculate frame time and frame multiplier required for smooth animation
         var now = Date.now();
         GameHandler.frametime = now - frameStart;
         GameHandler.frameMultipler = GameHandler.frameInterval / GameHandler.FPSMS;
         GameHandler.frameStart = frameStart;
         
         // update last fps every few frames for debugging output
         if (GameHandler.frameCount % 16 === 0) GameHandler.lastfps = ~~(1000 / GameHandler.frameInterval);
         
         // IE9 does not support requestAnimationFrame so need to calc interval manually
         var ieinterval = 17 - (GameHandler.frametime);
         
         requestAnimFrame(GameHandler.start, ieinterval > 0 ? ieinterval : 1);
      },
      
      isGameOver: function isGameOver()
      {
         return false;
      }
   };
})();

// requestAnimFrame shim
window.requestAnimFrame = (function()
{
   return  window.requestAnimationFrame       || 
           window.webkitRequestAnimationFrame || 
           window.oRequestAnimationFrame      || 
           window.mozRequestAnimationFrame    || 
           window.msRequestAnimationFrame     || 
           function(callback, frameOffset)
           {
               window.setTimeout(callback, frameOffset);
           };
})();


/**
 * Game scene base class. Adapted for Benchmark scoring.
 * 
 * @namespace Game
 * @class Game.Scene
 */
(function()
{
   Game.Scene = function(playable, interval)
   {
      this.playable = playable;
      this.interval = interval;
   };
   
   Game.Scene.prototype =
   {
      playable: true,
      
      interval: null,

      sceneStartTime: null,
      sceneCompletedTime: null,
      sceneGlitchCount: 0,
      
      testState: 0,
      testScore: 0,
      
      world:
      {
         size: 1500,       // total units vertically and horizontally
         viewx: 0,         // current view left corner xpos
         viewy: 0,         // current view left corner ypos
         viewsize: 1500,   // size of the viewable area
         scale: 1  // scale for world->view transformation - calculated based on physical viewport size
      },
      
      /**
       * Return true if this scene should update the actor list.
       */
      isPlayable: function isPlayable()
      {
         return this.playable;
      },
      
      _onInitScene: function _onInitScene()
      {
         this.sceneGlitchCount = this.testScore = this.testState = 0;
         Profiler.resume(this.interval && this.interval.label);
         this.sceneStartTime = Date.now();
         this.sceneCompletedTime = null;
      },

      onInitScene: function onInitScene()
      {
         if (this.interval !== null)
         {
            // reset interval flag
            this.interval.reset();
         }
      },
      
      _onBeforeRenderScene: function _onBeforeRenderScene()
      {
         // calculate if the scene shoud render in benchmark mode or not
         if (this.playable)
         {
            if (!this.sceneCompletedTime)
            {
               if (GameHandler.frameInterval > GameHandler.FRAME_TIME_MAX)
               {
                  this.sceneGlitchCount++;
               }
               if (this.sceneGlitchCount < GameHandler.MAX_GLITCH_COUNT)
               {
                  return true;
               }
               else
               {
                  // too many FPS glitches! so benchmark scene completed (allow to run visually for a few seconds)
                  this.sceneCompletedTime = Date.now();
                  var score = ~~(((this.sceneCompletedTime - this.sceneStartTime) * this.testScore) / 100);
                  GameHandler.benchmarkScoreCount += score;
                  GameHandler.benchmarkScores.push(score);
                  var name = this.interval.label.replace(/Test [0-9] - /g, "");
                  name = name.replace(/, /g, "- ");
                  GameHandler.benchmarkLabels.push(name);
                  Profiler.pause(this.interval.label);
                  if (typeof console !== "undefined")
                  {
                     console.log(score + " [" + this.interval.label + "]");
                  }
               }
            }
         }
         return false;
      },
      
      getTransientTestScore: function getTransientTestScore()
      {
         var score = ((this.sceneCompletedTime ? this.sceneCompletedTime : Date.now()) - this.sceneStartTime) * this.testScore;
         return ~~(score/100);
      },
      
      onBeforeRenderScene: function onBeforeRenderScene()
      {
      },
      
      onRenderScene: function onRenderScene(ctx)
      {
      },
      
      onRenderInterval: function onRenderInterval(ctx)
      {
      },
      
      onKeyDownHandler: function onKeyDownHandler(keyCode)
      {
      },
      
      onKeyUpHandler: function onKeyUpHandler(keyCode)
      {
      },
      
      isComplete: function isComplete()
      {
         return this.sceneCompletedTime && (Date.now() > this.sceneCompletedTime);
      },
      
      intervalRenderer: function intervalRenderer(interval, ctx)
      {
         if (interval.framecounter++ < 100)
         {
            Game.centerFillText(ctx, interval.label, "14pt Courier New", GameHandler.height/2 - 8, "white");
         }
         else
         {
            interval.complete = true;
         }
      }
   };
})();


(function()
{
   Game.Interval = function(label, intervalRenderer)
   {
      this.label = label;
      this.intervalRenderer = intervalRenderer;
      this.framecounter = 0;
      this.complete = false;
   };
   
   Game.Interval.prototype =
   {
      label: null,
      intervalRenderer: null,
      framecounter: 0,
      complete: false,
      
      reset: function reset()
      {
         this.framecounter = 0;
         this.complete = false;
      }
   };
})();


/**
 * Actor base class.
 * 
 * Game actors have a position in the game world and a current vector to indicate
 * direction and speed of travel per frame. They each support the onUpdate() and
 * onRender() event methods, finally an actor has an expired() method which should
 * return true when the actor object should be removed from play.
 * 
 * An actor can be hit and destroyed by bullets or similar. The class supports a hit()
 * method which should return true when the actor should be removed from play.
 * 
 * @namespace Game
 * @class Game.Actor
 */
(function()
{
   Game.Actor = function(p, v)
   {
      this.position = p;
      this.vector = v;
      
      return this;
   };
   
   Game.Actor.prototype =
   {
      /**
       * Actor position
       *
       * @property position
       * @type Vector
       */
      position: null,
      
      /**
       * Actor vector
       *
       * @property vector
       * @type Vector
       */
      vector: null,
      
      /**
       * Alive flag
       *
       * @property alive
       * @type boolean
       */
      alive: true,
      
      /**
       * Radius - default is zero to imply that it is not affected by collision tests etc.
       *
       * @property radius
       * @type int
       */
      radius: 0,
      
      /**
       * Actor expiration test
       * 
       * @method expired
       * @return true if expired and to be removed from the actor list, false if still in play
       */
      expired: function expired()
      {
         return !(this.alive);
      },
      
      /**
       * Hit by bullet
       * 
       * @param force of the impacting bullet (as the actor may support health)
       * @return true if destroyed, false otherwise
       */
      hit: function hit(force)
      {
         this.alive = false;
         return true;
      },
      
      /**
       * Transform current position vector from world coordinates to screen.
       * Applies the appropriate translation and scaling to the canvas context.
       * 
       * @method worldToScreen
       * @return Vector or null if non visible
       */
      worldToScreen: function worldToScreen(ctx, world, radius)
      {
         var viewposition = Game.worldToScreen(this.position, world, radius);
         if (viewposition)
         {
            // scale ALL graphics... - translate to position apply canvas scaling
            ctx.translate(viewposition.x, viewposition.y);
            ctx.scale(world.scale, world.scale);
         }
         return viewposition;
      },
      
      /**
       * Actor game loop update event method. Called for each actor
       * at the start of each game loop cycle.
       * 
       * @method onUpdate
       */
      onUpdate: function onUpdate()
      {
      },
      
      /**
       * Actor rendering event method. Called for each actor to
       * render for each frame.
       * 
       * @method onRender
       * @param ctx {object} Canvas rendering context
       * @param world {object} World metadata
       */
      onRender: function onRender(ctx, world)
      {
      }
   };
})();


/**
 * SpriteActor base class.
 * 
 * An actor that can be rendered by a bitmap. The sprite handling code deals with the increment
 * of the current frame within the supplied bitmap sprite strip image, based on animation direction,
 * animation speed and the animation length before looping. Call renderSprite() each frame.
 * 
 * NOTE: by default sprites source images are 64px wide 64px by N frames high and scaled to the
 * appropriate final size. Any other size input source should be set in the constructor.
 * 
 * @namespace Game
 * @class Game.SpriteActor
 */
(function()
{
   Game.SpriteActor = function(p, v, s)
   {
      Game.SpriteActor.superclass.constructor.call(this, p, v);
      if (s) this.frameSize = s;
      
      return this;
   };
   
   extend(Game.SpriteActor, Game.Actor,
   {
      /**
       * Size in pixels of the width/height of an individual frame in the image
       */
      frameSize: 64,
      
      /**
       * Animation image sprite reference.
       * Sprite image sources are all currently 64px wide 64px by N frames high.
       */
      animImage: null,
      
      /**
       * Length in frames of the sprite animation
       */
      animLength: 0,
      
      /**
       * Animation direction, true for forward, false for reverse.
       */
      animForward: true,
      
      /**
       * Animation frame inc/dec speed.
       */
      animSpeed: 1.0,
      
      /**
       * Current animation frame index
       */
      animFrame: 0,
      
      /**
       * Render sprite graphic based on current anim image, frame and anim direction
       * Automatically updates the current anim frame.
       * 
       * Optionally this method will automatically correct for objects moving on/off
       * a cyclic canvas play area - if so it will render the appropriate stencil
       * sections of the sprite top/bottom/left/right as needed to complete the image.
       * Note that this feature can only be used if the sprite is absolutely positioned
       * and not translated/rotated into position by canvas operations.
       */
      renderSprite: function renderSprite(ctx, x, y, w, cyclic)
      {
         var offset = this.animFrame << 6,
             fs = this.frameSize;
         
         ctx.drawImage(this.animImage, 0, offset, fs, fs, x, y, w, w);
         
         if (cyclic)
         {
            if (x < 0 || y < 0)
            {
               ctx.drawImage(this.animImage, 0, offset, fs, fs,
                  (x < 0 ? (GameHandler.width + x) : x),
                  (y < 0 ? (GameHandler.height + y) : y),
                  w, w);
            }
            if (x + w >= GameHandler.width || y + w >= GameHandler.height)
            {
               ctx.drawImage(this.animImage, 0, offset, fs, fs,
                  (x + w >= GameHandler.width ? (x - GameHandler.width) : x),
                  (y + w >= GameHandler.height ? (y - GameHandler.height) : y),
                  w, w);
            }
         }
         
         // update animation frame index
         if (this.animForward)
         {
            this.animFrame += this.animSpeed;
            if (this.animFrame >= this.animLength)
            {
               this.animFrame = 0;
            }
         }
         else
         {
            this.animFrame -= this.animSpeed;
            if (this.animFrame < 0)
            {
               this.animFrame = this.animLength - 1;
            }
         }
      }
   });
})();


/**
 * EffectActor base class.
 * 
 * An actor representing a transient effect in the game world. An effect is nothing more than
 * a special graphic that does not play any direct part in the game and does not interact with
 * any other objects. It automatically expires after a set lifespan, generally the rendering of
 * the effect is based on the remaining lifespan.
 * 
 * @namespace Game
 * @class Game.EffectActor
 */
(function()
{
   Game.EffectActor = function(p, v, lifespan)
   {
      Game.EffectActor.superclass.constructor.call(this, p, v);
      this.lifespan = lifespan;
      return this;
   };
   
   extend(Game.EffectActor, Game.Actor,
   {
      /**
       * Effect lifespan remaining
       */
      lifespan: 0,
      
      /**
       * Actor expiration test
       * 
       * @return true if expired and to be removed from the actor list, false if still in play
       */
      expired: function expired()
      {
      	// deduct lifespan from the explosion
      	return (--this.lifespan === 0);
      }
   });
})();


/**
 * Image Preloader class. Executes the supplied callback function once all
 * registered images are loaded by the browser.
 * 
 * @namespace Game
 * @class Game.Preloader
 */
(function()
{
   Game.Preloader = function()
   {
      this.images = [];
      return this;
   };
   
   Game.Preloader.prototype =
   {
      /**
       * Image list
       *
       * @property images
       * @type Array
       */
      images: null,
      
      /**
       * Callback function
       *
       * @property callback
       * @type Function
       */
      callback: null,
      
      /**
       * Images loaded so far counter
       */
      counter: 0,
      
      /**
       * Add an image to the list of images to wait for
       */
      addImage: function addImage(img, url)
      {
         var me = this;
         img.url = url;
         // attach closure to the image onload handler
         img.onload = function()
         {
            me.counter++;
            if (me.counter === me.images.length)
            {
               // all images are loaded - execute callback function
               me.callback.call(me);
            }
         };
         this.images.push(img);
      },
      
      /**
       * Load the images and call the supplied function when ready
       */
      onLoadCallback: function onLoadCallback(fn)
      {
         this.counter = 0;
         this.callback = fn;
         // load the images
         for (var i=0, j=this.images.length; i<j; i++)
         {
            this.images[i].src = this.images[i].url;
         }
      }
   };
})();


/**
 * Render text into the canvas context.
 * Compatible with FF3, FF3.5, SF4, GC4, OP10
 * 
 * @method Game.drawText
 * @static
 */
Game.drawText = function(g, txt, font, x, y, col)
{
   g.save();
   if (col) g.strokeStyle = col;
   g.font = font;
   g.strokeText(txt, x, y);
   g.restore();
};

Game.fillText = function(g, txt, font, x, y, col)
{
   g.save();
   if (col) g.fillStyle = col;
   g.font = font;
   g.fillText(txt, x, y);
   g.restore();
};

Game.centerFillText = function(g, txt, font, y, col)
{
   g.save();
   if (col) g.fillStyle = col;
   g.font = font;
   g.fillText(txt, (GameHandler.width - g.measureText(txt).width) / 2, y);
   g.restore();
};

Game.fontSize = function fontSize(world, size)
{
   var s = ~~(size * world.scale * 2);
   if (s > 20) s = 20;
   else if (s < 8) s = 8;
   return s;
};

Game.fontFamily = function fontFamily(world, size, font)
{
   return Game.fontSize(world, size) + "pt " + (font ? font : "Courier New");
};/**
 * Feature test scenes for CanvasMark Rendering Benchmark - March 2013
 *  
 * (C) 2013 Kevin Roast kevtoast@yahoo.com @kevinroast
 * 
 * Please see: license.txt
 * You are welcome to use this code, but I would appreciate an email or tweet
 * if you do anything interesting with it!
 */


/**
 * Feature root namespace.
 * 
 * @namespace Feature
 */
if (typeof Feature == "undefined" || !Feature)
{
   var Feature = {};
}

Feature.textureImage = new Image();
Feature.blurImage = new Image();

/**
 * Feature main Benchmark Test class.
 * 
 * @namespace Feature
 * @class Feature.Test
 */
(function()
{
   Feature.Test = function(benchmark, loader)
   {
      loader.addImage(Feature.textureImage, "./images/texture5.png");
      loader.addImage(Feature.blurImage, "./images/fruit.jpg");

      // add benchmark scenes
      var t = benchmark.scenes.length;
      for (var i=0; i<3; i++)
      {
         benchmark.addBenchmarkScene(new Feature.GameScene(this, t+i, i));
      }
   };
   
   Feature.Test.prototype =
   {
   };
})();


(function()
{
   /**
    * Feature.K3DController constructor
    */
   Feature.K3DController = function()
   {
      Feature.K3DController.superclass.constructor.call(this);
   };
   
   extend(Feature.K3DController, K3D.BaseController,
   {
      /**
       * Render tick - should be called from appropriate scene renderer
       */
      render: function(ctx)
      {
         // execute super class method to process render pipelines
         ctx.save();
         ctx.translate(GameHandler.width/2, GameHandler.height/2);
         this.processFrame(ctx);
         ctx.restore();
      }
   });
})();


/**
 * Feature Game scene class.
 * 
 * @namespace Feature
 * @class Feature.GameScene
 */
(function()
{
   Feature.GameScene = function(game, test, feature)
   {
      this.game = game;
      this.test = test;
      this.feature = feature;
      
      var msg = "Test " + test + " - ";
      switch (feature)
      {
         case 0: msg += "Plasma - Maths, canvas shapes"; break;
         case 1: msg += "3D Rendering - Maths, polygons, image transforms"; break;
         case 2: msg += "Pixel blur - Math, getImageData, putImageData"; break;
      }
      var interval = new Game.Interval(msg, this.intervalRenderer);
      Feature.GameScene.superclass.constructor.call(this, true, interval);
   };
   
   extend(Feature.GameScene, Game.Scene,
   {
      feature: 0,
      index: 0,
      game: null,
      
      /**
       * Scene init event handler
       */
      onInitScene: function onInitScene()
      {
         switch (this.feature)
         {
            case 0:
            {
               // generate plasma palette
               var palette = [];
               for (var i=0,r,g,b; i<256; i++)
               {
                  r = ~~(128 + 128 * Math.sin(Math.PI * i / 32));
                  g = ~~(128 + 128 * Math.sin(Math.PI * i / 64));
                  b = ~~(128 + 128 * Math.sin(Math.PI * i / 128));
                  palette[i] = "rgb(" + ~~r + "," + ~~g + "," + ~~b + ")";
               }
               this.paletteoffset = 0;
               this.palette = palette;
               
               // size of the plasma pixels ratio - bigger = more calculations and rendering
               this.plasmasize = 8;
               
               this.testScore = 10;
               
               break;
            }
            
            case 1:
            {
               // K3D controller
               this.k3d = new Feature.K3DController();
               // generate 3D objects
               for (var i=0; i<10; i++)
               {
                  this.add3DObject(i);
               }
               
               this.testScore = 10;
               
               break;
            }
            
            case 2:
            {
               this.testScore = 25;
               break;
            }
         }
      },

      add3DObject: function add3DObject(offset)
      {
         var gap = 360/20;
         var obj = new K3D.K3DObject();
         obj.ophi = (360 / gap) * offset;
         obj.otheta = (180 / gap / 2) * offset;
         obj.textures.push(Feature.textureImage);
         with (obj)
         {
            drawmode = "solid";     // one of "point", "wireframe", "solid"
            shademode = "lightsource";    // one of "plain", "depthcue", "lightsource" (solid drawing mode only)
            addgamma = 0.5; addtheta = -1.0; addphi = -0.75;
            aboutx = 150; abouty = -150; aboutz = -50;
            perslevel = 512;
            scale = 13;
            init(
               // describe the points of a simple unit cube
               [{x:-1,y:1,z:-1}, {x:1,y:1,z:-1}, {x:1,y:-1,z:-1}, {x:-1,y:-1,z:-1}, {x:-1,y:1,z:1}, {x:1,y:1,z:1}, {x:1,y:-1,z:1}, {x:-1,y:-1,z:1}],
               // describe the edges of the cube
               [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:4,b:5}, {a:5,b:6}, {a:6,b:7}, {a:7,b:4}, {a:0,b:4}, {a:1,b:5}, {a:2,b:6}, {a:3,b:7}],
               // describe the polygon faces of the cube
               [{color:[255,0,0],vertices:[0,1,2,3],texture:0},{color:[0,255,0],vertices:[0,4,5,1]},{color:[0,0,255],vertices:[1,5,6,2]},{color:[255,255,0],vertices:[2,6,7,3]},{color:[0,255,255],vertices:[3,7,4,0]},{color:[255,0,255],vertices:[7,6,5,4],texture:0}]
            );
         }
         // add another 3D object to the controller
         this.k3d.addK3DObject(obj);
      },
      
      /**
       * Scene before rendering event handler
       */
      onBeforeRenderScene: function onBeforeRenderScene(benchmark)
      {
         if (benchmark)
         {
            switch (this.feature)
            {
               case 0:
               {
                  if (Date.now() - this.sceneStartTime > this.testState)
                  {
                     this.testState+=100;
                     this.plasmasize++;
                  }
                  break;
               }
               case 1:
               {
                  if (Date.now() - this.sceneStartTime > this.testState)
                  {
                     this.testState+=100;
                     this.add3DObject(this.k3d.objects.length);
                  }
                  break;
               }
               case 2:
               {
                  if (Date.now() - this.sceneStartTime > this.testState)
                  {
                     this.testState+=2;
                  }
                  break;
               }
            }
         }
      },
      
      /**
       * Scene rendering event handler
       */
      onRenderScene: function onRenderScene(ctx)
      {
         ctx.clearRect(0, 0, GameHandler.width, GameHandler.height);
         
         // render feature benchmark
         var width = GameHandler.width, height = GameHandler.height;
         switch (this.feature)
         {
            case 0:
            {
               var dist = function dist(a, b, c, d)
               {
                  return Math.sqrt((a - c) * (a - c) + (b - d) * (b - d));
               }
               
               // plasma source width and height - variable benchmark state
               var pwidth = this.plasmasize;
               var pheight = pwidth * (height/width);
               // scale the plasma source to the canvas width/height
               var vpx = width/pwidth, vpy = height/pheight;
               var time = Date.now() / 64;
               
               var colour = function colour(x, y)
               {
                  // plasma function
                  return (128 + (128 * Math.sin(x * 0.0625)) +
                          128 + (128 * Math.sin(y * 0.03125)) +
                          128 + (128 * Math.sin(dist(x + time, y - time, width, height) * 0.125)) +
                          128 + (128 * Math.sin(Math.sqrt(x * x + y * y) * 0.125)) ) * 0.25;
               }
               
               // render plasma effect
               for (var y=0,x; y<pheight; y++)
               {
                  for (x=0; x<pwidth; x++)
                  {
                     // map plasma pixels to canvas pixels using the virtual pixel size
                     ctx.fillStyle = this.palette[~~(colour(x, y) + this.paletteoffset) % 256];
                     ctx.fillRect(Math.floor(x * vpx), Math.floor(y * vpy), Math.ceil(vpx), Math.ceil(vpy));
                  }
               }
               
               // palette cycle speed
               this.paletteoffset++;
               break;
            }
            
            case 1:
            {
               this.k3d.render(ctx);
               break;
            }

            case 2:
            {
               //
               // TODO: add more interesting image!
               //
               var s = this.testState < GameHandler.width ? this.testState : GameHandler.width;
               ctx.drawImage(Feature.blurImage, 0, 0, GameHandler.width, GameHandler.height);
               boxBlurCanvasRGBA( ctx, 0, 0, s, s, s >> 4 + 1, 1 );
               break;
            }
         }
         
         ctx.save();
         ctx.shadowBlur = 0;
         // Benchmark - information output
         if (this.sceneCompletedTime)
         {
            Game.fillText(ctx, "TEST "+this.test+" COMPLETED: "+this.getTransientTestScore(), "20pt Courier New", 4, 40, "white");
         }
         Game.fillText(ctx, "SCORE: " + this.getTransientTestScore(), "12pt Courier New", 0, GameHandler.height - 42, "lightblue");
         Game.fillText(ctx, "TSF: " + Math.round(GameHandler.frametime) + "ms", "12pt Courier New", 0, GameHandler.height - 22, "lightblue");
         Game.fillText(ctx, "FPS: " + GameHandler.lastfps, "12pt Courier New", 0, GameHandler.height - 2, "lightblue");
         ctx.restore();
      }
   });
})();

/*
Superfast Blur - a fast Box Blur For Canvas

Version:    0.5
Author:     Mario Klingemann
Contact:    mario@quasimondo.com
Website: http://www.quasimondo.com/BoxBlurForCanvas
Twitter: @quasimondo

In case you find this class useful - especially in commercial projects -
I am not totally unhappy for a small donation to my PayPal account
mario@quasimondo.de

Or support me on flattr:
https://flattr.com/thing/140066/Superfast-Blur-a-pretty-fast-Box-Blur-Effect-for-CanvasJavascript

Copyright (c) 2011 Mario Klingemann

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/
var mul_table = [ 1,57,41,21,203,34,97,73,227,91,149,62,105,45,39,137,241,107,3,173,39,71,65,238,219,101,187,87,81,151,141,133,249,117,221,209,197,187,177,169,5,153,73,139,133,127,243,233,223,107,103,99,191,23,177,171,165,159,77,149,9,139,135,131,253,245,119,231,224,109,211,103,25,195,189,23,45,175,171,83,81,79,155,151,147,9,141,137,67,131,129,251,123,30,235,115,113,221,217,53,13,51,50,49,193,189,185,91,179,175,43,169,83,163,5,79,155,19,75,147,145,143,35,69,17,67,33,65,255,251,247,243,239,59,29,229,113,111,219,27,213,105,207,51,201,199,49,193,191,47,93,183,181,179,11,87,43,85,167,165,163,161,159,157,155,77,19,75,37,73,145,143,141,35,138,137,135,67,33,131,129,255,63,250,247,61,121,239,237,117,29,229,227,225,111,55,109,216,213,211,209,207,205,203,201,199,197,195,193,48,190,47,93,185,183,181,179,178,176,175,173,171,85,21,167,165,41,163,161,5,79,157,78,154,153,19,75,149,74,147,73,144,143,71,141,140,139,137,17,135,134,133,66,131,65,129,1];

var shg_table = [0,9,10,10,14,12,14,14,16,15,16,15,16,15,15,17,18,17,12,18,16,17,17,19,19,18,19,18,18,19,19,19,20,19,20,20,20,20,20,20,15,20,19,20,20,20,21,21,21,20,20,20,21,18,21,21,21,21,20,21,17,21,21,21,22,22,21,22,22,21,22,21,19,22,22,19,20,22,22,21,21,21,22,22,22,18,22,22,21,22,22,23,22,20,23,22,22,23,23,21,19,21,21,21,23,23,23,22,23,23,21,23,22,23,18,22,23,20,22,23,23,23,21,22,20,22,21,22,24,24,24,24,24,22,21,24,23,23,24,21,24,23,24,22,24,24,22,24,24,22,23,24,24,24,20,23,22,23,24,24,24,24,24,24,24,23,21,23,22,23,24,24,24,22,24,24,24,23,22,24,24,25,23,25,25,23,24,25,25,24,22,25,25,25,24,23,24,25,25,25,25,25,25,25,25,25,25,25,25,23,25,23,24,25,25,25,25,25,25,25,25,25,24,22,25,25,23,25,25,20,24,25,24,25,25,22,24,25,24,25,24,25,25,24,25,25,25,25,22,25,25,25,24,25,24,25,18];

function boxBlurCanvasRGBA( context, top_x, top_y, width, height, radius, iterations ){
   radius |= 0;
   
   var imageData = context.getImageData( top_x, top_y, width, height );
   var pixels = imageData.data;
   
   var rsum,gsum,bsum,asum,x,y,i,p,p1,p2,yp,yi,yw,idx,pa;      
   var wm = width - 1;
   var hm = height - 1;
   var wh = width * height;
   var rad1 = radius + 1;
    
   var mul_sum = mul_table[radius];
   var shg_sum = shg_table[radius];

   var r = [];
   var g = [];
   var b = [];
   var a = [];
   
   var vmin = [];
   var vmax = [];
  
   while ( iterations-- > 0 ){
      yw = yi = 0;
    
      for ( y=0; y < height; y++ ){
         rsum = pixels[yw]   * rad1;
         gsum = pixels[yw+1] * rad1;
         bsum = pixels[yw+2] * rad1;
         asum = pixels[yw+3] * rad1;
         
         for( i = 1; i <= radius; i++ ){
            p = yw + (((i > wm ? wm : i )) << 2 );
            rsum += pixels[p++];
            gsum += pixels[p++];
            bsum += pixels[p++];
            asum += pixels[p]
         }
         
         for ( x = 0; x < width; x++ ) {
            r[yi] = rsum;
            g[yi] = gsum;
            b[yi] = bsum;
            a[yi] = asum;

            if( y==0) {
               vmin[x] = ( ( p = x + rad1) < wm ? p : wm ) << 2;
               vmax[x] = ( ( p = x - radius) > 0 ? p << 2 : 0 );
            } 
            
            p1 = yw + vmin[x];
            p2 = yw + vmax[x];
              
            rsum += pixels[p1++] - pixels[p2++];
            gsum += pixels[p1++] - pixels[p2++];
            bsum += pixels[p1++] - pixels[p2++];
            asum += pixels[p1]   - pixels[p2];
                
            yi++;
         }
         yw += ( width << 2 );
      }
     
      for ( x = 0; x < width; x++ ) {
         yp = x;
         rsum = r[yp] * rad1;
         gsum = g[yp] * rad1;
         bsum = b[yp] * rad1;
         asum = a[yp] * rad1;
         
         for( i = 1; i <= radius; i++ ) {
           yp += ( i > hm ? 0 : width );
           rsum += r[yp];
           gsum += g[yp];
           bsum += b[yp];
           asum += a[yp];
         }
         
         yi = x << 2;
         for ( y = 0; y < height; y++) {
            
            pixels[yi+3] = pa = (asum * mul_sum) >>> shg_sum;
            if ( pa > 0 )
            {
               pa = 255 / pa;
               pixels[yi]   = ((rsum * mul_sum) >>> shg_sum) * pa;
               pixels[yi+1] = ((gsum * mul_sum) >>> shg_sum) * pa;
               pixels[yi+2] = ((bsum * mul_sum) >>> shg_sum) * pa;
            } else {
               pixels[yi] = pixels[yi+1] = pixels[yi+2] = 0;
            }           
            if( x == 0 ) {
               vmin[y] = ( ( p = y + rad1) < hm ? p : hm ) * width;
               vmax[y] = ( ( p = y - radius) > 0 ? p * width : 0 );
            } 
           
            p1 = x + vmin[y];
            p2 = x + vmax[y];

            rsum += r[p1] - r[p2];
            gsum += g[p1] - g[p2];
            bsum += b[p1] - b[p2];
            asum += a[p1] - a[p2];

            yi += width << 2;
         }
      }
   }
   
   context.putImageData( imageData, top_x, top_y );
}/**
 * Asteroids HTML5 Canvas Game
 * Scenes for CanvasMark Rendering Benchmark - March 2013 
 *
 * @email kevtoast at yahoo dot com
 * @twitter kevinroast
 *
 * (C) 2013 Kevin Roast
 * 
 * Please see: license.txt
 * You are welcome to use this code, but I would appreciate an email or tweet
 * if you do anything interesting with it!
 */


// Globals
var BITMAPS = true;
var GLOWEFFECT = false;
var g_asteroidImg1 = new Image();
var g_asteroidImg2 = new Image();
var g_asteroidImg3 = new Image();
var g_asteroidImg4 = new Image();
var g_shieldImg = new Image();
var g_backgroundImg = new Image();
var g_playerImg = new Image();
var g_enemyshipImg = new Image();


/**
 * Asteroids root namespace.
 * 
 * @namespace Asteroids
 */
if (typeof Asteroids == "undefined" || !Asteroids)
{
   var Asteroids = {};
}


/**
 * Asteroids benchmark test class.
 * 
 * @namespace Asteroids
 * @class Asteroids.Test
 */
(function()
{
   Asteroids.Test = function(benchmark, loader)
   {
      // get the image graphics loading
      loader.addImage(g_backgroundImg, './images/bg3_1.jpg');
      loader.addImage(g_playerImg, './images/player.png');
      loader.addImage(g_asteroidImg1, './images/asteroid1.png');
      loader.addImage(g_asteroidImg2, './images/asteroid2.png');
      loader.addImage(g_asteroidImg3, './images/asteroid3.png');
      loader.addImage(g_asteroidImg4, './images/asteroid4.png');
      loader.addImage(g_enemyshipImg, './images/enemyship1.png');
      
      // generate the single player actor - available across all scenes
      this.player = new Asteroids.Player(new Vector(GameHandler.width / 2, GameHandler.height / 2), new Vector(0.0, 0.0), 0.0);
      
      // add the Asteroid game benchmark scenes
      for (var level, i=0, t=benchmark.scenes.length; i<4; i++)
      {
         level = new Asteroids.BenchMarkScene(this, t+i, i+1);// NOTE: asteroids indexes feature from 1...
         benchmark.addBenchmarkScene(level);
      }
   };
   
   Asteroids.Test.prototype =
   {
      /**
       * Reference to the single game player actor
       */
      player: null,
      
      /**
       * Lives count (only used to render overlay graphics during benchmark mode)
       */
      lives: 3
   };
})();


/**
 * Asteroids Benchmark scene class.
 * 
 * @namespace Asteroids
 * @class Asteroids.BenchMarkScene
 */
(function()
{
   Asteroids.BenchMarkScene = function(game, test, feature)
   {
      this.game = game;
      this.test = test;
      this.feature = feature;
      this.player = game.player;
      
      var msg = "Test " + test + " - Asteroids - ";
      switch (feature)
      {
         case 1: msg += "Bitmaps"; break;
         case 2: msg += "Vectors"; break;
         case 3: msg += "Bitmaps, shapes, text"; break;
         case 4: msg += "Shapes, shadows, blending"; break;
      }
      var interval = new Game.Interval(msg, this.intervalRenderer);
      Asteroids.BenchMarkScene.superclass.constructor.call(this, true, interval);
      
      // generate background starfield
      for (var star, i=0; i<this.STARFIELD_SIZE; i++)
      {
         star = new Asteroids.Star();
         star.init();
         this.starfield.push(star);
      }
   };
   
   extend(Asteroids.BenchMarkScene, Game.Scene,
   {
      STARFIELD_SIZE: 32,
      
      game: null,
      
      test: 0,
      
      /**
       * Local reference to the game player actor
       */
      player: null,
      
      /**
       * Top-level list of game actors sub-lists
       */
      actors: null,
      
      /**
       * List of player fired bullet actors
       */
      playerBullets: null,
      
      /**
       * List of enemy actors (asteroids, ships etc.)
       */
      enemies: null,
      
      /**
       * List of enemy fired bullet actors
       */
      enemyBullets: null,
      
      /**
       * List of effect actors
       */
      effects: null,
      
      /**
       * Background scrolling bitmap x position
       */
      backgroundX: 0,
      
      /**
       * Background starfield star list
       */
      starfield: [],
      
      /**
       * Update each individual star in the starfield background
       */
      updateStarfield: function updateStarfield(ctx)
      {
         for (var s, i=0, j=this.starfield.length; i<j; i++)
         {
            s = this.starfield[i];
            s.render(ctx);
            s.z -= s.VELOCITY * 0.1;
            if (s.z < 0.1 || s.prevx > GameHandler.height || s.prevy > GameHandler.width)
            {
               s.init();
            }
         }
      },
      
      /**
       * Scene init event handler
       */
      onInitScene: function onInitScene()
      {
         // generate the actors and add the actor sub-lists to the main actor list
         this.actors = [];
         this.enemies = [];
         this.actors.push(this.enemies);
         this.actors.push(this.playerBullets = []);
         this.actors.push(this.enemyBullets = []);
         this.actors.push(this.effects = []);
         
         this.actors.push([this.player]);
         
         // reset the player position
         with (this.player)
         {
            position.x = GameHandler.width / 2;
            position.y = GameHandler.height / 2;
            vector.x = 0.0;
            vector.y = 0.0;
            heading = 0.0;
         }
         
         // tests 1-2 display asteroids in various modes
         switch (this.feature)
         {
            case 1:
            {
               // start with 10 asteroids - more will be added if framerate is acceptable
               for (var i=0; i<10; i++)
               {
                  this.enemies.push(this.generateAsteroid(Math.random()+1.0, ~~(Math.random()*4) + 1));
               }
               this.testScore = 10;
               break;
            }
            case 2:
            {
               // start with 10 asteroids - more will be added if framerate is acceptable
               for (var i=0; i<10; i++)
               {
                  this.enemies.push(this.generateAsteroid(Math.random()+1.0, ~~(Math.random()*4) + 1));
               }
               this.testScore = 20;
               break;
            }
            case 3:
            {
               // test 3 generates lots of enemy ships that fire
               for (var i=0; i<10; i++)
               {
                  this.enemies.push(new Asteroids.EnemyShip(this, i%2));
               }
               this.testScore = 10;
               break;
            }
            case 4:
            {
               this.testScore = 25;
               break;
            }
         }
         
         // tests 2 in wireframe, all others are bitmaps
         BITMAPS = !(this.feature === 2);
         
         // reset interval flag
         this.interval.reset();
      },
      
      /**
       * Scene before rendering event handler
       */
      onBeforeRenderScene: function onBeforeRenderScene(benchmark)
      {
         // add items to the test
         if (benchmark)
         {
            switch (this.feature)
            {
               case 1:
               case 2:
               {
                  var count;
                  switch (this.feature)
                  {
                     case 1:
                        count = 10;
                        break;
                     case 2:
                        count = 5;
                        break;
                  }
                  for (var i=0; i<count; i++)
                  {
                     this.enemies.push(this.generateAsteroid(Math.random()+1.0, ~~(Math.random()*4) + 1));
                  }
                  break;
               }
               case 3:
               {
                  if (Date.now() - this.sceneStartTime > this.testState)
                  {
                     this.testState += 20;
                     for (var i=0; i<2; i++)
                     {
                        this.enemies.push(new Asteroids.EnemyShip(this, i%2));
                     }
                     this.enemies[0].hit();
                     this.destroyEnemy(this.enemies[0], new Vector(0, 1));
                  }
                  break;
               }
               case 4:
               {
                  if (Date.now() - this.sceneStartTime > this.testState)
                  {
                     this.testState += 25;
                     
                     // spray forward guns
                     for (var i=0; i<=~~(this.testState/500); i++)
                     {
                        h = this.player.heading - 15;
                        t = new Vector(0.0, -7.0).rotate(h * RAD).add(this.player.vector);
                        this.playerBullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h));
                        h = this.player.heading;
                        t = new Vector(0.0, -7.0).rotate(h * RAD).add(this.player.vector);
                        this.playerBullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h));
                        h = this.player.heading + 15;
                        t = new Vector(0.0, -7.0).rotate(h * RAD).add(this.player.vector);
                        this.playerBullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h));
                     }
                     
                     // side firing guns also
                     h = this.player.heading - 90;
                     t = new Vector(0.0, -8.0).rotate(h * RAD).add(this.player.vector);
                     this.playerBullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h, 25));
                     
                     h = this.player.heading + 90;
                     t = new Vector(0.0, -8.0).rotate(h * RAD).add(this.player.vector);
                     this.playerBullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h, 25));
                     
                     // update player heading to rotate
                     this.player.heading += 8;
                  }
                  break;
               }
            }
         }
         
         // update all actors using their current vector
         this.updateActors();
      },
      
      /**
       * Scene rendering event handler
       */
      onRenderScene: function onRenderScene(ctx)
      {
         // setup canvas for a render pass and apply background
         if (BITMAPS)
         {
            // draw a scrolling background image
            ctx.drawImage(g_backgroundImg, this.backgroundX++, 0, GameHandler.width, GameHandler.height, 0, 0, GameHandler.width, GameHandler.height);
            if (this.backgroundX == (g_backgroundImg.width / 2))
            {
               this.backgroundX = 0;
            }
            ctx.shadowBlur = 0;
         }
         else
         {
            // clear the background to black
            ctx.fillStyle = "black";
            ctx.fillRect(0, 0, GameHandler.width, GameHandler.height);
            
            // glowing vector effect shadow
            ctx.shadowBlur = GLOWEFFECT ? 8 : 0;
            
            // update and render background starfield effect
            this.updateStarfield(ctx);
         }
         
         // render the game actors
         this.renderActors(ctx);
         
         // render info overlay graphics
         this.renderOverlay(ctx);
      },
      
      /**
       * Randomly generate a new large asteroid. Ensures the asteroid is not generated
       * too close to the player position!
       * 
       * @param speedFactor {number} Speed multiplier factor to apply to asteroid vector
       */
      generateAsteroid: function generateAsteroid(speedFactor, size)
      {
         while (true)
         {
            // perform a test to check it is not too close to the player
            var apos = new Vector(Math.random()*GameHandler.width, Math.random()*GameHandler.height);
            if (this.player.position.distance(apos) > 125)
            {
               var vec = new Vector( ((Math.random()*2)-1)*speedFactor, ((Math.random()*2)-1)*speedFactor );
               var asteroid = new Asteroids.Asteroid(
                  apos, vec, size ? size : 4);
               return asteroid;
            }
         }
      },
      
      /**
       * Update the actors position based on current vectors and expiration.
       */
      updateActors: function updateActors()
      {
         for (var i = 0, j = this.actors.length; i < j; i++)
         {
            var actorList = this.actors[i];
            
            for (var n = 0; n < actorList.length; n++)
            {
               var actor = actorList[n];
               
               // call onUpdate() event for each actor
               actor.onUpdate(this);
               
               // expiration test first
               if (actor.expired())
               {
                  actorList.splice(n, 1);
               }
               else
               {
                  // update actor using its current vector
                  actor.position.add(actor.vector);
                  
                  // handle traversing out of the coordinate space and back again
                  if (actor.position.x >= GameHandler.width)
                  {
                     actor.position.x = 0;
                  }
                  else if (actor.position.x < 0)
                  {
                     actor.position.x = GameHandler.width - 1;
                  }
                  if (actor.position.y >= GameHandler.height)
                  {
                     actor.position.y = 0;
                  }
                  else if (actor.position.y < 0)
                  {
                     actor.position.y = GameHandler.height - 1;
                  }
               }
            }
         }
      },
      
      /**
       * Blow up an enemy.
       * 
       * An asteroid may generate new baby asteroids and leave an explosion
       * in the wake.
       * 
       * Also applies the score for the destroyed item.
       * 
       * @param enemy {Game.Actor} The enemy to destory and add score for
       * @param parentVector {Vector} The vector of the item that hit the enemy
       * @param player {boolean} If true, the player was the destroyed
       */
      destroyEnemy: function destroyEnemy(enemy, parentVector)
      {
         if (enemy instanceof Asteroids.Asteroid)
         {
            // generate baby asteroids
            this.generateBabyAsteroids(enemy, parentVector);
            
            // add an explosion actor at the asteriod position and vector
            var boom = new Asteroids.Explosion(enemy.position.clone(), enemy.vector.clone(), enemy.size);
            this.effects.push(boom);
            
            // generate a score effect indicator at the destroyed enemy position
            var vec = new Vector(0, -(Math.random()*2 + 0.5));
            var effect = new Asteroids.ScoreIndicator(
                  new Vector(enemy.position.x, enemy.position.y), vec, Math.floor(100 + (Math.random()*100)));
            this.effects.push(effect);
         }
         else if (enemy instanceof Asteroids.EnemyShip)
         {
            // add an explosion actor at the asteriod position and vector
            var boom = new Asteroids.Explosion(enemy.position.clone(), enemy.vector.clone(), 4);
            this.effects.push(boom);
            
            // generate a score text effect indicator at the destroyed enemy position
            var vec = new Vector(0, -(Math.random()*2 + 0.5));
            var effect = new Asteroids.ScoreIndicator(
                  new Vector(enemy.position.x, enemy.position.y), vec, Math.floor(100 + (Math.random()*100)));
            this.effects.push(effect);
         }
      },
      
      /**
       * Generate a number of baby asteroids from a detonated parent asteroid. The number
       * and size of the generated asteroids are based on the parent size. Some of the
       * momentum of the parent vector (e.g. impacting bullet) is applied to the new asteroids.
       *
       * @param asteroid {Asteroids.Asteroid} The parent asteroid that has been destroyed
       * @param parentVector {Vector} Vector of the impacting object e.g. a bullet
       */
      generateBabyAsteroids: function generateBabyAsteroids(asteroid, parentVector)
      {
         // generate some baby asteroid(s) if bigger than the minimum size
         if (asteroid.size > 1)
         {
            for (var x=0, xc=Math.floor(asteroid.size/2); x<xc; x++)
            {
               var babySize = asteroid.size - 1;
               
               var vec = asteroid.vector.clone();
               
               // apply a small random vector in the direction of travel
               var t = new Vector(0.0, -(Math.random() * 3));
               
               // rotate vector by asteroid current heading - slightly randomized
               t.rotate(asteroid.vector.theta() * (Math.random()*Math.PI));
               vec.add(t);
               
               // add the scaled parent vector - to give some momentum from the impact
               vec.add(parentVector.clone().scale(0.2));
               
               // create the asteroid - slightly offset from the centre of the old one
               var baby = new Asteroids.Asteroid(
                     new Vector(asteroid.position.x + (Math.random()*5)-2.5, asteroid.position.y + (Math.random()*5)-2.5),
                     vec, babySize, asteroid.type);
               this.enemies.push(baby);
            }
         }
      },
      
      /**
       * Render each actor to the canvas.
       * 
       * @param ctx {object} Canvas rendering context
       */
      renderActors: function renderActors(ctx)
      {
         for (var i = 0, j = this.actors.length; i < j; i++)
         {
            // walk each sub-list and call render on each object
            var actorList = this.actors[i];
            
            for (var n = actorList.length - 1; n >= 0; n--)
            {
               actorList[n].onRender(ctx);
            }
         }
      },
      
      /**
       * Render player information HUD overlay graphics.
       * 
       * @param ctx {object} Canvas rendering context
       */
      renderOverlay: function renderOverlay(ctx)
      {
         ctx.save();
         
         // energy bar (100 pixels across, scaled down from player energy max)
         ctx.strokeStyle = "rgb(50,50,255)";
         ctx.strokeRect(4, 4, 101, 6);
         ctx.fillStyle = "rgb(100,100,255)";
         var energy = this.player.energy;
         if (energy > this.player.ENERGY_INIT)
         {
            // the shield is on for "free" briefly when he player respawns
            energy = this.player.ENERGY_INIT;
         }
         ctx.fillRect(5, 5, (energy / (this.player.ENERGY_INIT / 100)), 5);
         
         // lives indicator graphics
         for (var i=0; i<this.game.lives; i++)
         {
            if (BITMAPS)
            {
               ctx.drawImage(g_playerImg, 0, 0, 64, 64, 350+(i*20), 0, 16, 16);
            }
            else
            {
               ctx.save();
               ctx.shadowColor = ctx.strokeStyle = "rgb(255,255,255)";
               ctx.translate(360+(i*16), 8);
               ctx.beginPath();
               ctx.moveTo(-4, 6);
               ctx.lineTo(4, 6);
               ctx.lineTo(0, -6);
               ctx.closePath();
               ctx.stroke();
               ctx.restore();
            }
         }
         
         // score display
         Game.fillText(ctx, "00000000", "12pt Courier New", 120, 12, "white");
         
         // high score
         Game.fillText(ctx, "HI: 00000000", "12pt Courier New", 220, 12, "white");
         
         // Benchmark - information output
         if (this.sceneCompletedTime)
         {
            Game.fillText(ctx, "TEST " + this.feature + " COMPLETED: " + this.getTransientTestScore(), "20pt Courier New", 4, 40, "white");
         }
         Game.fillText(ctx, "SCORE: " + this.getTransientTestScore(), "12pt Courier New", 0, GameHandler.height - 42, "lightblue");
         Game.fillText(ctx, "TSF: " + Math.round(GameHandler.frametime) + "ms", "12pt Courier New", 0, GameHandler.height - 22, "lightblue");
         Game.fillText(ctx, "FPS: " + GameHandler.lastfps, "12pt Courier New", 0, GameHandler.height - 2, "lightblue");
         
         ctx.restore();
      }
   });
})();


/**
 * Starfield star class.
 * 
 * @namespace Asteroids
 * @class Asteroids.Star
 */
(function()
{
   Asteroids.Star = function()
   {
      return this;
   };
   
   Asteroids.Star.prototype =
   {
      MAXZ: 12.0,
      VELOCITY: 1.5,
      MAXSIZE: 5,
      
      x: 0,
      y: 0,
      z: 0,
      prevx: 0,
      prevy: 0,
      
      init: function init()
      {
         // select a random point for the initial location
         this.x = (Math.random() * GameHandler.width - (GameHandler.width * 0.5)) * this.MAXZ;
         this.y = (Math.random() * GameHandler.height - (GameHandler.height * 0.5)) * this.MAXZ;
         this.z = this.MAXZ;
      },
      
      render: function render(ctx)
      {
         var xx = this.x / this.z;
         var yy = this.y / this.z;
         
         var size = 1.0 / this.z * this.MAXSIZE + 1;
         
         ctx.save();
         ctx.fillStyle = "rgb(200,200,200)";
         ctx.beginPath();
         ctx.arc(xx + (GameHandler.width / 2), yy +(GameHandler.height / 2), size/2, 0, TWOPI, true);
         ctx.closePath();
         ctx.fill();
         ctx.restore();
         
         this.prevx = xx;
         this.prevy = yy;
      }
   };
})();
/**
 * Player actor class.
 *
 * @namespace Asteroids
 * @class Asteroids.Player
 */
(function()
{
   Asteroids.Player = function(p, v, h)
   {
      Asteroids.Player.superclass.constructor.call(this, p, v);
      this.heading = h;
      this.energy = this.ENERGY_INIT;
      
      // setup SpriteActor values - used for shield sprite
      this.animImage = g_shieldImg;
      this.animLength = this.SHIELD_ANIM_LENGTH;
      
      // setup weapons
      this.primaryWeapons = [];
      this.primaryWeapons["main"] = new Asteroids.PrimaryWeapon(this);
      
      return this;
   };
   
   extend(Asteroids.Player, Game.SpriteActor,
   {
      MAX_PLAYER_VELOCITY: 10.0,
      PLAYER_RADIUS: 10,
      SHIELD_RADIUS: 14,
      SHIELD_ANIM_LENGTH: 100,
      SHIELD_MIN_PULSE: 20,
      ENERGY_INIT: 200,
      THRUST_DELAY: 1,
      BOMB_RECHARGE: 20,
      BOMB_ENERGY: 40,
      
      /**
       * Player heading
       */
      heading: 0.0,
      
      /**
       * Player energy (shield and bombs)
       */
      energy: 0,
      
      /**
       * Player shield active counter
       */
      shieldCounter: 0,
      
      /**
       * Player 'alive' flag
       */
      alive: true,
      
      /**
       * Primary weapon list
       */
      primaryWeapons: null,
      
      /**
       * Bomb fire recharging counter
       */
      bombRecharge: 0,
      
      /**
       * Engine thrust recharge counter
       */
      thrustRecharge: 0,
      
      /**
       * True if the engine thrust graphics should be rendered next frame
       */
      engineThrust: false,
      
      /**
       * Frame that the player was killed on - to cause a delay before respawning the player
       */
      killedOnFrame: 0,
      
      /**
       * Power up setting - can fire when shielded
       */
      fireWhenShield: false,
      
      /**
       * Player rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         var headingRad = this.heading * RAD;
         
         // render engine thrust?
         if (this.engineThrust)
         {
            ctx.save();
            ctx.translate(this.position.x, this.position.y);
            ctx.rotate(headingRad);
            ctx.globalAlpha = 0.4 + Math.random()/2;
            if (BITMAPS)
            {
               ctx.fillStyle = "rgb(25,125,255)";
            }
            else
            {
               ctx.shadowColor = ctx.strokeStyle = "rgb(25,125,255)";
            }
            ctx.beginPath();
            ctx.moveTo(-5, 8);
            ctx.lineTo(5, 8);
            ctx.lineTo(0, 15 + Math.random()*7);
            ctx.closePath();
            if (BITMAPS) ctx.fill(); else ctx.stroke();
            ctx.restore();
            this.engineThrust = false;
         }
         
         // render player graphic
         if (BITMAPS)
         {
            var size = (this.PLAYER_RADIUS * 2) + 4;
            var normAngle = this.heading % 360;
            if (normAngle < 0)
            {
               normAngle = 360 + normAngle;
            }
            ctx.drawImage(g_playerImg, 0, normAngle * 16, 64, 64, this.position.x - (size / 2), this.position.y - (size / 2), size, size);
         }
         else
         {
            ctx.save();
            ctx.shadowColor = ctx.strokeStyle = "rgb(255,255,255)";
            ctx.translate(this.position.x, this.position.y);
            ctx.rotate(headingRad);
            ctx.beginPath();
            ctx.moveTo(-6, 8);
            ctx.lineTo(6, 8);
            ctx.lineTo(0, -8);
            ctx.closePath();
            ctx.stroke();
            ctx.restore();
         }
         
         // shield up? if so render a shield graphic around the ship
         if (this.shieldCounter > 0 && this.energy > 0)
         {
            if (BITMAPS)
            {
               // render shield graphic bitmap
               ctx.save();
               ctx.translate(this.position.x, this.position.y);
               ctx.rotate(headingRad);
               this.renderSprite(ctx, -this.SHIELD_RADIUS-1, -this.SHIELD_RADIUS-1, (this.SHIELD_RADIUS * 2) + 2);
               ctx.restore();
            }
            else
            {
               // render shield as a simple circle around the ship
               ctx.save();
               ctx.translate(this.position.x, this.position.y);
               ctx.rotate(headingRad);
               ctx.shadowColor = ctx.strokeStyle = "rgb(100,100,255)";
               ctx.beginPath();
               ctx.arc(0, 2, this.SHIELD_RADIUS, 0, TWOPI, true);
               ctx.closePath();
               ctx.stroke();
               ctx.restore();
            }
            
            this.shieldCounter--;
            this.energy--;
         }
      },
      
      /**
       * Execute player forward thrust request
       * Automatically a delay is used between each application - to ensure stable thrust on all machines.
       */
      thrust: function thrust()
      {
         // now test we did not thrust too recently - to stop fast key repeat issues
         if (GameHandler.frameCount - this.thrustRecharge > this.THRUST_DELAY)
         {
            // update last frame count
            this.thrustRecharge = GameHandler.frameCount;
            
            // generate a small thrust vector
            var t = new Vector(0.0, -0.55);
            
            // rotate thrust vector by player current heading
            t.rotate(this.heading * RAD);
            
            // test player won't then exceed maximum velocity
            var pv = this.vector.clone();
            if (pv.add(t).length() < this.MAX_PLAYER_VELOCITY)
            {
               // add to current vector (which then gets applied during each render loop)
               this.vector.add(t);
            }
         }
         // mark so that we know to render engine thrust graphics
         this.engineThrust = true;
      },
      
      /**
       * Execute player active shield request
       * If energy remaining the shield will be briefly applied - or until key is release
       */
      activateShield: function activateShield()
      {
         // ensure shield stays up for a brief pulse between key presses!
         if (this.energy > 0)
         {
            this.shieldCounter = this.SHIELD_MIN_PULSE;
         }
      },
      
      isShieldActive: function isShieldActive()
      {
         return (this.shieldCounter > 0 && this.energy > 0);
      },
      
      radius: function radius()
      {
         return (this.isShieldActive() ? this.SHIELD_RADIUS : this.PLAYER_RADIUS);
      },
      
      expired: function expired()
      {
         return !(this.alive);
      },
      
      kill: function kill()
      {
         this.alive = false;
         this.killedOnFrame = GameHandler.frameCount;
      },
      
      /**
       * Fire primary weapon(s)
       * @param bulletList {Array} to add bullet(s) to on success
       */
      firePrimary: function firePrimary(bulletList)
      {
         // attempt to fire the primary weapon(s)
         // first ensure player is alive and the shield is not up
         if (this.alive && (!this.isShieldActive() || this.fireWhenShield))
         {
            for (var w in this.primaryWeapons)
            {
               var b = this.primaryWeapons[w].fire();
               if (b)
               {
                  if (isArray(b))
                  {
                     for (var i=0; i<b.length; i++)
                     {
                        bulletList.push(b[i]);
                     }
                  }
                  else
                  {
                     bulletList.push(b);
                  }
               }
            }
         }
      },
      
      /**
       * Fire secondary weapon.
       * @param bulletList {Array} to add bullet to on success
       */
      fireSecondary: function fireSecondary(bulletList)
      {
         // attempt to fire the secondary weapon and generate bomb object if successful
         // first ensure player is alive and the shield is not up
         if (this.alive && (!this.isShieldActive() || this.fireWhenShield) && this.energy > this.BOMB_ENERGY)
         {
            // now test we did not fire too recently
            if (GameHandler.frameCount - this.bombRecharge > this.BOMB_RECHARGE)
            {
               // ok, update last fired frame and we can now generate a bomb
               this.bombRecharge = GameHandler.frameCount;
               
               // decrement energy supply
               this.energy -= this.BOMB_ENERGY;
               
               // generate a vector rotated to the player heading and then add the current player
               // vector to give the bomb the correct directional momentum
               var t = new Vector(0.0, -6.0);
               t.rotate(this.heading * RAD);
               t.add(this.vector);
               
               bulletList.push(new Asteroids.Bomb(this.position.clone(), t));
            }
         }
      },
      
      onUpdate: function onUpdate()
      {
         // slowly recharge the shield - if not active
         if (!this.isShieldActive() && this.energy < this.ENERGY_INIT)
         {
            this.energy += 0.1;
         }
      },
      
      reset: function reset(persistPowerUps)
      {
         // reset energy, alive status, weapons and power up flags
         this.alive = true;
         if (!persistPowerUps)
         {
            this.primaryWeapons = [];
            this.primaryWeapons["main"] = new Asteroids.PrimaryWeapon(this);
            this.fireWhenShield = false;
         }
         this.energy = this.ENERGY_INIT + this.SHIELD_MIN_PULSE;  // for shield as below
         
         // active shield briefly
         this.activateShield();
      }
   });
})();
/**
 * Asteroid actor class.
 *
 * @namespace Asteroids
 * @class Asteroids.Asteroid
 */
(function()
{
   Asteroids.Asteroid = function(p, v, s, t)
   {
      Asteroids.Asteroid.superclass.constructor.call(this, p, v);
      this.size = s;
      this.health = s;
      
      // randomly select an asteroid image bitmap
      if (t === undefined)
      {
         t = randomInt(1, 4);
      }
      eval("this.animImage=g_asteroidImg" + t);
      this.type = t;
      
      // randomly setup animation speed and direction
      this.animForward = (Math.random() < 0.5);
      this.animSpeed = 0.25 + Math.random();
      this.animLength = this.ANIMATION_LENGTH;
      this.rotation = randomInt(0, 180);
      this.rotationSpeed = randomInt(-1, 1) / 25;
      
      return this;
   };
   
   extend(Asteroids.Asteroid, Game.SpriteActor,
   {
      ANIMATION_LENGTH: 180,
      
      /**
       * Asteroid size - values from 1-4 are valid.
       */
      size: 0,
      
      /**
       * Asteroid type i.e. which bitmap it is drawn from
       */
      type: 1,
      
      /**
       * Asteroid health before it's destroyed
       */
      health: 0,
      
      /**
       * Retro graphics mode rotation orientation and speed
       */
      rotation: 0,
      rotationSpeed: 0,
      
      /**
       * Asteroid rendering method
       */
      onRender: function onRender(ctx)
      {
         var rad = this.size * 8;
         ctx.save();
         if (BITMAPS)
         {
            // render asteroid graphic bitmap
            // bitmap is rendered slightly large than the radius as the raytraced asteroid graphics do not
            // quite touch the edges of the 64x64 sprite - this improves perceived collision detection
            this.renderSprite(ctx, this.position.x - rad - 2, this.position.y - rad - 2, (rad * 2)+4, true);
         }
         else
         {
            // draw asteroid outline circle
            ctx.shadowColor = ctx.strokeStyle = "white";
            ctx.translate(this.position.x, this.position.y);
            ctx.scale(this.size * 0.8, this.size * 0.8);
            ctx.rotate(this.rotation += this.rotationSpeed);
            ctx.lineWidth = (0.8 / this.size) * 2;
            ctx.beginPath();
            // asteroid wires
            switch (this.type)
            {
               case 1:
                  ctx.moveTo(0,10);
                  ctx.lineTo(8,6);
                  ctx.lineTo(10,-4);
                  ctx.lineTo(4,-2);
                  ctx.lineTo(6,-6);
                  ctx.lineTo(0,-10);
                  ctx.lineTo(-10,-3);
                  ctx.lineTo(-10,5);
                  break;
               case 2:
                  ctx.moveTo(0,10);
                  ctx.lineTo(8,6);
                  ctx.lineTo(10,-4);
                  ctx.lineTo(4,-2);
                  ctx.lineTo(6,-6);
                  ctx.lineTo(0,-10);
                  ctx.lineTo(-8,-8);
                  ctx.lineTo(-6,-3);
                  ctx.lineTo(-8,-4);
                  ctx.lineTo(-10,5);
                  break;
               case 3:
                  ctx.moveTo(-4,10);
                  ctx.lineTo(1,8);
                  ctx.lineTo(7,10);
                  ctx.lineTo(10,-4);
                  ctx.lineTo(4,-2);
                  ctx.lineTo(6,-6);
                  ctx.lineTo(0,-10);
                  ctx.lineTo(-10,-3);
                  ctx.lineTo(-10,5);
                  break;
               case 4:
                  ctx.moveTo(-8,10);
                  ctx.lineTo(7,8);
                  ctx.lineTo(10,-2);
                  ctx.lineTo(6,-10);
                  ctx.lineTo(-2,-8);
                  ctx.lineTo(-6,-10);
                  ctx.lineTo(-10,-6);
                  ctx.lineTo(-7,0);
                  break;
            }
            ctx.closePath();
            ctx.stroke();
         }
         ctx.restore();
      },
      
      radius: function radius()
      {
         return this.size * 8;
      },
      
      /**
       * Asteroid hit by player bullet
       * 
       * @param force of the impacting bullet, -1 for instant kill
       * @return true if destroyed, false otherwise
       */
      hit: function hit(force)
      {
         if (force !== -1)
         {
            this.health -= force;
         }
         else
         {
            // instant kill
            this.health = 0;
         }
         return !(this.alive = (this.health > 0));
      }
   });
})();


/**
 * Enemy Ship actor class.
 * 
 * @namespace Asteroids
 * @class Asteroids.EnemyShip
 */
(function()
{
   Asteroids.EnemyShip = function(scene, size)
   {
      this.size = size;
      
      // small ship, alter settings slightly
      if (this.size === 1)
      {
         this.BULLET_RECHARGE = 45;
         this.RADIUS = 8;
      }
      
      // randomly setup enemy initial position and vector
      // ensure the enemy starts in the opposite quadrant to the player
      var p, v;
      if (scene.player.position.x < GameHandler.width / 2)
      {
         // player on left of the screen
         if (scene.player.position.y < GameHandler.height / 2)
         {
            // player in top left of the screen
            p = new Vector(GameHandler.width-48, GameHandler.height-48);
         }
         else
         {
            // player in bottom left of the screen
            p = new Vector(GameHandler.width-48, 48);
         }
         v = new Vector(-(Math.random() + 1 + size), Math.random() + 0.5 + size);
      }
      else
      {
         // player on right of the screen
         if (scene.player.position.y < GameHandler.height / 2)
         {
            // player in top right of the screen
            p = new Vector(0, GameHandler.height-48);
         }
         else
         {
            // player in bottom right of the screen
            p = new Vector(0, 48);
         }
         v = new Vector(Math.random() + 1 + size, Math.random() + 0.5 + size);
      }
      
      // setup SpriteActor values
      this.animImage = g_enemyshipImg;
      this.animLength = this.SHIP_ANIM_LENGTH;
      
      Asteroids.EnemyShip.superclass.constructor.call(this, p, v);
      
      return this;
   };
   
   extend(Asteroids.EnemyShip, Game.SpriteActor,
   {
      SHIP_ANIM_LENGTH: 90,
      RADIUS: 16,
      BULLET_RECHARGE: 60,
      
      /**
       * Enemy ship size - 0 = large (slow), 1 = small (fast)
       */
      size: 0,
      
      /**
       * Bullet fire recharging counter
       */
      bulletRecharge: 0,
      
      onUpdate: function onUpdate(scene)
      {
         // change enemy direction randomly
         if (this.size === 0)
         {
            if (Math.random() < 0.01)
            {
               this.vector.y = -(this.vector.y + (0.25 - (Math.random()/2)));
            }
         }
         else
         {
            if (Math.random() < 0.02)
            {
               this.vector.y = -(this.vector.y + (0.5 - Math.random()));
            }
         }
         
         // regular fire a bullet at the player
         if (GameHandler.frameCount - this.bulletRecharge > this.BULLET_RECHARGE && scene.player.alive)
         {
            // ok, update last fired frame and we can now generate a bullet
            this.bulletRecharge = GameHandler.frameCount;
            
            // generate a vector pointed at the player
            // by calculating a vector between the player and enemy positions
            var v = scene.player.position.clone().sub(this.position);
            // scale resulting vector down to bullet vector size
            var scale = (this.size === 0 ? 5.0 : 6.0) / v.length();
            v.x *= scale;
            v.y *= scale;
            // slightly randomize the direction (big ship is less accurate also)
            v.x += (this.size === 0 ? (Math.random() * 2 - 1) : (Math.random() - 0.5));
            v.y += (this.size === 0 ? (Math.random() * 2 - 1) : (Math.random() - 0.5));
            // - could add the enemy motion vector for correct momentum
            // - but problem is this leads to slow bullets firing back from dir of travel
            // - so pretend that enemies are clever enough to account for this...
            //v.add(this.vector);
            
            var bullet = new Asteroids.EnemyBullet(this.position.clone(), v);
            scene.enemyBullets.push(bullet);
         }
      },
      
      /**
       * Enemy rendering method
       */
      onRender: function onRender(ctx)
      {
         if (BITMAPS)
         {
            // render enemy graphic bitmap
            var rad = this.RADIUS + 2;
            this.renderSprite(ctx, this.position.x - rad, this.position.y - rad, rad * 2, true);
         }
         else
         {
            ctx.save();
            ctx.translate(this.position.x, this.position.y);
            if (this.size === 0)
            {
               ctx.scale(2, 2);
            }
            
            ctx.beginPath();
            ctx.moveTo(0, -4);
            ctx.lineTo(8, 3);
            ctx.lineTo(0, 8);
            ctx.lineTo(-8, 3);
            ctx.lineTo(0, -4);
            ctx.closePath();
            ctx.shadowColor = ctx.strokeStyle = "rgb(100,150,100)";
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(0, -8);
            ctx.lineTo(4, -4);
            ctx.lineTo(0, 0);
            ctx.lineTo(-4, -4);
            ctx.lineTo(0, -8);
            ctx.closePath();
            ctx.shadowColor = ctx.strokeStyle = "rgb(150,200,150)";
            ctx.stroke();
            
            ctx.restore();
         }
      },
      
      radius: function radius()
      {
         return this.RADIUS;
      }
   });
})();
/**
 * Weapon system base class for the player actor.
 * 
 * @namespace Asteroids
 * @class Asteroids.Weapon
 */
(function()
{
   Asteroids.Weapon = function(player)
   {
      this.player = player;
      return this;
   };
   
   Asteroids.Weapon.prototype =
   {
      WEAPON_RECHARGE: 3,
      weaponRecharge: 0,
      player: null,
      
      fire: function()
      {
         // now test we did not fire too recently
         if (GameHandler.frameCount - this.weaponRecharge > this.WEAPON_RECHARGE)
         {
            // ok, update last fired frame and we can now generate a bullet
            this.weaponRecharge = GameHandler.frameCount;
            
            return this.doFire();
         }
      },
      
      doFire: function()
      {
      }
   };
})();


/**
 * Basic primary weapon for the player actor.
 * 
 * @namespace Asteroids
 * @class Asteroids.PrimaryWeapon
 */
(function()
{
   Asteroids.PrimaryWeapon = function(player)
   {
      Asteroids.PrimaryWeapon.superclass.constructor.call(this, player);
      return this;
   };
   
   extend(Asteroids.PrimaryWeapon, Asteroids.Weapon,
   {
      doFire: function()
      {
         // generate a vector rotated to the player heading and then add the current player
         // vector to give the bullet the correct directional momentum
         var t = new Vector(0.0, -8.0);
         t.rotate(this.player.heading * RAD);
         t.add(this.player.vector);
         
         return new Asteroids.Bullet(this.player.position.clone(), t, this.player.heading);
      }
   });
})();


/**
 * Twin Cannons primary weapon for the player actor.
 * 
 * @namespace Asteroids
 * @class Asteroids.TwinCannonsWeapon
 */
(function()
{
   Asteroids.TwinCannonsWeapon = function(player)
   {
      Asteroids.TwinCannonsWeapon.superclass.constructor.call(this, player);
      return this;
   };
   
   extend(Asteroids.TwinCannonsWeapon, Asteroids.Weapon,
   {
      doFire: function()
      {
         var t = new Vector(0.0, -8.0);
         t.rotate(this.player.heading * RAD);
         t.add(this.player.vector);
         
         return new Asteroids.BulletX2(this.player.position.clone(), t, this.player.heading);
      }
   });
})();


/**
 * V Spray Cannons primary weapon for the player actor.
 * 
 * @namespace Asteroids
 * @class Asteroids.VSprayCannonsWeapon
 */
(function()
{
   Asteroids.VSprayCannonsWeapon = function(player)
   {
      this.WEAPON_RECHARGE = 5;
      Asteroids.VSprayCannonsWeapon.superclass.constructor.call(this, player);
      return this;
   };
   
   extend(Asteroids.VSprayCannonsWeapon, Asteroids.Weapon,
   {
      doFire: function()
      {
         var t, h;
         
         var bullets = [];
         
         h = this.player.heading - 15;
         t = new Vector(0.0, -7.0).rotate(h * RAD).add(this.player.vector);
         bullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h));
         
         h = this.player.heading;
         t = new Vector(0.0, -7.0).rotate(h * RAD).add(this.player.vector);
         bullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h));
         
         h = this.player.heading + 15;
         t = new Vector(0.0, -7.0).rotate(h * RAD).add(this.player.vector);
         bullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h));
         
         return bullets;
      }
   });
})();


/**
 * Side Guns additional primary weapon for the player actor.
 * 
 * @namespace Asteroids
 * @class Asteroids.SideGunWeapon
 */
(function()
{
   Asteroids.SideGunWeapon = function(player)
   {
      this.WEAPON_RECHARGE = 5;
      Asteroids.SideGunWeapon.superclass.constructor.call(this, player);
      return this;
   };
   
   extend(Asteroids.SideGunWeapon, Asteroids.Weapon,
   {
      doFire: function()
      {
         var t, h;
         
         var bullets = [];
         
         h = this.player.heading - 90;
         t = new Vector(0.0, -8.0).rotate(h * RAD).add(this.player.vector);
         bullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h, 25));
         
         h = this.player.heading + 90;
         t = new Vector(0.0, -8.0).rotate(h * RAD).add(this.player.vector);
         bullets.push(new Asteroids.Bullet(this.player.position.clone(), t, h, 25));
         
         return bullets;
      }
   });
})();


/**
 * Rear Gun additional primary weapon for the player actor.
 * 
 * @namespace Asteroids
 * @class Asteroids.RearGunWeapon
 */
(function()
{
   Asteroids.RearGunWeapon = function(player)
   {
      this.WEAPON_RECHARGE = 5;
      Asteroids.RearGunWeapon.superclass.constructor.call(this, player);
      return this;
   };
   
   extend(Asteroids.RearGunWeapon, Asteroids.Weapon,
   {
      doFire: function()
      {
         var t = new Vector(0.0, -8.0);
         var h = this.player.heading + 180;
         t.rotate(h * RAD);
         t.add(this.player.vector);
         
         return new Asteroids.Bullet(this.player.position.clone(), t, h, 25);
      }
   });
})();


/**
 * Player Bullet actor class.
 *
 * @namespace Asteroids
 * @class Asteroids.Bullet
 */
(function()
{
   Asteroids.Bullet = function(p, v, h, lifespan)
   {
      Asteroids.Bullet.superclass.constructor.call(this, p, v);
      this.heading = h;
      if (lifespan)
      {
         this.lifespan = lifespan;
      }
      return this;
   };
   
   extend(Asteroids.Bullet, Game.Actor,
   {
      BULLET_WIDTH: 2,
      BULLET_HEIGHT: 6,
      FADE_LENGTH: 5,
      
      /**
       * Bullet heading
       */
      heading: 0.0,
      
      /**
       * Bullet lifespan remaining
       */
      lifespan: 40,
      
      /**
       * Bullet power energy
       */
      powerLevel: 1,
      
      /**
       * Bullet rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         var width = this.BULLET_WIDTH;
         var height = this.BULLET_HEIGHT;
         ctx.save();
         ctx.globalCompositeOperation = "lighter";
         if (this.lifespan < this.FADE_LENGTH)
         {
            // fade out
            ctx.globalAlpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
         }
         if (BITMAPS)
         {
            ctx.shadowBlur = 8;
            ctx.shadowColor = ctx.fillStyle = "rgb(50,255,50)";
         }
         else
         {
            ctx.shadowColor = ctx.strokeStyle = "rgb(50,255,50)";
         }
         // rotate the little bullet rectangle into the correct heading
         ctx.translate(this.position.x, this.position.y);
         ctx.rotate(this.heading * RAD);
         var x = -(width / 2);
         var y = -(height / 2);
         if (BITMAPS)
         {
            ctx.fillRect(x, y, width, height);
            ctx.fillRect(x, y+1, width, height-1);
         }
         else
         {
            ctx.strokeRect(x, y-1, width, height+1);
            ctx.strokeRect(x, y, width, height);
         }
         ctx.restore();
      },
      
      /**
       * Actor expiration test
       * 
       * @return true if expired and to be removed from the actor list, false if still in play
       */
      expired: function expired()
      {
         // deduct lifespan from the bullet
         return (--this.lifespan === 0);
      },
      
      /**
       * Area effect weapon radius - zero for primary bullets
       */
      effectRadius: function effectRadius()
      {
         return 0;
      },
      
      radius: function radius()
      {
         // approximate based on average between width and height
         return (this.BULLET_HEIGHT + this.BULLET_WIDTH) / 2;
      },
      
      power: function power()
      {
         return this.powerLevel;
      }
   });
})();


/**
 * Player BulletX2 actor class. Used by the Twin Cannons primary weapon.
 *
 * @namespace Asteroids
 * @class Asteroids.BulletX2
 */
(function()
{
   Asteroids.BulletX2 = function(p, v, h)
   {
      Asteroids.BulletX2.superclass.constructor.call(this, p, v, h);
      this.lifespan = 50;
      this.powerLevel = 2;
      return this;
   };
   
   extend(Asteroids.BulletX2, Asteroids.Bullet,
   {
      /**
       * Bullet rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         var width = this.BULLET_WIDTH;
         var height = this.BULLET_HEIGHT;
         ctx.save();
         ctx.globalCompositeOperation = "lighter";
         if (this.lifespan < this.FADE_LENGTH)
         {
            // fade out
            ctx.globalAlpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
         }
         if (BITMAPS)
         {
            ctx.shadowBlur = 8;
            ctx.shadowColor = ctx.fillStyle = "rgb(50,255,128)";
         }
         else
         {
            ctx.shadowColor = ctx.strokeStyle = "rgb(50,255,128)";
         }
         // rotate the little bullet rectangle into the correct heading
         ctx.translate(this.position.x, this.position.y);
         ctx.rotate(this.heading * RAD);
         var x = -(width / 2);
         var y = -(height / 2);
         if (BITMAPS)
         {
            ctx.fillRect(x - 4, y, width, height);
            ctx.fillRect(x - 4, y+1, width, height-1);
            ctx.fillRect(x + 4, y, width, height);
            ctx.fillRect(x + 4, y+1, width, height-1);
         }
         else
         {
            ctx.strokeRect(x - 4, y-1, width, height+1);
            ctx.strokeRect(x - 4, y, width, height);
            ctx.strokeRect(x + 4, y-1, width, height+1);
            ctx.strokeRect(x + 4, y, width, height);
         }
         ctx.restore();
      },
      
      radius: function radius()
      {
         // double width bullets - so bigger hit area than basic ones
         return (this.BULLET_HEIGHT);
      }
   });
})();


/**
 * Bomb actor class.
 *
 * @namespace Asteroids
 * @class Asteroids.Bomb
 */
(function()
{
   Asteroids.Bomb = function(p, v)
   {
      Asteroids.Bomb.superclass.constructor.call(this, p, v);
      return this;
   };
   
   extend(Asteroids.Bomb, Asteroids.Bullet,
   {
      BOMB_RADIUS: 4,
      FADE_LENGTH: 5,
      EFFECT_RADIUS: 45,
      
      /**
       * Bomb lifespan remaining
       */
      lifespan: 80,
      
      /**
       * Bomb rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         var rad = this.BOMB_RADIUS;
         ctx.save();
         ctx.globalCompositeOperation = "lighter";
         var alpha = 0.8;
         if (this.lifespan < this.FADE_LENGTH)
         {
            // fade out
            alpha = (0.8 / this.FADE_LENGTH) * this.lifespan;
            rad = (this.BOMB_RADIUS / this.FADE_LENGTH) * this.lifespan;
         }
         ctx.globalAlpha = alpha;
         if (BITMAPS)
         {
            ctx.fillStyle = "rgb(155,255,155)";
         }
         else
         {
            ctx.shadowColor = ctx.strokeStyle = "rgb(155,255,155)";
         }
         ctx.translate(this.position.x, this.position.y);
         ctx.rotate(GameHandler.frameCount % 360);
         // account for the fact that stroke() draws around the shape
         if (!BITMAPS) ctx.scale(0.8, 0.8);
         ctx.beginPath()
         ctx.moveTo(rad * 2, 0);
         for (var i=0; i<15; i++)
         {
            ctx.rotate(Math.PI / 8);
            if (i % 2 == 0)
            {
               ctx.lineTo((rad * 2 / 0.525731) * 0.200811, 0);
            }
            else
            {
               ctx.lineTo(rad * 2, 0);
            }
         }
         ctx.closePath();
         if (BITMAPS) ctx.fill(); else ctx.stroke();
         ctx.restore();
      },
      
      /**
       * Area effect weapon radius
       */
      effectRadius: function effectRadius()
      {
         return this.EFFECT_RADIUS;
      },
      
      radius: function radius()
      {
         var rad = this.BOMB_RADIUS;
         if (this.lifespan <= this.FADE_LENGTH)
         {
            rad = (this.BOMB_RADIUS / this.FADE_LENGTH) * this.lifespan;
         }
         return rad;
      }
   });
})();


/**
 * Enemy Bullet actor class.
 *
 * @namespace Asteroids
 * @class Asteroids.EnemyBullet
 */
(function()
{
   Asteroids.EnemyBullet = function(p, v)
   {
      Asteroids.EnemyBullet.superclass.constructor.call(this, p, v);
      return this;
   };
   
   extend(Asteroids.EnemyBullet, Game.Actor,
   {
      BULLET_RADIUS: 4,
      FADE_LENGTH: 5,
      
      /**
       * Bullet lifespan remaining
       */
      lifespan: 60,
      
      /**
       * Bullet rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         var rad = this.BULLET_RADIUS;
         ctx.save();
         ctx.globalCompositeOperation = "lighter";
         var alpha = 0.7;
         if (this.lifespan < this.FADE_LENGTH)
         {
            // fade out and make smaller
            alpha = (0.7 / this.FADE_LENGTH) * this.lifespan;
            rad = (this.BULLET_RADIUS / this.FADE_LENGTH) * this.lifespan;
         }
         ctx.globalAlpha = alpha;
         if (BITMAPS)
         {
            ctx.fillStyle = "rgb(150,255,150)";
         }
         else
         {
            ctx.shadowColor = ctx.strokeStyle = "rgb(150,255,150)";
         }
         
         ctx.beginPath();
         ctx.arc(this.position.x, this.position.y, (rad-1 > 0 ? rad-1 : 0.1), 0, TWOPI, true);
         ctx.closePath();
         if (BITMAPS) ctx.fill(); else ctx.stroke();
         
         ctx.translate(this.position.x, this.position.y);
         ctx.rotate((GameHandler.frameCount % 720) / 2);
         ctx.beginPath()
         ctx.moveTo(rad * 2, 0);
         for (var i=0; i<7; i++)
         {
            ctx.rotate(Math.PI/4);
            if (i%2 == 0)
            {
               ctx.lineTo((rad * 2/0.525731) * 0.200811, 0);
            }
            else
            {
               ctx.lineTo(rad * 2, 0);
            }
         }
         ctx.closePath();
         if (BITMAPS) ctx.fill(); else ctx.stroke();
         
         ctx.restore();
      },
      
      /**
       * Actor expiration test
       * 
       * @return true if expired and to be removed from the actor list, false if still in play
       */
      expired: function expired()
      {
         // deduct lifespan from the bullet
         return (--this.lifespan === 0);
      },
      
      radius: function radius()
      {
         var rad = this.BULLET_RADIUS;
         if (this.lifespan <= this.FADE_LENGTH)
         {
            rad = (rad / this.FADE_LENGTH) * this.lifespan;
         }
         return rad;
      }
   });
})();
/**
 * Basic explosion effect actor class.
 * 
 * @namespace Asteroids
 * @class Asteroids.Explosion
 */
(function()
{
   Asteroids.Explosion = function(p, v, s)
   {
      Asteroids.Explosion.superclass.constructor.call(this, p, v, this.FADE_LENGTH);
      this.size = s;
      return this;
   };
   
   extend(Asteroids.Explosion, Game.EffectActor,
   {
      FADE_LENGTH: 10,
      
      /**
       * Explosion size
       */
      size: 0,
      
      /**
       * Explosion rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         // fade out
         var brightness = Math.floor((255 / this.FADE_LENGTH) * this.lifespan);
         var rad = (this.size * 8 / this.FADE_LENGTH) * this.lifespan;
         var rgb = brightness.toString();
         ctx.save();
         ctx.globalAlpha = 0.75;
         ctx.fillStyle = "rgb(" + rgb + ",0,0)";
         ctx.beginPath();
         ctx.arc(this.position.x, this.position.y, rad, 0, TWOPI, true);
         ctx.closePath();
         ctx.fill();
         ctx.restore();
      }
   });
})();


/**
 * Player Explosion effect actor class.
 * 
 * @namespace Asteroids
 * @class Asteroids.PlayerExplosion
 */
(function()
{
   Asteroids.PlayerExplosion = function(p, v)
   {
      Asteroids.PlayerExplosion.superclass.constructor.call(this, p, v, this.FADE_LENGTH);
      return this;
   };
   
   extend(Asteroids.PlayerExplosion, Game.EffectActor,
   {
      FADE_LENGTH: 15,
      
      /**
       * Explosion rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         ctx.save();
         var alpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
         ctx.globalCompositeOperation = "lighter";
         ctx.globalAlpha = alpha;
         
         var rad;
         if (this.lifespan > 5 && this.lifespan <= 15)
         {
            var offset = this.lifespan - 5;
            rad = (48 / this.FADE_LENGTH) * offset;
            ctx.fillStyle = "rgb(255,170,30)";
            ctx.beginPath();
            ctx.arc(this.position.x-2, this.position.y-2, rad, 0, TWOPI, true);
            ctx.closePath();
            ctx.fill();
         }
         
         if (this.lifespan > 2 && this.lifespan <= 12)
         {
            var offset = this.lifespan - 2;
            rad = (32 / this.FADE_LENGTH) * offset;
            ctx.fillStyle = "rgb(255,255,50)";
            ctx.beginPath();
            ctx.arc(this.position.x+2, this.position.y+2, rad, 0, TWOPI, true);
            ctx.closePath();
            ctx.fill();
         }
         
         if (this.lifespan <= 10)
         {
            var offset = this.lifespan;
            rad = (24 / this.FADE_LENGTH) * offset;
            ctx.fillStyle = "rgb(255,70,100)";
            ctx.beginPath();
            ctx.arc(this.position.x+2, this.position.y-2, rad, 0, TWOPI, true);
            ctx.closePath();
            ctx.fill();
         }
         
         ctx.restore();
      }
   });
})();


/**
 * Impact effect (from bullet hitting an object) actor class.
 * 
 * @namespace Asteroids
 * @class Asteroids.Impact
 */
(function()
{
   Asteroids.Impact = function(p, v)
   {
      Asteroids.Impact.superclass.constructor.call(this, p, v, this.FADE_LENGTH);
      return this;
   };
   
   extend(Asteroids.Impact, Game.EffectActor,
   {
      FADE_LENGTH: 12,
      
      /**
       * Impact effect rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         // fade out alpha
         var alpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
         ctx.save();
         ctx.globalAlpha = alpha * 0.75;
         if (BITMAPS)
         {
            ctx.fillStyle = "rgb(50,255,50)";
         }
         else
         {
            ctx.shadowColor = ctx.strokeStyle = "rgb(50,255,50)";
         }
         ctx.beginPath();
         ctx.arc(this.position.x, this.position.y, 2, 0, TWOPI, true);
         ctx.closePath();
         if (BITMAPS) ctx.fill(); else ctx.stroke();
         ctx.globalAlpha = alpha;
         ctx.beginPath();
         ctx.arc(this.position.x, this.position.y, 1, 0, TWOPI, true);
         ctx.closePath();
         if (BITMAPS) ctx.fill(); else ctx.stroke();
         ctx.restore();
      }
   });
})();


/**
 * Text indicator effect actor class.
 * 
 * @namespace Asteroids
 * @class Asteroids.TextIndicator
 */
(function()
{
   Asteroids.TextIndicator = function(p, v, msg, textSize, colour, fadeLength)
   {
      this.fadeLength = (fadeLength ? fadeLength : this.DEFAULT_FADE_LENGTH);
      Asteroids.TextIndicator.superclass.constructor.call(this, p, v, this.fadeLength);
      this.msg = msg;
      if (textSize)
      {
         this.textSize = textSize;
      }
      if (colour)
      {
         this.colour = colour;
      }
      return this;
   };
   
   extend(Asteroids.TextIndicator, Game.EffectActor,
   {
      DEFAULT_FADE_LENGTH: 16,
      fadeLength: 0,
      textSize: 12,
      msg: null,
      colour: "rgb(255,255,255)",
      
      /**
       * Text indicator effect rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         // fade out alpha
         var alpha = (1.0 / this.fadeLength) * this.lifespan;
         ctx.save();
         ctx.globalAlpha = alpha;
         Game.fillText(ctx, this.msg, this.textSize + "pt Courier New", this.position.x, this.position.y, this.colour);
         ctx.restore();
      }
   });
})();


/**
 * Score indicator effect actor class.
 * 
 * @namespace Asteroids
 * @class Asteroids.ScoreIndicator
 */
(function()
{
   Asteroids.ScoreIndicator = function(p, v, score, textSize, prefix, colour, fadeLength)
   {
      var msg = score.toString();
      if (prefix)
      {
         msg = prefix + ' ' + msg;
      }
      Asteroids.ScoreIndicator.superclass.constructor.call(this, p, v, msg, textSize, colour, fadeLength);
      return this;
   };
   
   extend(Asteroids.ScoreIndicator, Asteroids.TextIndicator,
   {
   });
})();


/**
 * Power up collectable.
 * 
 * @namespace Asteroids
 * @class Asteroids.PowerUp
 */
(function()
{
   Asteroids.PowerUp = function(p, v)
   {
      Asteroids.PowerUp.superclass.constructor.call(this, p, v);
      return this;
   };
   
   extend(Asteroids.PowerUp, Game.EffectActor,
   {
      RADIUS: 8,
      pulse: 128,
      pulseinc: 8,
      
      /**
       * Power up rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx)
      {
         ctx.save();
         ctx.globalAlpha = 0.75;
         var col = "rgb(255," + this.pulse.toString() + ",0)";
         if (BITMAPS)
         {
            ctx.fillStyle = col;
            ctx.strokeStyle = "rgb(255,255,128)";
         }
         else
         {
            ctx.lineWidth = 2.0;
            ctx.shadowColor = ctx.strokeStyle = col;
         }
         ctx.beginPath();
         ctx.arc(this.position.x, this.position.y, this.RADIUS, 0, TWOPI, true);
         ctx.closePath();
         if (BITMAPS)
         {
            ctx.fill();
         }
         ctx.stroke();
         ctx.restore();
         this.pulse += this.pulseinc;
         if (this.pulse > 255)
         {
            this.pulse = 256 - this.pulseinc;
            this.pulseinc =- this.pulseinc;
         }
         else if (this.pulse < 0)
         {
            this.pulse = 0 - this.pulseinc;
            this.pulseinc =- this.pulseinc;
         }
      },
      
      radius: function radius()
      {
         return this.RADIUS;
      },
      
      collected: function collected(game, player, scene)
      {
         // randomly select a powerup to apply
         var message = null;
         switch (randomInt(0, 9))
         {
            case 0:
            case 1:
               // boost energy
               message = "Energy Boost!";
               player.energy += player.ENERGY_INIT/2;
               if (player.energy > player.ENERGY_INIT)
               {
                  player.energy = player.ENERGY_INIT;
               }
               break;
            
            case 2:
               // fire when shieled
               message = "Fire When Shielded!";
               player.fireWhenShield = true;
               break;
            
            case 3:
               // extra life
               message = "Extra Life!";
               game.lives++;
               break;
            
            case 4:
               // slow down asteroids
               message = "Slow Down Asteroids!";
               for (var n = 0, m = scene.enemies.length, enemy; n < m; n++)
               {
                  enemy = scene.enemies[n];
                  if (enemy instanceof Asteroids.Asteroid)
                  {
                     enemy.vector.scale(0.75);
                  }
               }
               break;
            
            case 5:
               // smart bomb
               message = "Smart Bomb!";
               
               var effectRad = 96;
               
               // add a BIG explosion actor at the smart bomb weapon position and vector
               var boom = new Asteroids.Explosion(
                     this.position.clone(), this.vector.clone().scale(0.5), effectRad / 8);
               scene.effects.push(boom);
               
               // test circle intersection with each enemy actor
               // we check the enemy list length each iteration to catch baby asteroids
               // this is a fully fledged smart bomb after all!
               for (var n = 0, enemy, pos = this.position; n < scene.enemies.length; n++)
               {
                  enemy = scene.enemies[n];
                  
                  // test the distance against the two radius combined
                  if (pos.distance(enemy.position) <= effectRad + enemy.radius())
                  {
                     // intersection detected! 
                     enemy.hit(-1);
                     scene.generatePowerUp(enemy);
                     scene.destroyEnemy(enemy, this.vector, true);
                  }
               }
               break;
            
            case 6:
               // twin cannon primary weapon upgrade
               message = "Twin Cannons!";
               player.primaryWeapons["main"] = new Asteroids.TwinCannonsWeapon(player);
               break;
            
            case 7:
               // v spray cannons
               message = "Spray Cannons!";
               player.primaryWeapons["main"] = new Asteroids.VSprayCannonsWeapon(player);
               break;
            
            case 8:
               // rear guns
               message = "Rear Gun!";
               player.primaryWeapons["rear"] = new Asteroids.RearGunWeapon(player);
               break;
            
            case 9:
               // side guns
               message = "Side Guns!";
               player.primaryWeapons["side"] = new Asteroids.SideGunWeapon(player);
               break;
         }
         
         if (message)
         {
            // generate a effect indicator at the destroyed enemy position
            var vec = new Vector(0, -3.0);
            var effect = new Asteroids.TextIndicator(
                  new Vector(this.position.x, this.position.y - this.RADIUS), vec, message, null, null, 32);
            scene.effects.push(effect);
         }
      }
   });
})();
/**
 * Arena5 HTML canvas game.
 * Scenes for CanvasMark Rendering Benchmark - March 2013
 *  
 * (C) 2013 Kevin Roast kevtoast@yahoo.com @kevinroast
 * 
 * Please see: license.txt
 * You are welcome to use this code, but I would appreciate an email or tweet
 * if you do anything interesting with it!
 */


/**
 * Arena root namespace.
 * 
 * @namespace Arena
 */
if (typeof Arena == "undefined" || !Arena)
{
   var Arena = {};
}


/**
 * Arena prerenderer class.
 * 
 * @namespace Arena
 * @class Arena.Prerenderer
 */
(function()
{
   Arena.Prerenderer = function()
   {
      this.images = [];
      this._renderers = [];
      return this;
   };
   
   Arena.Prerenderer.prototype =
   {
      /**
       * Image list. Keyed by renderer ID - returning an array also. So to get
       * the first image output by prerenderer with id "default": images["default"][0]
       * 
       * @public
       * @property images
       * @type Array
       */
      images: null,
      
      _renderers: null,
      
      /**
       * Add a renderer function to the list of renderers to execute
       * 
       * @param fn {function}    Callback to execute to perform prerender
       *                         Passed canvas element argument - to execute against - the
       *                         callback is responsible for setting appropriate width/height
       *                         of the buffer and should not assume it is cleared.
       *                         Should return Array of images from prerender process
       * @param id {string}      Id of the prerender - used to lookup images later
       */
      addRenderer: function addRenderer(fn, id)
      {
         this._renderers[id] = fn;
      },
      
      /**
       * Execute all prerender functions - call once all renderers have been added
       */
      execute: function execute()
      {
         var buffer = document.createElement('canvas');
         for (var id in this._renderers)
         {
            this.images[id] = this._renderers[id].call(this, buffer);
         }
      }
    };
})();


/**
 * Arena main Benchmark Test class.
 * 
 * @namespace Arena
 * @class Arena.Test
 */
(function()
{
   Arena.Test = function(benchmark)
   {
      // generate the single player actor - available across all scenes
      this.player = new Arena.Player(new Vector(GameHandler.width / 2, GameHandler.height / 2), new Vector(0, 0), 0);
      
      // add benchmark scene
      benchmark.addBenchmarkScene(new Arena.BenchmarkScene(this, benchmark.scenes.length, 0));
      
      // perform prerender steps - create bitmap graphics to use later
      var pr = new Arena.Prerenderer();
      // function to generate a set of point particle images
      var fnPointRenderer = function(buffer, colour)
         {
            var imgs = [];
            for (var size=4; size<=10; size+=2)
            {
               var width = size << 1;
               buffer.width = buffer.height = width;
               var ctx = buffer.getContext('2d');
               var radgrad = ctx.createRadialGradient(size, size, size >> 1, size, size, size);  
               radgrad.addColorStop(0, colour);
               radgrad.addColorStop(1, "#000");
               ctx.fillStyle = radgrad;
               ctx.fillRect(0, 0, width, width);
               var img = new Image();
               img.src = buffer.toDataURL("image/png");
               imgs.push(img);
            }
            return imgs;
         };
      // add the various point particle image prerenderers based on above function
      // default explosion colour
      pr.addRenderer(function(buffer) {
            return fnPointRenderer.call(this, buffer, "rgb(255,125,50)");
         }, "points_rgb(255,125,50)");
      // Tracker: enemy particles
      pr.addRenderer(function(buffer) {
            return fnPointRenderer.call(this, buffer, "rgb(255,96,0)");
         }, "points_rgb(255,96,0)");
      // Borg: enemy particles
      pr.addRenderer(function(buffer) {
            return fnPointRenderer.call(this, buffer, "rgb(0,255,64)");
         }, "points_rgb(0,255,64)");
      // Splitter: enemy particles
      pr.addRenderer(function(buffer) {
            return fnPointRenderer.call(this, buffer, "rgb(148,0,255)");
         }, "points_rgb(148,0,255)");
      // Bomber: enemy particles
      pr.addRenderer(function(buffer) {
            return fnPointRenderer.call(this, buffer, "rgb(255,0,255)");
         }, "points_rgb(255,0,255)");
      // add the smudge explosion particle image prerenderer
      pr.addRenderer(function(buffer) {
            var imgs = [];
            for (var size=8; size<=64; size+=8)
            {
               var width = size << 1;
               buffer.width = buffer.height = width;
               var ctx = buffer.getContext('2d');
               var radgrad = ctx.createRadialGradient(size, size, size >> 3, size, size, size);  
               radgrad.addColorStop(0, "rgb(255,125,50)");
               radgrad.addColorStop(1, "#000");
               ctx.fillStyle = radgrad;
               ctx.fillRect(0, 0, width, width);
               var img = new Image();
               img.src = buffer.toDataURL("image/png");
               imgs.push(img);
            }
            return imgs;
         }, "smudges");
      pr.addRenderer(function(buffer) {
            var imgs = [];
            var size = 40;
            buffer.width = buffer.height = size;
            var ctx = buffer.getContext('2d');
            
            // draw bullet primary weapon
            var rf = function(width, height)
            {
               ctx.beginPath();
               ctx.moveTo(0, height);
               ctx.lineTo(width, 0);
               ctx.lineTo(width, -height);
               ctx.lineTo(0, -height*0.5);
               ctx.lineTo(-width, -height);
               ctx.lineTo(-width, 0);
               ctx.closePath();
               ctx.fill();
            };
            ctx.shadowBlur = 8;
            ctx.globalCompositeOperation = "lighter";
            ctx.translate(size/2, size/2);
            ctx.shadowColor = "rgb(255,255,255)";
            ctx.fillStyle = "rgb(255,220,75)";
            rf.call(this, 10, 15)
            ctx.shadowColor = "rgb(255,100,100)";
            ctx.fillStyle = "rgb(255,50,50)";
            rf.call(this, 10 * 0.75, 15 * 0.75);
            
            var img = new Image();
            img.src = buffer.toDataURL("image/png");
            imgs.push(img);
            return imgs;
         }, "playerweapon");
      pr.execute();
      GameHandler.prerenderer = pr;
   };
   
   Arena.Test.prototype =
   {
      /**
       * Reference to the single game player actor
       */
      player: null,
      
      /**
       * Lives count
       */
      lives: 1,
      
      /**
       * Current game score 
       */
      score: 0,
      
      /**
       * High score
       */
      highscore: 0,
      
      /**
       * Last score
       */
      lastscore: 0,
      
      /**
       * Current multipler
       */
      scoreMultiplier: 1
   };
})();


/**
 * Arena Game scene class.
 * 
 * @namespace Arena
 * @class Arena.BenchmarkScene
 */
(function()
{
   Arena.BenchmarkScene = function(game, test, feature)
   {
      this.game = game;
      this.test = test;
      this.feature = feature;
      this.player = game.player;
      
      var msg = "Test " + test + " - Arena5 - Vectors, shadows, bitmaps, text";
      var interval = new Game.Interval(msg, this.intervalRenderer);
      Arena.BenchmarkScene.superclass.constructor.call(this, true, interval);
   };
   
   extend(Arena.BenchmarkScene, Game.Scene,
   {
      test: 0,
      game: null,
      
      /**
       * Local reference to the game player actor
       */
      player: null,
      
      /**
       * Top-level list of game actors sub-lists
       */
      actors: null,
      
      /**
       * List of player fired bullet actors
       */
      playerBullets: null,
      
      /**
       * List of enemy actors (asteroids, ships etc.)
       */
      enemies: null,
      
      /**
       * List of enemy fired bullet actors
       */
      enemyBullets: null,
      
      /**
       * List of effect actors
       */
      effects: null,
      
      /**
       * List of collectables actors
       */
      collectables: null,
      
      /**
       * Displayed score (animates towards actual score)
       */
      scoredisplay: 0,
      
      /**
       * Scene init event handler
       */
      onInitScene: function onInitScene()
      {
         // generate the actors and add the actor sub-lists to the main actor list
         this.actors = [];
         this.actors.push(this.enemies = []);
         this.actors.push(this.playerBullets = []);
         this.actors.push(this.enemyBullets = []);
         this.actors.push(this.effects = []);
         this.actors.push(this.collectables = []);
         
         // start view centered in the game world
         this.world.viewx = this.world.viewy = (this.world.size / 2) - (this.world.viewsize / 2);
         
         this.testScore = 10;
         
         // reset player
         this.resetPlayerActor();
      },
      
      /**
       * Restore the player to the game - reseting position etc.
       */
      resetPlayerActor: function resetPlayerActor(persistPowerUps)
      {
         this.actors.push([this.player]);
         
         // reset the player position - centre of world
         with (this.player)
         {
            position.x = position.y = this.world.size / 2;
            vector.x = vector.y = heading = 0;
            reset(persistPowerUps);
         }
      },
      
      /**
       * Scene before rendering event handler
       */
      onBeforeRenderScene: function onBeforeRenderScene(benchmark)
      {
         var p = this.player,
             w = this.world;
         
         // update view position based on new player position
         var viewedge = w.viewsize * 0.2;
         if (p.position.x > viewedge && p.position.x < w.size - viewedge)
         {
            w.viewx = p.position.x - w.viewsize * 0.5;
         }
         if (p.position.y > viewedge && p.position.y < w.size - viewedge)
         {
            w.viewy = p.position.y - w.viewsize * 0.5;
         }
         
         if (benchmark)
         {
            if (Date.now() - this.sceneStartTime > this.testState)
            {
               this.testState += 500;
               for (var i=0; i<2; i++)
               {
                  this.enemies.push(new Arena.EnemyShip(this, (this.enemies.length % 6) + 1));
               }
               this.enemies[0].damageBy(100);
               this.destroyEnemy(this.enemies[0], new Vector(0,1), this.player)
            }
         }
         
         // update all actors using their current vector in the game world
         this.updateActors();
      },
      
      /**
       * Scene rendering event handler
       */
      onRenderScene: function onRenderScene(ctx)
      {
         ctx.clearRect(0, 0, GameHandler.width, GameHandler.height);
         
         // glowing vector effect shadow
         ctx.shadowBlur = (DEBUG && DEBUG.DISABLEGLOWEFFECT) ? 0 : 8;
         
         // render background effect - wire grid
         this.renderBackground(ctx);
         
         // render the game actors
         this.renderActors(ctx);
         
         // render info overlay graphics
         this.renderOverlay(ctx);
         
         // detect bullet collisions
         this.collisionDetectBullets();
      },
      
      /**
       * Render background effects for the scene
       */
      renderBackground: function renderBackground(ctx)
      {
         // render background effect - wire grid
         // manually transform world to screen for this effect and therefore
         // assume there is a horizonal and vertical "wire" every N units
         ctx.save();
         ctx.strokeStyle = "rgb(0,30,60)";
         ctx.lineWidth = 1.0;
         ctx.shadowBlur = 0;
         ctx.beginPath();
         
         var UNIT = 100;
         var w = this.world;
             xoff = UNIT - w.viewx % UNIT,
             yoff = UNIT - w.viewy % UNIT,
             // calc top left edge of world (prescaled)
             x1 = (w.viewx >= 0 ? 0 : -w.viewx) * w.scale,
             y1 = (w.viewy >= 0 ? 0 : -w.viewy) * w.scale,
             // calc bottom right edge of world (prescaled)
             x2 = (w.viewx < w.size - w.viewsize ? w.viewsize : w.size - w.viewx) * w.scale,
             y2 = (w.viewy < w.size - w.viewsize ? w.viewsize : w.size - w.viewy) * w.scale;
         
         // plot the grid wires that make up the background
         for (var i=0, j=w.viewsize/UNIT; i<j; i++)
         {
            // check we are in bounds of the visible world before drawing grid line segments
            if (xoff + w.viewx > 0 && xoff + w.viewx < w.size)
            {
               ctx.moveTo(xoff * w.scale, y1);
               ctx.lineTo(xoff * w.scale, y2);
            }
            if (yoff + w.viewy > 0 && yoff + w.viewy < w.size)
            {
               ctx.moveTo(x1, yoff * w.scale);
               ctx.lineTo(x2, yoff * w.scale);
            }
            xoff += UNIT;
            yoff += UNIT;
         }
         
         ctx.closePath();
         ctx.stroke();
         
         // render world edges
         ctx.strokeStyle = "rgb(60,128,90)";
         ctx.lineWidth = 1;
         ctx.beginPath();
         
         if (w.viewx <= 0)
         {
            xoff = -w.viewx;
            ctx.moveTo(xoff * w.scale, y1);
            ctx.lineTo(xoff * w.scale, y2);
         }
         else if (w.viewx >= w.size - w.viewsize)
         {
            xoff = w.size - w.viewx;
            ctx.moveTo(xoff * w.scale, y1);
            ctx.lineTo(xoff * w.scale, y2);
         }
         if (w.viewy <= 0)
         {
            yoff = -w.viewy;
            ctx.moveTo(x1, yoff * w.scale);
            ctx.lineTo(x2, yoff * w.scale);
         }
         else if (w.viewy >= w.size - w.viewsize)
         {
            yoff = w.size - w.viewy;
            ctx.moveTo(x1, yoff * w.scale);
            ctx.lineTo(x2, yoff * w.scale);
         }
         
         ctx.closePath();
         ctx.stroke();
         ctx.restore();
      },
      
      /**
       * Update the scene actors based on current vectors and expiration.
       */
      updateActors: function updateActors()
      {
         for (var i = 0, j = this.actors.length; i < j; i++)
         {
            var actorList = this.actors[i];
            
            for (var n = 0; n < actorList.length; n++)
            {
               var actor = actorList[n];
               
               // call onUpdate() event for each actor
               actor.onUpdate(this);
               
               // expiration test first
               if (actor.expired())
               {
                  actorList.splice(n, 1);
               }
               else
               {
                  // update actor using its current vector
                  actor.position.add(actor.vector);
                  
                  // TODO: different behavior for traversing out of the world space?
                  //       add behavior flag to Actor i.e. bounce, invert, disipate etc.
                  //       - could add method to actor itself - so would handle internally...
                  if (actor === this.player)
                  {
                     if (actor.position.x >= this.world.size ||
                         actor.position.x < 0 ||
                         actor.position.y >= this.world.size ||
                         actor.position.y < 0)
                     {
                        actor.vector.invert();
                        actor.vector.scale(0.75);
                        actor.position.add(actor.vector);
                     }
                  }
                  else
                  {
                     var bounceX = false,
                         bounceY = false;
                     if (actor.position.x >= this.world.size)
                     {
                        actor.position.x = this.world.size;
                        bounceX = true;
                     }
                     else if (actor.position.x < 0)
                     {
                        actor.position.x = 0;
                        bounceX = true;
                     }
                     if (actor.position.y >= this.world.size)
                     {
                        actor.position.y = this.world.size;
                        bounceY = true;
                     }
                     else if (actor.position.y < 0)
                     {
                        actor.position.y = 0;
                        bounceY = true
                     }
                     // bullets don't bounce - create an effect at the arena boundry instead
                     if ((bounceX || bounceY) &&
                         ((actor instanceof Arena.Bullet && !this.player.bounceWeapons) ||
                          actor instanceof Arena.EnemyBullet))
                     {
                        // replace bullet with a particle effect at the same position and vector
                        var vec = actor.vector.nscale(0.5);
                        this.effects.push(new Arena.BulletImpactEffect(actor.position.clone(), vec));
                        // remove bullet actor from play
                        actorList.splice(n, 1);
                     }
                     else
                     {
                        if (bounceX)
                        {
                           var h = actor.vector.thetaTo2(new Vector(0, 1));
                           actor.vector.rotate(h*2);
                           actor.vector.scale(0.9);
                           actor.position.add(actor.vector);
                           // TODO: add "interface" for actor with heading?
                           //       or is hasProperty() more "javascript"
                           if (actor.hasOwnProperty("heading")) actor.heading += (h*2)/RAD;
                        }
                        if (bounceY)
                        {
                           var h = actor.vector.thetaTo2(new Vector(1, 0));
                           actor.vector.rotate(h*2);
                           actor.vector.scale(0.9);
                           actor.position.add(actor.vector);
                           if (actor.hasOwnProperty("heading")) actor.heading += (h*2)/RAD;
                        }
                     }
                  }
               }
            }
         }
      },
      
      /**
       * Detect bullet collisions with enemy actors.
       */
      collisionDetectBullets: function collisionDetectBullets()
      {
         var bullet, bulletRadius, bulletPos;
         
         // collision detect player bullets with enemies
         // NOTE: test length each loop as list length can change
         for (var i = 0; i < this.playerBullets.length; i++)
         {
            bullet = this.playerBullets[i];
            bulletRadius = bullet.radius;
            bulletPos = bullet.position;
            
            // test circle intersection with each enemy actor
            for (var n = 0, m = this.enemies.length, enemy, z; n < m; n++)
            {
               enemy = this.enemies[n];
               
               // test the distance against the two radius combined
               if (bulletPos.distance(enemy.position) <= bulletRadius + enemy.radius)
               {
                  // test for area effect bomb weapon
                  var effectRad = bullet.effectRadius();
                  //if (effectRad === 0)
                  {
                     // impact the enemy with the bullet - may destroy it or just damage it
                     if (enemy.damageBy(bullet.power()))
                     {
                        // destroy the enemy under the bullet
                        this.destroyEnemy(enemy, bullet.vector, true);
                        this.generateMultiplier(enemy);
                        this.generatePowerUp(enemy);
                     }
                     else
                     {
                        // add bullet impact effect to show the bullet hit
                        var effect = new Arena.EnemyImpact(
                           bullet.position.clone(),
                           bullet.vector.nscale(0.5 + Rnd() * 0.5), enemy);
                        this.effects.push(effect);
                     }
                  }
                  
                  // remove this bullet from the actor list as it has been destroyed
                  this.playerBullets.splice(i, 1);
                  break;
               }
            }
         }
      },
      
      /**
       * Destroy an enemy. Replace with appropriate effect.
       * Also applies the score for the destroyed item if the player caused it.
       * 
       * @param enemy {Game.EnemyActor} The enemy to destory and add score for
       * @param parentVector {Vector} The vector of the item that hit the enemy
       * @param player {boolean} If true, the player was the destroyer
       */
      destroyEnemy: function destroyEnemy(enemy, parentVector, player)
      {
         // add an explosion actor at the enemy position and vector
         var vec = enemy.vector.clone();
         // add scaled parent vector - to give some momentum from the impact
         vec.add(parentVector.nscale(0.2));
         this.effects.push(new Arena.EnemyExplosion(enemy.position.clone(), vec, enemy));
         
         if (player)
         {
            // increment score
            var inc = (enemy.scoretype + 1) * 5 * this.game.scoreMultiplier;
            this.game.score += inc;
            
            // generate a score effect indicator at the destroyed enemy position
            var vec = new Vector(0, -5.0).add(enemy.vector.nscale(0.5));
            this.effects.push(new Arena.ScoreIndicator(
                  new Vector(enemy.position.x, enemy.position.y - 16), vec, inc));
            
            // call event handler for enemy
            enemy.onDestroyed(this, player);
         }
      },
      
      /**
       * Generate score multiplier(s) for player to collect after enemy is destroyed
       */
      generateMultiplier: function generateMultiplier(enemy)
      {
         if (enemy.dropsMutliplier)
         {
            var count = randomInt(1, (enemy.type < 5 ? enemy.type : 4));
            for (var i=0; i<count; i++)
            {
               this.collectables.push(new Arena.Multiplier(enemy.position.clone(),
                  enemy.vector.nscale(0.2).rotate(Rnd() * TWOPI)));
            }
         }
      },
      
      /**
       * Generate powerup for player to collect after enemy is destroyed
       */
      generatePowerUp: function generatePowerUp(enemy)
      {
         if (this.player.energy !== this.player.ENERGY_INIT && Rnd() < 0.1)
         {
            this.collectables.push(new Arena.EnergyBoostPowerup(enemy.position.clone(),
               enemy.vector.nscale(0.5).rotate(Rnd() * TWOPI)));
         }
      },
      
      /**
       * Render each actor to the canvas.
       * 
       * @param ctx {object} Canvas rendering context
       */
      renderActors: function renderActors(ctx)
      {
         for (var i = 0, j = this.actors.length; i < j; i++)
         {
            // walk each sub-list and call render on each object
            var actorList = this.actors[i];
            
            for (var n = actorList.length - 1; n >= 0; n--)
            {
               actorList[n].onRender(ctx, this.world);
            }
         }
      },
      
      /**
       * Render player information HUD overlay graphics.
       * 
       * @param ctx {object} Canvas rendering context
       */
      renderOverlay: function renderOverlay(ctx)
      {
         var w = this.world,
             width = GameHandler.width,
             height = GameHandler.height;
         
         ctx.save();
         ctx.shadowBlur = 0;
         
         // energy bar (scaled down from player energy max)
         var ewidth = ~~(100 * w.scale * 2),
             eheight = ~~(4 * w.scale * 2);
         ctx.strokeStyle = "rgb(128,128,50)";
         ctx.strokeRect(4, 4, ewidth+1, 4 + eheight);
         ctx.fillStyle = "rgb(255,255,150)";
         ctx.fillRect(5, 5, (this.player.energy / (this.player.ENERGY_INIT / ewidth)), 3 + eheight);
         
         // score display - update towards the score in increments to animate it
         var font12pt = Game.fontFamily(w, 12),
             font12size = Game.fontSize(w, 12);
         var score = this.game.score,
             inc = (score - this.scoredisplay) * 0.1;
         this.scoredisplay += inc;
         if (this.scoredisplay > score)
         {
            this.scoredisplay = score;
         }
         var sscore = Ceil(this.scoredisplay).toString();
         // pad with zeros
         for (var i=0, j=8-sscore.length; i<j; i++)
         {
            sscore = "0" + sscore;
         }
         Game.fillText(ctx, sscore, font12pt, width * 0.2 + width * 0.1, font12size + 2, "white");
         
         // high score
         // TODO: add method for incrementing score so this is not done here
         if (score > this.game.highscore)
         {
            this.game.highscore = score;
         }
         sscore = this.game.highscore.toString();
         // pad with zeros
         for (var i=0, j=8-sscore.length; i<j; i++)
         {
            sscore = "0" + sscore;
         }
         Game.fillText(ctx, "HI: " + sscore, font12pt, width * 0.4 + width * 0.1, font12size + 2, "white");
         
         // score multiplier indicator
         Game.fillText(ctx, "x" + this.game.scoreMultiplier, font12pt, width * 0.7 + width * 0.1, font12size + 2, "white");
         
         // Benchmark - information output
         if (this.sceneCompletedTime)
         {
            Game.fillText(ctx, "TEST " + this.test + " COMPLETED: " + this.getTransientTestScore(), "20pt Courier New", 4, 40, "white");
         }
         Game.fillText(ctx, "SCORE: " + this.getTransientTestScore(), "12pt Courier New", 0, GameHandler.height - 42, "lightblue");
         Game.fillText(ctx, "TSF: " + Math.round(GameHandler.frametime) + "ms", "12pt Courier New", 0, GameHandler.height - 22, "lightblue");
         Game.fillText(ctx, "FPS: " + GameHandler.lastfps, "12pt Courier New", 0, GameHandler.height - 2, "lightblue");
         
         ctx.restore();
      },
      
      screenCenterVector: function screenCenterVector()
      {
         // transform to world position - to get the center of the game screen
         var m = new Vector(GameHandler.width*0.5, GameHandler.height*0.5);
         m.scale(1 / this.world.scale);
         m.x += this.world.viewx;
         m.y += this.world.viewy;
         return m;
      }
   });
})();
/**
 * Arena.K3DController class.
 * 
 * Arena impl of a K3D controller. One per sprite.
 */
(function()
{
   /**
    * Arena.Controller constructor
    * 
    * @param canvas {Object}  The canvas to render the object list into.
    */
   Arena.Controller = function()
   {
      Arena.Controller.superclass.constructor.call(this);
   };
   
   extend(Arena.Controller, K3D.BaseController,
   {
      /**
       * Render tick - should be called from appropriate sprite renderer
       */
      render: function(ctx)
      {
         // execute super class method to process render pipelines
         this.processFrame(ctx);
      }
   });
})();


/**
 * K3DActor class.
 * 
 * An actor that can be rendered by K3D. The code implements a K3D controller.
 * Call renderK3D() each frame.
 * 
 * @namespace Arena
 * @class Arena.K3DActor
 */
(function()
{
   Arena.K3DActor = function(p, v)
   {
      Arena.K3DActor.superclass.constructor.call(this, p, v);
      this.k3dController = new Arena.Controller();
      return this;
   };
   
   extend(Arena.K3DActor, Game.Actor,
   {
      k3dController: null,
      k3dObject: null,
      
      /**
       * Render K3D graphic.
       */
      renderK3D: function renderK3D(ctx)
      {
         this.k3dController.render(ctx);
      },
      
      setK3DObject: function setK3DObject(obj)
      {
         this.k3dObject = obj;
         this.k3dController.addK3DObject(obj);
      }
   });
})();/**
 * Player actor class.
 *
 * @namespace Arena
 * @class Arena.Player
 */
(function()
{
   Arena.Player = function(p, v, h)
   {
      Arena.Player.superclass.constructor.call(this, p, v);
      
      this.energy = this.ENERGY_INIT;
      this.radius = 20;
      this.heading = h;
      
      // setup weapons
      this.primaryWeapons = [];
      this.primaryWeapons["main"] = new Arena.PrimaryWeapon(this);
      
      // 3D sprite object - must be created after constructor call
      var obj = new K3D.K3DObject();
      with (obj)
      {
         drawmode = "wireframe";
         shademode = "depthcue";
         depthscale = 32;
         linescale = 3;
         perslevel = 256;
         
         addphi = -1.0; //addphi = 1.0; addtheta = -1.0; addgamma = -0.75;
         scale = 0.8;    // TODO: pre-scale points? (this is only done once for player, but enemies...)
         init(
            [{x:-30,y:-20,z:0}, {x:-15,y:-25,z:20}, {x:15,y:-25,z:20}, {x:30,y:-20,z:0}, {x:15,y:-25,z:-20}, {x:-15,y:-25,z:-20}, {x:0,y:35,z:0}],
            [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:4}, {a:4,b:5}, {a:5,b:0}, {a:1,b:6}, {a:2,b:6}, {a:4,b:6}, {a:5,b:6}, {a:0,b:6}, {a:3,b:6}],
            [{vertices:[0,1,6]}, {vertices:[1,2,6]}, {vertices:[2,3,6]}, {vertices:[3,4,6]}, {vertices:[4,5,6]}, {vertices:[5,0,6]}, {vertices:[0,1,2,3,4,5]}]
         );
      }
      this.setK3DObject(obj);
      
      return this;
   };
   
   extend(Arena.Player, Arena.K3DActor,
   {
      MAX_PLAYER_VELOCITY: 15.0,
      THRUST_DELAY: 1,
      ENERGY_INIT: 100,
      
      /**
       * Player heading
       */
      heading: 0,
      
      /**
       * Player energy level
       */
      energy: 0,
      
      /**
       * Primary weapon list
       */
      primaryWeapons: null,
      
      /**
       * Engine thrust recharge counter
       */
      thrustRecharge: 0,
      
      /**
       * True if the engine thrust graphics should be rendered next frame
       */
      engineThrust: false,
      
      /**
       * Frame that the player was killed on - to cause a delay before respawning the player
       */
      killedOnFrame: 0,
      
      /**
       * Power up settings - primary weapon bounce
       */
      bounceWeapons: false,
      
      /**
       * Player rendering method
       * 
       * @param ctx {object} Canvas rendering context
       * @param world {object} World metadata
       */
      onRender: function onRender(ctx, world)
      {
         var headingRad = this.heading * RAD;
         
         // transform world to screen - non-visible returns null
         var viewposition = Game.worldToScreen(this.position, world, this.radius);
         if (viewposition)
         {
            // render engine thrust?
            if (this.engineThrust)
            {
               ctx.save();
               // scale ALL graphics... - translate to position apply canvas scaling
               ctx.translate(viewposition.x, viewposition.y);
               ctx.scale(world.scale, world.scale);
               ctx.rotate(headingRad);
               ctx.translate(0, -4);   // slight offset so that collision radius is centered
               ctx.globalAlpha = 0.4 + Rnd() * 0.5;
               ctx.shadowColor = ctx.fillStyle = "rgb(25,125,255)";
               ctx.beginPath();
               ctx.moveTo(-12, 20);
               ctx.lineTo(12, 20);
               ctx.lineTo(0, 50 + Rnd() * 20);
               ctx.closePath();
               ctx.fill();
               ctx.restore();
               this.engineThrust = false;
            }
            
            // render player graphic
            ctx.save();
            ctx.shadowColor = "rgb(255,255,255)";
            ctx.translate(viewposition.x, viewposition.y);
            ctx.scale(world.scale, world.scale);
            ctx.rotate(headingRad);
            ctx.translate(0, -4);   // slight offset so that collision radius is centered
            
            // render 3D sprite
            this.renderK3D(ctx);
            
            ctx.restore();
         }
         //if (DEBUG && !viewposition) console.log("non-visible: " + new Date().getTime());
      },
      
      /**
       * Handle key input to rotate and move the player
       */
      handleInput: function handleInput(input)
      {
         var h = this.heading % 360;
         
         // TODO: hack, fix this to maintain +ve heading or change calculation below...
         if (h < 0) h += 360;
         
         // first section tweens the current rendered heading of the player towards
         // the desired heading - but the next section actually applies a vector
         // TODO: this seems over complicated - must be an easier way to do this...
         if (input.left)
         {
            if (h > 270 || h < 90)
            {
               if (h > 270) this.heading -= ((h - 270) * 0.2);
               else this.heading -= ((h + 90) * 0.2);
            }
            else this.heading += ((270 - h) * 0.2);
         }
         if (input.right)
         {
            if (h < 90 || h > 270)
            {
               if (h < 90) this.heading += ((90 - h) * 0.2);
               else this.heading += ((h - 90) * 0.2);
            }
            else this.heading -= ((h - 90) * 0.2);
         }
         if (input.up)
         {
            if (h < 180)
            {
               this.heading -= (h * 0.2);
            }
            else this.heading += ((360 - h) * 0.2);
         }
         if (input.down)
         {
            if (h < 180)
            {
               this.heading += ((180 - h) * 0.2);
            }
            else this.heading -= ((h - 180) * 0.2);
         }
         
         // second section applies the direct thrust angled vector
         // this ensures a snappy control method with the above heading effect
         var angle = null;
         if (input.left)
         {
            if (input.up) angle = 315;
            else if (input.down) angle = 225;
            else angle = 270;
         }
         else if (input.right)
         {
            if (input.up) angle = 45;
            else if (input.down) angle = 135;
            else angle = 90;
         }
         else if (input.up)
         {
            if (input.left) angle = 315;
            else if (input.right) angle = 45;
            else angle = 0;
         }
         else if (input.down)
         {
            if (input.left) angle = 225;
            else if (input.right) angle = 135;
            else angle = 180;
         }
         if (angle !== null)
         {
            this.thrust(angle);
         }
         else
         {
            // reduce thrust over time if player isn't actively moving
            this.vector.scale(0.9);
         }
      },
      
      /**
       * Execute player forward thrust request
       * Automatically a delay is used between each application - to ensure stable thrust on all machines.
       */
      thrust: function thrust(angle)
      {
         // now test we did not thrust too recently - to stop fast key repeat issues
         if (GameHandler.frameCount - this.thrustRecharge > this.THRUST_DELAY)
         {
            // update last frame count
            this.thrustRecharge = GameHandler.frameCount;
            
            // generate a small thrust vector
            var t = new Vector(0.0, -2.00);
            
            // rotate thrust vector by player current heading
            t.rotate(angle * RAD);
            
            // add player thrust vector to position
            this.vector.add(t);
            
            // player can't exceed maximum velocity - scale vector down if
            // this occurs - do this rather than not adding the thrust at all
            // otherwise the player cannot turn and thrust at max velocity
            if (this.vector.length() > this.MAX_PLAYER_VELOCITY)
            {
               this.vector.scale(this.MAX_PLAYER_VELOCITY / this.vector.length());
            }
         }
         // mark so that we know to render engine thrust graphics
         this.engineThrust = true;
      },
      
      damageBy: function damageBy(enemy)
      {
         this.energy -= enemy.playerDamage;
         if (this.energy <= 0)
         {
            this.energy = 0;
            this.kill();
         }
      },
      
      kill: function kill()
      {
         this.alive = false;
         this.killedOnFrame = GameHandler.frameCount;
      },
      
      /**
       * Fire primary weapon(s)
       * @param bulletList {Array} to add bullet(s) to on success
       * @param heading {Number} bullet heading
       */
      firePrimary: function firePrimary(bulletList, vector, heading)
      {
         // attempt to fire the primary weapon(s)
         // first ensure player is alive
         if (this.alive)
         {
            for (var w in this.primaryWeapons)
            {
               var b = this.primaryWeapons[w].fire(vector, heading);
               if (b)
               {
                  for (var i=0; i<b.length; i++)
                  {
                     bulletList.push(b[i]);
                  }
               }
            }
         }
      },
      
      reset: function reset(persistPowerUps)
      {
         // reset energy, alive status, weapons and power up flags
         this.alive = true;
         if (!persistPowerUps)
         {
            // reset weapons
            this.primaryWeapons = [];
            this.primaryWeapons["main"] = new Arena.PrimaryWeapon(this);
            
            // reset powerup settings
            this.bounceWeapons = false;
         }
         this.energy = this.ENERGY_INIT;
      }
   });
})();


/**
 * Player score multiplier collectable class.
 *
 * @namespace Arena
 * @class Arena.Multiplier
 */
(function()
{
   Arena.Multiplier = function(p, v, h)
   {
      Arena.Multiplier.superclass.constructor.call(this, p, v, this.LIFESPAN);
      this.radius = 10;
      this.rotation = 0;
      return this;
   };
   
   extend(Arena.Multiplier, Game.EffectActor,
   {
      LIFESPAN: 250,
      FADE_LENGTH: 16,
      rotation: 0,
      
      /**
       * Multiplier collectable rendering method
       * 
       * @param ctx {object} Canvas rendering context
       * @param world {object} World metadata
       */
      onRender: function onRender(ctx, world)
      {
         // transform world to screen - non-visible returns null
         var viewposition = Game.worldToScreen(this.position, world, this.radius);
         if (viewposition)
         {
            var r = this.radius * 0.6;
            ctx.save();
            ctx.globalCompositeOperation = "lighter";
            if (this.lifespan < this.FADE_LENGTH)
            {
               ctx.globalAlpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
            }
            ctx.strokeStyle = ctx.shadowColor = "rgb(255,180,0)";
            ctx.translate(viewposition.x, viewposition.y);
            ctx.scale(world.scale, world.scale);
            ctx.rotate(this.rotation);
            ctx.strokeRect(-r, -r, this.radius*1.2, this.radius*1.2);
            ctx.restore();
            this.rotation += 0.02;
         }
      },
      
      collected: function collected(game, player, scene)
      {
         if (++game.scoreMultiplier % 10 === 0)
         {
            // display multiplier to player every large increment
            var vec = new Vector(0, -5.0).add(this.vector);
            scene.effects.push(new Arena.TextIndicator(
               this.position.clone(), vec, "x" + game.scoreMultiplier, 32, "white", 32));
         }
      }
   });
})();


/**
 * Player energy boost powerup collectable class.
 *
 * @namespace Arena
 * @class Arena.EnergyBoostPowerup
 */
(function()
{
   Arena.EnergyBoostPowerup = function(p, v, h)
   {
      Arena.EnergyBoostPowerup.superclass.constructor.call(this, p, v, this.LIFESPAN);
      this.radius = 12;
      this.rotation = 0;
      return this;
   };
   
   extend(Arena.EnergyBoostPowerup, Game.EffectActor,
   {
      LIFESPAN: 350,
      FADE_LENGTH: 16,
      rotation: 0,
      
      /**
       * EnergyBoostPowerup collectable rendering method
       * 
       * @param ctx {object} Canvas rendering context
       * @param world {object} World metadata
       */
      onRender: function onRender(ctx, world)
      {
         // transform world to screen - non-visible returns null
         var viewposition = Game.worldToScreen(this.position, world, this.radius);
         if (viewposition)
         {
            var r = this.radius * 0.6;
            ctx.save();
            ctx.globalCompositeOperation = "lighter";
            if (this.lifespan < this.FADE_LENGTH)
            {
               ctx.globalAlpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
            }
            ctx.lineWidth = 2.0;
            ctx.strokeStyle = ctx.shadowColor = "rgb(100,255,0)";
            ctx.translate(viewposition.x, viewposition.y);
            ctx.scale(world.scale, world.scale);
            ctx.rotate(this.rotation);
            ctx.strokeRect(-r, -r, this.radius*1.2, this.radius*1.2);
            ctx.restore();
            this.rotation += 0.05;
         }
      },
      
      collected: function collected(game, player, scene)
      {
         // increment player energy
         player.energy += 25;
         if (player.energy > player.ENERGY_INIT)
         {
            player.energy = player.ENERGY_INIT;
         }
         
         // display indicator
         var vec = new Vector(0, -5.0).add(this.vector);
         scene.effects.push(new Arena.TextIndicator(
            this.position.clone(), vec, "Energy Boost!", 32, "white", 32));
      }
   });
})();
/**
 * Weapon system base class for the player actor.
 * 
 * @namespace Arena
 * @class Arena.Weapon
 */
(function()
{
   Arena.Weapon = function(player)
   {
      this.player = player;
      return this;
   };
   
   Arena.Weapon.prototype =
   {
      rechargeTime: 3,
      weaponRecharged: 0,
      player: null,
      
      fire: function(v, h)
      {
         // now test we did not fire too recently
         if (GameHandler.frameCount - this.weaponRecharged > this.rechargeTime)
         {
            // ok, update last fired frame and we can now generate a bullet
            this.weaponRecharged = GameHandler.frameCount;
            
            return this.doFire(v, h);
         }
      },
      
      doFire: function(v, h)
      {
      }
   };
})();


/**
 * Basic primary weapon for the player actor.
 * 
 * @namespace Arena
 * @class Arena.PrimaryWeapon
 */
(function()
{
   Arena.PrimaryWeapon = function(player)
   {
      Arena.PrimaryWeapon.superclass.constructor.call(this, player);
      this.rechargeTime = this.DEFAULT_RECHARGE;
      return this;
   };
   
   extend(Arena.PrimaryWeapon, Arena.Weapon,
   {
      DEFAULT_RECHARGE: 5,
      bulletCount: 1,   // increase this to output more intense bullet stream
      
      doFire: function(vector, heading)
      {
         var bullets = [],
             count = this.bulletCount,
             total = (count > 2 ? randomInt(count - 1, count) : count);
         for (var i=0; i<total; i++)
         {
            // slightly randomize the spread based on bullet count
            var offset = (count > 1 ? Rnd() * PIO16 * (count-1) : 0),
                h = heading + offset - (PIO32 * (count-1)),
                v = vector.nrotate(offset - (PIO32 * (count-1))).scale(1 + Rnd() * 0.1 - 0.05);
            v.add(this.player.vector);
            
            bullets.push(new Arena.Bullet(this.player.position.clone(), v, h));
         }
         return bullets;
      }
   });
})();


/**
 * Player Bullet actor class.
 *
 * @namespace Arena
 * @class Arena.Bullet
 */
(function()
{
   Arena.Bullet = function(p, v, h, lifespan)
   {
      Arena.Bullet.superclass.constructor.call(this, p, v);
      this.heading = h;
      this.lifespan = (lifespan ? lifespan : this.BULLET_LIFESPAN);
      this.radius = this.BULLET_RADIUS;
      return this;
   };
   
   extend(Arena.Bullet, Game.Actor,
   {
      BULLET_RADIUS: 12,
      BULLET_LIFESPAN: 30,
      FADE_LENGTH: 5,
      
      /**
       * Bullet heading
       */
      heading: 0,
      
      /**
       * Bullet lifespan remaining
       */
      lifespan: 0,
      
      /**
       * Bullet power energy
       */
      powerLevel: 1,
      
      /**
       * Bullet rendering method
       * 
       * @param ctx {object} Canvas rendering context
       * @param world {object} World metadata
       */
      onRender: function onRender(ctx, world)
      {
         ctx.save();
         ctx.shadowBlur = 0;
         ctx.globalCompositeOperation = "lighter";
         if (this.worldToScreen(ctx, world, this.BULLET_RADIUS) &&
             this.lifespan < this.BULLET_LIFESPAN - 1)   // hack - to stop draw over player ship
         {
            if (this.lifespan < this.FADE_LENGTH)
            {
               ctx.globalAlpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
            }
            
            // rotate into the correct heading
            ctx.rotate(this.heading * RAD);
            
            // draw bullet primary weapon
            try
            {
               ctx.drawImage(GameHandler.prerenderer.images["playerweapon"][0], -20, -20);
            }
            catch (error)
            {
               if (console !== undefined) console.log(error.message);
            }
         }
         ctx.restore();
      },
      
      /**
       * Actor expiration test
       * 
       * @return true if expired and to be removed from the actor list, false if still in play
       */
      expired: function expired()
      {
         // deduct lifespan from the bullet
         return (--this.lifespan === 0);
      },
      
      /**
       * Area effect weapon radius - zero for primary bullets
       */
      effectRadius: function effectRadius()
      {
         return 0;
      },
      
      power: function power()
      {
         return this.powerLevel;
      }
   });
})();


/**
 * Enemy Bullet actor class.
 *
 * @namespace Arena
 * @class Arena.EnemyBullet
 */
(function()
{
   Arena.EnemyBullet = function(p, v, power)
   {
      Arena.EnemyBullet.superclass.constructor.call(this, p, v);
      this.powerLevel = this.playerDamage = power;
      this.lifespan = this.BULLET_LIFESPAN;
      this.radius = this.BULLET_RADIUS;
      return this;
   };
   
   extend(Arena.EnemyBullet, Game.Actor,
   {
      BULLET_LIFESPAN: 75,
      BULLET_RADIUS: 10,
      FADE_LENGTH: 8,
      powerLevel: 0,
      playerDamage: 0,
      
      /**
       * Bullet lifespan remaining
       */
      lifespan: 0,
      
      /**
       * Bullet rendering method
       * 
       * @param ctx {object} Canvas rendering context
       * @param world {object} World metadata
       */
      onRender: function onRender(ctx, world)
      {
         ctx.save();
         ctx.globalCompositeOperation = "lighter";
         if (this.worldToScreen(ctx, world, this.BULLET_RADIUS) &&
             this.lifespan < this.BULLET_LIFESPAN - 1)   // hack - to stop draw over enemy
         {
            if (this.lifespan < this.FADE_LENGTH)
            {
               ctx.globalAlpha = (1.0 / this.FADE_LENGTH) * this.lifespan;
            }
            ctx.shadowColor = ctx.fillStyle = "rgb(150,255,150)";
            
            var rad = this.BULLET_RADIUS - 2;
            ctx.beginPath();
            ctx.arc(0, 0, (rad-1 > 0 ? rad-1 : 0.1), 0, TWOPI, true);
            ctx.closePath();
            ctx.fill();
            
            ctx.rotate((GameHandler.frameCount % 1800) / 5);
            ctx.beginPath()
            ctx.moveTo(rad * 2, 0);
            for (var i=0; i<7; i++)
            {
               ctx.rotate(PIO4);
               if (i%2 === 0)
               {
                  ctx.lineTo((rad * 2 / 0.5) * 0.2, 0);
               }
               else
               {
                  ctx.lineTo(rad * 2, 0);
               }
            }
            ctx.closePath();
            ctx.fill();
         }
         ctx.restore();
      },
      
      /**
       * Actor expiration test
       * 
       * @return true if expired and to be removed from the actor list, false if still in play
       */
      expired: function expired()
      {
         // deduct lifespan from the bullet
         return (--this.lifespan === 0);
      },
      
      power: function power()
      {
         return this.powerLevel;
      }
   });
})();
/**
 * Enemy Ship actor class.
 * 
 * @namespace Arena
 * @class Arena.EnemyShip
 */
(function()
{
   Arena.EnemyShip = function(scene, type)
   {
      // enemy score multiplier based on type buy default - but some enemies
      // will tweak this in the individual setup code later
      this.type = this.scoretype = type;
      
      // generate enemy at start position - not too close to the player
      var p, v = null;
      while (!v)
      {
         p = new Vector(Rnd() * scene.world.size, Rnd() * scene.world.size);
         if (scene.player.position.distance(p) > 220)
         {
            v = new Vector(0,0);
         }
      }
      Arena.EnemyShip.superclass.constructor.call(this, p, v);
      
      // 3D sprite object - must be created after constructor call
      var me = this;
      var obj = new K3D.K3DObject();
      with (obj)
      {
         drawmode = "wireframe";
         shademode = "depthcue";
         depthscale = 32;
         linescale = 3;
         perslevel = 256;
         
         switch (type)
         {
            case 0:
               // Dumbo: blue stretched cubiod
               me.radius = 22;
               me.playerDamage = 10;
               me.colorRGB = "rgb(0,128,255)";
               color = [0,128,255];
               addphi = -1.0; addgamma = -0.75;
               init(
                  [{x:-20,y:-20,z:12}, {x:-20,y:20,z:12}, {x:20,y:20,z:12}, {x:20,y:-20,z:12}, {x:-10,y:-10,z:-12}, {x:-10,y:10,z:-12}, {x:10,y:10,z:-12}, {x:10,y:-10,z:-12}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:4,b:5}, {a:5,b:6}, {a:6,b:7}, {a:7,b:4}, {a:0,b:4}, {a:1,b:5}, {a:2,b:6}, {a:3,b:7}],
                  []);
               break;
            
            case 1:
               // Zoner: yellow diamond
               me.radius = 22;
               me.playerDamage = 10;
               me.colorRGB = "rgb(255,255,0)";
               color = [255,255,0];
               addphi = 0.5; addgamma = -0.5; addtheta = -1.0;
               init(
                  [{x:-20,y:-20,z:0}, {x:-20,y:20,z:0}, {x:20,y:20,z:0}, {x:20,y:-20,z:0}, {x:0,y:0,z:-20}, {x:0,y:0,z:20}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:0,b:4}, {a:1,b:4}, {a:2,b:4}, {a:3,b:4}, {a:0,b:5}, {a:1,b:5}, {a:2,b:5}, {a:3,b:5}],
                  []);
               break;
            
            case 2:
               // Tracker: red flattened square
               me.radius = 22;
               me.health = 2;
               me.playerDamage = 15;
               me.colorRGB = "rgb(255,96,0)";
               color = [255,96,0];
               addgamma = 1.0;
               init(
                  [{x:-20,y:-20,z:5}, {x:-20,y:20,z:5}, {x:20,y:20,z:5}, {x:20,y:-20,z:5}, {x:-15,y:-15,z:-5}, {x:-15,y:15,z:-5}, {x:15,y:15,z:-5}, {x:15,y:-15,z:-5}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:4,b:5}, {a:5,b:6}, {a:6,b:7}, {a:7,b:4}, {a:0,b:4}, {a:1,b:5}, {a:2,b:6}, {a:3,b:7}],
                  []);
               break;
            
            case 3:
               // Borg: big green cube
               me.radius = 52;
               me.health = 5;
               me.playerDamage = 25;
               me.colorRGB = "rgb(0,255,64)";
               color = [0,255,64];
               depthscale = 96;  // tweak for larger object
               addphi = -1.5;
               init(
                  [{x:-40,y:-40,z:40}, {x:-40,y:40,z:40}, {x:40,y:40,z:40}, {x:40,y:-40,z:40}, {x:-40,y:-40,z:-40}, {x:-40,y:40,z:-40}, {x:40,y:40,z:-40}, {x:40,y:-40,z:-40}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:4,b:5}, {a:5,b:6}, {a:6,b:7}, {a:7,b:4}, {a:0,b:4}, {a:1,b:5}, {a:2,b:6}, {a:3,b:7}],
                  []);
               break;
            
            case 4:
               // Dodger: small cyan cube
               me.radius = 25;
               me.playerDamage = 10;
               me.colorRGB = "rgb(0,255,255)";
               color = [0,255,255];
               addphi = 0.5; addtheta = -3.0;
               init(
                  [{x:-20,y:-20,z:20}, {x:-20,y:20,z:20}, {x:20,y:20,z:20}, {x:20,y:-20,z:20}, {x:-20,y:-20,z:-20}, {x:-20,y:20,z:-20}, {x:20,y:20,z:-20}, {x:20,y:-20,z:-20}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:4,b:5}, {a:5,b:6}, {a:6,b:7}, {a:7,b:4}, {a:0,b:4}, {a:1,b:5}, {a:2,b:6}, {a:3,b:7}],
                  []);
               break;
            
            case 5:
               // Splitter: medium purple pyrimid (converts to 2x smaller versions when hit)
               me.radius = 25;
               me.health = 3;
               me.playerDamage = 20;
               me.colorRGB = "rgb(148,0,255)";
               color = [148,0,255];
               depthscale = 56;  // tweak for larger object
               addphi = 3.0;
               init(
                  [{x:-30,y:-20,z:0}, {x:0,y:-20,z:30}, {x:30,y:-20,z:0}, {x:0,y:-20,z:-30}, {x:0,y:30,z:0}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:0,b:4}, {a:1,b:4}, {a:2,b:4}, {a:3,b:4}],
                  []);
               break;
            
            case 6:
               // Bomber: medium magenta star - dodge bullets, dodge player!
               me.radius = 28;
               me.health = 5;
               me.playerDamage = 20;
               me.colorRGB = "rgb(255,0,255)";
               color = [255,0,255];
               depthscale = 56;  // tweak for larger object
               addgamma = -5.0;
               init(
                  [{x:-30,y:-30,z:10}, {x:-30,y:30,z:10}, {x:30,y:30,z:10}, {x:30,y:-30,z:10}, {x:-15,y:-15,z:-15}, {x:-15,y:15,z:-15}, {x:15,y:15,z:-15}, {x:15,y:-15,z:-15}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:4,b:5}, {a:5,b:6}, {a:6,b:7}, {a:7,b:4}, {a:0,b:4}, {a:1,b:5}, {a:2,b:6}, {a:3,b:7}],
                  []);
               break;
            
            case 99:
               // Splitter-mini: see Splitter above
               me.scoretype = 4;    // override default score type setting
               me.dropsMutliplier = false;
               me.radius = 12;
               me.health = 1;
               me.playerDamage = 5;
               me.colorRGB = "rgb(148,0,211)";
               color = [148,0,211];
               depthscale = 16;  // tweak for smaller object
               addphi = 5.0;
               init(
                  [{x:-15,y:-10,z:0}, {x:0,y:-10,z:15}, {x:15,y:-10,z:0}, {x:0,y:-10,z:-15}, {x:0,y:15,z:0}],
                  [{a:0,b:1}, {a:1,b:2}, {a:2,b:3}, {a:3,b:0}, {a:0,b:4}, {a:1,b:4}, {a:2,b:4}, {a:3,b:4}],
                  []);
               break;
         }
      }
      this.setK3DObject(obj);
      
      return this;
   };
   
   extend(Arena.EnemyShip, Arena.K3DActor,
   {
      BULLET_RECHARGE: 50,
      SPAWN_LENGTH: 20, // TODO: replace this with anim state machine
      aliveTime: 0,     // TODO: replace this with anim state machine
      type: 0,
      scoretype: 0,
      dropsMutliplier: true,
      health: 1,
      colorRGB: null,
      playerDamage: 0,
      bulletRecharge: 0,
      hit: false, // TODO: replace with state? - "extends" default render state...?
      
      onUpdate: function onUpdate(scene)
      {
         // TODO: replace this with anim state machine
         if (++this.aliveTime < this.SPAWN_LENGTH)
         {
            // TODO: needs enemy state implemented so can test for "alive" state
            //       for collision detection?
            //       other methods can then test state such as onRender()
            //       SPAWNED->ALIVE->DEAD
            return;
         }
         else if (this.aliveTime === this.SPAWN_LENGTH)
         {
            // initial vector needed for some enemy types - others will set later
            this.vector = new Vector(4 * (Rnd < 0.5 ? 1 : -1), 4 * (Rnd < 0.5 ? 1 : -1));
         }
         switch (this.type)
         {
            case 0:
               // dumb - change direction randomly
               if (Rnd() < 0.01)
               {
                  this.vector.y = -(this.vector.y + (0.5 - Rnd()));
               }
               break;
            
            case 1:
               // randomly reorientate towards player ("perception level")
               // so player can avade by moving around them
               if (Rnd() < 0.04)
               {
                  // head towards player - generate a vector pointed at the player
                  // by calculating a vector between the player and enemy positions
                  var v = scene.player.position.nsub(this.position);
                  // scale resulting vector down to fixed vector size i.e. speed
                  this.vector = v.scaleTo(4);
               }
               break;
            
            case 2:
               // very perceptive and faster - this one is mean
               if (Rnd() < 0.2)
               {
                  var v = scene.player.position.nsub(this.position);
                  this.vector = v.scaleTo(8);
               }
               break;
            
            case 3:
               // fast dash towards player, otherwise it slows down
               if (Rnd() < 0.03)
               {
                  var v = scene.player.position.nsub(this.position);
                  this.vector = v.scaleTo(12);
               }
               else
               {
                  this.vector.scale(0.95);
               }
               break;
            
            case 4:
               // perceptive and fast - and tries to dodgy bullets!
               var dodged = false;
               
               // if we are close to the player then don't try and dodge,
               // otherwise enemy might dash away rather than go for the kill
               if (scene.player.position.nsub(this.position).length() > 150)
               {
                  var p = this.position,
                      r = this.radius + 50;  // bullet "distance" perception
                  
                  // look at player bullets list - are any about to hit?
                  for (var i=0, j=scene.playerBullets.length, bullet, n; i < j; i++)
                  {
                     bullet = scene.playerBullets[i];
                     
                     // test the distance against the two radius combined
                     if (bullet.position.distance(p) <= bullet.radius + r)
                     {
                        // if so attempt a fast sideways dodge!
                        var v = bullet.position.nsub(p).scaleTo(12);
                        // randomise dodge direction a bit
                        v.rotate((n = Rnd()) < 0.5 ? n*PIO4 : -n*PIO4);
                        v.invert();
                        this.vector = v;
                        dodged = true;
                        break;
                     }
                  }
               }
               if (!dodged && Rnd() < 0.04)
               {
                  var v = scene.player.position.nsub(this.position);
                  this.vector = v.scaleTo(8);
               }
               break;
            
            case 5:
               if (Rnd() < 0.04)
               {
                  var v = scene.player.position.nsub(this.position);
                  this.vector = v.scaleTo(5);
               }
               break;
            
            case 6:
               // if we are near the player move away
               // if we are far from the player move towards
               var v = scene.player.position.nsub(this.position);
               if (v.length() > 400)
               {
                  // move closer
                  if (Rnd() < 0.08) this.vector = v.scaleTo(8);
               }
               else if (v.length() < 350)
               {
                  // move away
                  if (Rnd() < 0.08) this.vector = v.invert().scaleTo(8);
               }
               else
               {
                  // slow down into a firing position
                  this.vector.scale(0.8);
                  
                  // reguarly fire at the player
                  if (GameHandler.frameCount - this.bulletRecharge > this.BULLET_RECHARGE && scene.player.alive)
                  {
                     // update last fired frame and generate a bullet
                     this.bulletRecharge = GameHandler.frameCount;
                     
                     // generate a vector pointed at the player
                     // by calculating a vector between the player and enemy positions
                     // then scale to a fixed size - i.e. bullet speed
                     var v = scene.player.position.nsub(this.position).scaleTo(10);
                     // slightly randomize the direction to apply some accuracy issues
                     v.x += (Rnd() * 2 - 1);
                     v.y += (Rnd() * 2 - 1);
                     
                     var bullet = new Arena.EnemyBullet(this.position.clone(), v, 10);
                     scene.enemyBullets.push(bullet);
                  }
               }
               break;
            
            case 99: 
               if (Rnd() < 0.04)
               {
                  var v = scene.player.position.nsub(this.position);
                  this.vector = v.scaleTo(8);
               }
               break;
         }
      },
      
      /**
       * Enemy rendering method
       * 
       * @param ctx {object} Canvas rendering context
       * @param world {object} World metadata
       */
      onRender: function onRender(ctx, world)
      {
         ctx.save();
         if (this.worldToScreen(ctx, world, this.radius))
         {
            // render 3D sprite
            if (!this.hit)
            {
               ctx.shadowColor = this.colorRGB;
            }
            else
            {
               // override colour with plain white for "hit" effect
               ctx.shadowColor = "white";
               var oldColor = this.k3dObject.color;
               this.k3dObject.color = [255,255,255];
               this.k3dObject.shademode = "plain";
            }
            // TODO: replace this with anim state machine test...
            // TODO: adjust RADIUS for collision etc. during spawn!
            if (this.aliveTime < this.SPAWN_LENGTH)
            {
               // nifty scaling effect as an enemy spawns into position
               var scale = 1 - (this.SPAWN_LENGTH - this.aliveTime) / this.SPAWN_LENGTH;
               if (scale <= 0) scale = 0.01;
               else if (scale > 1) scale = 1;
               ctx.scale(scale, scale);
            }
            this.renderK3D(ctx);
            if (this.hit)
            {
               // restore colour and depthcue rendering mode
               this.k3dObject.color = oldColor;
               this.k3dObject.shademode = "depthcue";
               this.hit = false;
            }
         }
         ctx.restore();
      },
      
      damageBy: function damageBy(force)
      {
         // record hit - will change enemy colour for a single frame
         this.hit = true;
         if (force === -1 || (this.health -= force) <= 0)
         {
            this.alive = false;
         }
         return !this.alive;
      },
      
      onDestroyed: function onDestroyed(scene, player)
      {
         if (this.type === 5)
         {
            // Splitter enemy divides into two smaller ones
            var enemy = new Arena.EnemyShip(scene, 99);
            // update position and vector
            // TODO: move this as option in constructor
            enemy.vector = this.vector.nrotate(PIO2);
            enemy.position = this.position.nadd(enemy.vector);
            scene.enemies.push(enemy);
            
            enemy = new Arena.EnemyShip(scene, 99);
            enemy.vector = this.vector.nrotate(-PIO2);
            enemy.position = this.position.nadd(enemy.vector);
            scene.enemies.push(enemy);
         }
      }
   });
})();
/**
 * Particle emitter effect actor class.
 * 
 * A simple particle emitter, that does not recycle particles, but sets itself as expired() once
 * all child particles have expired.
 * 
 * Requires a function known as the emitter that is called per particle generated.
 * 
 * @namespace Arena
 * @class Arena.Particles
 */
(function()
{
   /**
    * Constructor
    * 
    * @param p {Vector} Emitter position
    * @param v {Vector} Emitter velocity
    * @param count {Integer} Number of particles
    * @param fnEmitter {Function} Emitter function to call per particle generated
    */
   Arena.Particles = function(p, v, count, fnEmitter)
   {
      Arena.Particles.superclass.constructor.call(this, p, v);
      
      // generate particles based on the supplied emitter function
      this.particles = new Array(count);
      for (var i=0; i<count; i++)
      {
         this.particles[i] = fnEmitter.call(this, i);
      }
      
      return this;
   };
   
   extend(Arena.Particles, Game.Actor,
   {
      particles: null,
      
      /**
       * Particle effect rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx, world)
      {
         ctx.save();
         ctx.shadowBlur = 0;
         ctx.globalCompositeOperation = "lighter";
         for (var i=0, particle, viewposition; i<this.particles.length; i++)
         {
            particle = this.particles[i];
            
            // update particle and test for lifespan
            if (particle.update())
            {
               viewposition = Game.worldToScreen(particle.position, world, particle.size);
               if (viewposition)
               {
                  ctx.save();
                  ctx.translate(viewposition.x, viewposition.y);
                  ctx.scale(world.scale, world.scale);
                  particle.render(ctx);
                  ctx.restore();
               }
            }
            else
            {
               // particle no longer alive, remove from list
               this.particles.splice(i, 1);
            }
         }
         ctx.restore();
      },
      
      expired: function expired()
      {
         return (this.particles.length === 0);
      }
   });
})();


/**
 * Default Arena Particle structure.
 * Currently supports three particle types; dot, line and smudge.
 */
function ArenaParticle(position, vector, size, type, lifespan, fadelength, colour)
{
   this.position = position;
   this.vector = vector;
   this.size = size;
   this.type = type;
   this.lifespan = lifespan;
   this.fadelength = fadelength;
   this.colour = colour ? colour : "rgb(255,125,50)"; // default colour if none set
   // randomize rotation speed and angle for line particle
   if (type === 1)
   {
      this.rotate = Rnd() * TWOPI;
      this.rotationv = Rnd() - 0.5;
   }
   
   this.update = function()
   {
      this.position.add(this.vector);
      return (--this.lifespan !== 0);
   };
   
   this.render = function(ctx)
   {
  	   // NOTE: the try/catch here is to handle where FireFox gets
  	   //       upset when rendering images outside the canvas area
      try
	  	{
         ctx.globalAlpha = (this.lifespan < this.fadelength ? ((1 / this.fadelength) * this.lifespan) : 1);
         switch (this.type)
         {
            case 0:  // point (prerendered image)
               // prerendered images for each enemy colour with health > 1
               // lookup based on particle colour e.g. points_rgb(x,y,z)
               ctx.drawImage(
                  GameHandler.prerenderer.images["points_" + this.colour][this.size], 0, 0);
               break;
            case 1:  // line
               var s = this.size;
               ctx.rotate(this.rotate);
               this.rotate += this.rotationv;
               // specific line colour - for enemy explosion pieces
               ctx.strokeStyle = this.colour;
               ctx.lineWidth = 2.0;
               ctx.beginPath();
               ctx.moveTo(-s, -s);
               ctx.lineTo(s, s);
               ctx.closePath();
               ctx.stroke();
               break;
            case 2:  // smudge (prerendered image)
      		  	ctx.drawImage(GameHandler.prerenderer.images["smudges"][this.size - 4], 0, 0);
      		  	break;
         }
      }
      catch (error)
      {
         if (console !== undefined) console.log(error.message);
      }
   };
}


/**
 * Enemy explosion - Particle effect actor class.
 * 
 * @namespace Arena
 * @class Arena.EnemyExplosion
 */
(function()
{
   /**
    * Constructor
    */
   Arena.EnemyExplosion = function(p, v, enemy)
   {
      Arena.EnemyExplosion.superclass.constructor.call(this, p, v, 16, function()
         {
            // randomise start position slightly
            var pos = p.clone();
            pos.x += randomInt(-5, 5);
            pos.y += randomInt(-5, 5);
            // randomise radial direction vector - speed and angle, then add parent vector
            switch (randomInt(0, 2))
            {
               case 0:
                  var t = new Vector(0, randomInt(20, 25));
                  t.rotate(Rnd() * TWOPI);
                  t.add(v);
                  return new ArenaParticle(
                     pos, t, ~~(Rnd() * 4), 0, 20, 15);
               case 1:
                  var t = new Vector(0, randomInt(5, 10));
                  t.rotate(Rnd() * TWOPI);
                  t.add(v);
                  // create line particle - size based on enemy type
                  return new ArenaParticle(
                     pos, t, (enemy.type !== 3 ? Rnd() * 5 + 5 : Rnd() * 10 + 10), 1, 20, 15, enemy.colorRGB);
               case 2:
                  var t = new Vector(0, randomInt(2, 4));
                  t.rotate(Rnd() * TWOPI);
                  t.add(v);
                  return new ArenaParticle(
                     pos, t, ~~(Rnd() * 4 + 4), 2, 20, 15);
            }
         });
      
      return this;
   };
   
   extend(Arena.EnemyExplosion, Arena.Particles);
})();


/**
 * Enemy impact effect - Particle effect actor class.
 * Used when an enemy is hit by player bullet but not destroyed.
 * 
 * @namespace Arena
 * @class Arena.EnemyImpact
 */
(function()
{
   /**
    * Constructor
    */
   Arena.EnemyImpact = function(p, v, enemy)
   {
      Arena.EnemyImpact.superclass.constructor.call(this, p, v, 5, function()
         {
            // slightly randomise vector angle - then add parent vector
            var t = new Vector(0, Rnd() < 0.5 ? randomInt(-5, -10) : randomInt(5, 10));
            t.rotate(Rnd() * PIO2 - PIO4);
            t.add(v);
            return new ArenaParticle(
               p.clone(), t, ~~(Rnd() * 4), 0, 15, 10, enemy.colorRGB);
         });
      
      return this;
   };
   
   extend(Arena.EnemyImpact, Arena.Particles);
})();


/**
 * Bullet impact effect - Particle effect actor class.
 * Used when an bullet hits an object and is destroyed.
 * 
 * @namespace Arena
 * @class Arena.BulletImpactEffect
 */
(function()
{
   /**
    * Constructor
    */
   Arena.BulletImpactEffect = function(p, v, enemy)
   {
      Arena.BulletImpactEffect.superclass.constructor.call(this, p, v, 3, function()
         {
            return new ArenaParticle(
               p.clone(), v.nrotate(Rnd()*PIO8), ~~(Rnd() * 4), 0, 15, 10);
         });
      
      return this;
   };
   
   extend(Arena.BulletImpactEffect, Arena.Particles);
})();


/**
 * Player explosion - Particle effect actor class.
 * 
 * @namespace Arena
 * @class Arena.PlayerExplosion
 */
(function()
{
   /**
    * Constructor
    */
   Arena.PlayerExplosion = function(p, v)
   {
      Arena.PlayerExplosion.superclass.constructor.call(this, p, v, 20, function()
         {
            // randomise start position slightly
            var pos = p.clone();
            pos.x += randomInt(-5, 5);
            pos.y += randomInt(-5, 5);
            // randomise radial direction vector - speed and angle, then add parent vector
            switch (randomInt(1,2))
            {
               case 1:
                  var t = new Vector(0, randomInt(5, 8));
                  t.rotate(Rnd() * TWOPI);
                  t.add(v);
                  return new ArenaParticle(
                     pos, t, Rnd() * 5 + 5, 1, 25, 15, "white");
               case 2:
                  var t = new Vector(0, randomInt(5, 10));
                  t.rotate(Rnd() * TWOPI);
                  t.add(v);
                  return new ArenaParticle(
                     pos, t, ~~(Rnd() * 4 + 4), 2, 25, 15);
            }
         });
      
      return this;
   };
   
   extend(Arena.PlayerExplosion, Arena.Particles);
})();


/**
 * Text indicator effect actor class.
 * 
 * @namespace Arena
 * @class Arena.TextIndicator
 */
(function()
{
   Arena.TextIndicator = function(p, v, msg, textSize, colour, fadeLength)
   {
      this.fadeLength = (fadeLength ? fadeLength : this.DEFAULT_FADE_LENGTH);
      Arena.TextIndicator.superclass.constructor.call(this, p, v, this.fadeLength);
      this.msg = msg;
      if (textSize)
      {
         this.textSize = textSize;
      }
      if (colour)
      {
         this.colour = colour;
      }
      return this;
   };
   
   extend(Arena.TextIndicator, Game.EffectActor,
   {
      DEFAULT_FADE_LENGTH: 16,
      fadeLength: 0,
      textSize: 22,
      msg: null,
      colour: "rgb(255,255,255)",
      
      /**
       * Text indicator effect rendering method
       * 
       * @param ctx {object} Canvas rendering context
       */
      onRender: function onRender(ctx, world)
      {
         ctx.save();
         if (this.worldToScreen(ctx, world, 128))
         {
            var alpha = (1.0 / this.fadeLength) * this.lifespan;
            ctx.globalAlpha = alpha;
            ctx.shadowBlur = 0;
            Game.fillText(ctx, this.msg, this.textSize + "pt Courier New", 0, 0, this.colour);
         }
         ctx.restore();
      }
   });
})();


/**
 * Score indicator effect actor class.
 * 
 * @namespace Arena
 * @class Arena.ScoreIndicator
 */
(function()
{
   Arena.ScoreIndicator = function(p, v, score, textSize, prefix, colour, fadeLength)
   {
      var msg = score.toString();
      if (prefix)
      {
         msg = prefix + ' ' + msg;
      }
      Arena.ScoreIndicator.superclass.constructor.call(this, p, v, msg, textSize, colour, fadeLength);
      return this;
   };
   
   extend(Arena.ScoreIndicator, Arena.TextIndicator,
   {
   });
})();
/**
 * CanvasMark HTML5 Canvas Rendering Benchmark - March 2013
 *
 * @email kevtoast at yahoo dot com
 * @twitter kevinroast
 *
 * (C) 2013 Kevin Roast
 * 
 * Please see: license.txt
 * You are welcome to use this code, but I would appreciate an email or tweet
 * if you do anything interesting with it!
 */


window.addEventListener('load', onloadHandler, false);

/**
 * Global window onload handler
 */
var g_splashImg = new Image();
function onloadHandler()
{
   // once the slash screen is loaded, bootstrap the main benchmark class
   g_splashImg.src = 'images/canvasmark2013.jpg';
   g_splashImg.onload = function()
   {
      // init our game with Game.Main derived instance
      GameHandler.init();
      GameHandler.start(new Benchmark.Main());
   };
}


/**
 * Benchmark root namespace.
 * 
 * @namespace Benchmark
 */
if (typeof Benchmark == "undefined" || !Benchmark)
{
   var Benchmark = {};
}


/**
 * Benchmark main class.
 * 
 * @namespace Benchmark
 * @class Benchmark.Main
 */
(function()
{
   Benchmark.Main = function()
   {
      Benchmark.Main.superclass.constructor.call(this);
      
      // create the scenes that are directly part of the Benchmark container
      var infoScene = new Benchmark.InfoScene(this);
      
      // add the info scene - must be added first
      this.scenes.push(infoScene);
      
      // create the Test instances that the benchmark should manage
      // each Test instance will add child scenes to the benchmark
      var loader = new Game.Preloader();
      this.asteroidsTest = new Asteroids.Test(this, loader);
      this.arenaTest = new Arena.Test(this, loader);
      this.featureTest = new Feature.Test(this, loader);
      
      // add benchmark completed scene
      this.scenes.push(new Benchmark.CompletedScene(this));
      
      // the benchmark info scene is displayed first and responsible for allowing the
      // benchmark to start once images required by the game engines have been loaded
      loader.onLoadCallback(function() {
         infoScene.ready();
      });
   };
   
   extend(Benchmark.Main, Game.Main,
   {
      asteroidsTest: null,
      arenaTest: null,
      featureTest: null,
      
      addBenchmarkScene: function addBenchmarkScene(scene)
      {
         this.scenes.push(scene);
      }
   });
})();


/**
 * Benchmark Benchmark Info Scene scene class.
 * 
 * @namespace Benchmark
 * @class Benchmark.InfoScene
 */
(function()
{
   Benchmark.InfoScene = function(game)
   {
      this.game = game;
      
      // allow start via mouse click - also for starting benchmark on touch devices
      var me = this;
      var fMouseDown = function(e)
      {
         if (e.button == 0)
         {
            if (me.imagesLoaded)
            {
               me.start = true;
               return true;
            }
         }
      };
      GameHandler.canvas.addEventListener("mousedown", fMouseDown, false);
      
      Benchmark.InfoScene.superclass.constructor.call(this, false, null);
   };
   
   extend(Benchmark.InfoScene, Game.Scene,
   {
      game: null,
      start: false,
      imagesLoaded: false,
      sceneStarted: null,
      loadingMessage: false,
      
      /**
       * Scene completion polling method
       */
      isComplete: function isComplete()
      {
         return this.start;
      },
      
      onInitScene: function onInitScene()
      {
         this.playable = false;
         this.start = false;
         this.yoff = 1;
      },
      
      onRenderScene: function onRenderScene(ctx)
      {
         ctx.save();
         if (this.imagesLoaded)
         {
            // splash logo image dimensions
            var w = 640, h = 640;
            if (this.yoff < h - 1)
            {
               // liquid fill bg effect
               ctx.drawImage(g_splashImg, 0, 0, w, this.yoff, 0, 0, w, this.yoff);
               ctx.drawImage(g_splashImg, 0, this.yoff, w, 2, 0, this.yoff, w, h-this.yoff);
               this.yoff++;
            }
            else
            {
               var toff = (GameHandler.height/2 + 196), tsize = 40;
               ctx.drawImage(g_splashImg, 0, toff-tsize+12, w, tsize, 0, toff-tsize+12, w, tsize);
               ctx.shadowBlur = 6;
               ctx.shadowColor = "#000";
               // alpha fade bounce in a single tertiary statement using a single counter
               // first 64 values of 128 perform a fade in, for second 64 values, fade out
               ctx.globalAlpha = (this.yoff % 128 < 64) ? ((this.yoff % 64) / 64) : (1 - ((this.yoff % 64) / 64));
               
               Game.centerFillText(ctx, "Click or press SPACE to run CanvasMark", "18pt Helvetica", toff, "#fff");
            }
            this.yoff++;
         }
         else if (!this.loadingMessage)
         {
            Game.centerFillText(ctx, "Please wait... Loading Images...", "18pt Helvetica", GameHandler.height/2, "#eee");
            this.loadingMessage = true;
         }
         ctx.restore();
      },
      
      /**
       * Callback from image preloader when all images are ready
       */
      ready: function ready()
      {
         this.imagesLoaded = true;
         if (location.search === "?auto=true")
         {
            this.start = true;
         }
      },
      
      onKeyDownHandler: function onKeyDownHandler(keyCode)
      {
         switch (keyCode)
         {
            case KEY.SPACE:
            {
               if (this.imagesLoaded)
               {
                  this.start = true;
               }
               return true;
               break;
            }
         }
      }
   });
})();


/**
 * Benchmark CompletedScene scene class.
 * 
 * @namespace Benchmark
 * @class Benchmark.CompletedScene
 */
(function()
{
   Benchmark.CompletedScene = function(game)
   {
      this.game = game;
      
      // construct the interval to represent the Game Over text effect
      var interval = new Game.Interval("CanvasMark Completed!", this.intervalRenderer);
      Benchmark.CompletedScene.superclass.constructor.call(this, false, interval);
   };
   
   extend(Benchmark.CompletedScene, Game.Scene,
   {
      game: null,
      exit: false,
      
      /**
       * Scene init event handler
       */
      onInitScene: function onInitScene()
      {
         this.game.fps = 1;
         this.interval.reset();
         this.exit = false;
      },
      
      /**
       * Scene completion polling method
       */
      isComplete: function isComplete()
      {
         return true;
      },
      
      intervalRenderer: function intervalRenderer(interval, ctx)
      {
         ctx.clearRect(0, 0, GameHandler.width, GameHandler.height);
         var score = GameHandler.benchmarkScoreCount;
         if (interval.framecounter === 0)
         {
            var browser = BrowserDetect.browser + " " + BrowserDetect.version;
            var OS = BrowserDetect.OS;
            
            if (location.search === "?auto=true")
            {
				if ( typeof tpRecordTime !== "undefined" ) {
				  tpRecordTime(GameHandler.benchmarkScores.join(','), 0, GameHandler.benchmarkLabels.join(','));
				} else {
				  alert(score);
				}
            }
            else
            {
               // write results to browser
               $("#results").html("<p>CanvasMark Score: " + score + " (" + browser + " on " + OS + ")</p>");
               // tweet this result link
               var tweet = "http://twitter.com/home/?status=" + browser + " (" + OS + ") scored " + score + " in the CanvasMark HTML5 benchmark! Test your browser: http://bit.ly/canvasmark %23javascript %23html5";
               $("#tweetlink").attr('href', tweet.replace(/ /g, "%20"));
               $("#results-wrapper").fadeIn();
            }
         }
         Game.centerFillText(ctx, interval.label, "18pt Helvetica", GameHandler.height/2 - 32, "white");
         Game.centerFillText(ctx, "Benchmark Score: " + score, "14pt Helvetica", GameHandler.height/2, "white");
         
         interval.complete = (this.exit || interval.framecounter++ > 400);
      },
      
      onKeyDownHandler: function onKeyDownHandler(keyCode)
      {
         switch (keyCode)
         {
            case KEY.SPACE:
            {
               this.exit = true;
               return true;
               break;
            }
         }
      }
   });
})();


var BrowserDetect = {
	init: function () {
		this.browser = this.searchString(this.dataBrowser) || "Unknown Browser";
		this.version = this.searchVersion(navigator.userAgent)
			|| this.searchVersion(navigator.appVersion)
			|| "an unknown version";
		this.OS = this.searchString(this.dataOS) || "Unknown OS";
	},
	searchString: function (data) {
		for (var i=0;i<data.length;i++)	{
			var dataString = data[i].string;
			var dataProp = data[i].prop;
			this.versionSearchString = data[i].versionSearch || data[i].identity;
			if (dataString) {
				if (dataString.indexOf(data[i].subString) != -1)
					return data[i].identity;
			}
			else if (dataProp)
				return data[i].identity;
		}
	},
	searchVersion: function (dataString) {
		var index = dataString.indexOf(this.versionSearchString);
		if (index == -1) return;
		return parseFloat(dataString.substring(index+this.versionSearchString.length+1));
	},
	dataBrowser: [
		{
			string: navigator.userAgent,
			subString: "Chrome",
			identity: "Chrome"
		},
		{ 	string: navigator.userAgent,
			subString: "OmniWeb",
			versionSearch: "OmniWeb/",
			identity: "OmniWeb"
		},
		{
			string: navigator.vendor,
			subString: "Apple",
			identity: "Safari",
			versionSearch: "Version"
		},
		{
			prop: window.opera,
			identity: "Opera",
			versionSearch: "Version"
		},
		{
			string: navigator.vendor,
			subString: "iCab",
			identity: "iCab"
		},
		{
			string: navigator.vendor,
			subString: "KDE",
			identity: "Konqueror"
		},
		{
			string: navigator.userAgent,
			subString: "Firefox",
			identity: "Firefox"
		},
		{
			string: navigator.vendor,
			subString: "Camino",
			identity: "Camino"
		},
		{		// for newer Netscapes (6+)
			string: navigator.userAgent,
			subString: "Netscape",
			identity: "Netscape"
		},
		{
			string: navigator.userAgent,
			subString: "MSIE",
			identity: "IE",
			versionSearch: "MSIE"
		},
		{
			string: navigator.userAgent,
			subString: "Gecko",
			identity: "Mozilla",
			versionSearch: "rv"
		},
		{ 		// for older Netscapes (4-)
			string: navigator.userAgent,
			subString: "Mozilla",
			identity: "Netscape",
			versionSearch: "Mozilla"
		}
	],
	dataOS : [
		{
			string: navigator.platform,
			subString: "Win",
			identity: "Windows"
		},
		{
			string: navigator.platform,
			subString: "Mac",
			identity: "Mac"
		},
		{
		   string: navigator.userAgent,
		   subString: "iPhone",
		   identity: "iOS"
	   },
	   {
		   string: navigator.userAgent,
		   subString: "iPod",
		   identity: "iOS"
	   },
	   {
		   string: navigator.userAgent,
		   subString: "iPad",
		   identity: "iOS"
	   },
		{
			string: navigator.platform,
			subString: "Linux",
			identity: "Linux"
		}
	]
};
BrowserDetect.init();
