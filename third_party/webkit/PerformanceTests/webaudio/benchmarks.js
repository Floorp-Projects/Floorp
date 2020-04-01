if (typeof(window) == "undefined") {
  benchmarks = []
  registerTestFile = function() {}
  registerTestCase = function(o) { return benchmarks.push(o.name); }
}

registerTestFile("think-mono-48000.wav");
registerTestFile("think-mono-44100.wav");
registerTestFile("think-mono-38000.wav");
registerTestFile("think-stereo-48000.wav");
registerTestFile("think-stereo-44100.wav");
registerTestFile("think-stereo-38000.wav");

registerTestCase({
  func: function () {
    var oac = new OfflineAudioContext(1, DURATION * samplerate, samplerate);
    return oac;
  },
  name: "Empty testcase"
});

registerTestCase({
  func: function () {
    var oac = new OfflineAudioContext(1, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    source0.buffer = getSpecificFile({rate: oac.samplerate, channels:1});
    source0.loop = true;
    source0.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Simple gain test without resampling"
});

registerTestCase({
  func: function() {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    source0.buffer = getSpecificFile({rate: oac.samplerate, channels:2});
    source0.loop = true;
    source0.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Simple gain test without resampling (Stereo)"
});

registerTestCase({
  func: function () {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    var panner = oac.createPanner();
    source0.buffer = getSpecificFile({rate: oac.samplerate, channels:2});
    source0.loop = true;
    panner.setPosition(1, 2, 3);
    panner.setOrientation(10, 10, 10);
    source0.connect(panner);
    panner.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Simple gain test without resampling (Stereo and positional)"
});

registerTestCase({
  func: function() {
    var oac = new OfflineAudioContext(1, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    source0.buffer = getSpecificFile({rate: 38000, channels:1});
    source0.loop = true;
    source0.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Simple gain test"
});

registerTestCase({
  func: function() {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    source0.buffer = getSpecificFile({rate: 38000, channels:2});
    source0.loop = true;
    source0.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Simple gain test (Stereo)"
});

registerTestCase({
  func: function() {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    var panner = oac.createPanner();
    source0.buffer = getSpecificFile({rate: 38000, channels:2});
    source0.loop = true;
    panner.setPosition(1, 2, 3);
    panner.setOrientation(10, 10, 10);
    source0.connect(panner);
    panner.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Simple gain test (Stereo and positional)"
});

registerTestCase({
  func: function() {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    source0.buffer = getSpecificFile({rate: oac.samplerate, channels:1});
    source0.loop = true;
    source0.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Upmix without resampling (Mono -> Stereo)"
});

registerTestCase({
  func: function() {
    var oac = new OfflineAudioContext(1, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    source0.buffer = getSpecificFile({rate: oac.samplerate, channels:2});
    source0.loop = true;
    source0.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Downmix without resampling (Mono -> Stereo)"
});

registerTestCase({
  func: function() {
    var duration_adjusted = DURATION / 4;
    var oac = new OfflineAudioContext(2, duration_adjusted * samplerate, samplerate);
    for (var i = 0; i < 100; i++) {
      var source0 = oac.createBufferSource();
      source0.buffer = getSpecificFile({rate: 38000, channels:1});
      source0.loop = true;
      source0.connect(oac.destination);
      source0.start(0);
    }
    return oac;
  },
  name: "Simple mixing (same buffer)"
});

registerTestCase({
  func: function() {
    var duration_adjusted = DURATION / 4;
    var oac = new OfflineAudioContext(2, duration_adjusted * samplerate, samplerate);
    var reference = getSpecificFile({rate: 38000, channels:1}).getChannelData(0);
    for (var i = 0; i < 100; i++) {
      var source0 = oac.createBufferSource();
      // copy the buffer into the a new one, so we know the implementation is not
      // sharing them.
      var b = oac.createBuffer(1, reference.length, 38000);
      var data = b.getChannelData(0);
      for (var j = 0; j < b.length; j++) {
        data[i] = reference[i];
      }
      source0.buffer = b;
      source0.loop = true;
      source0.connect(oac.destination);
      source0.start(0);
    }
    return oac;
  },
  name: "Simple mixing (different buffers)"
});

registerTestCase({
  func: function() {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var gain = oac.createGain();
    gain.gain.value = -1;
    gain.connect(oac.destination);
    var gainsi = [];
    for (var i = 0; i < 4; i++) {
      var gaini = oac.createGain();
      gaini.gain.value = 0.25;
      gaini.connect(gain);
      gainsi[i] = gaini
    }
    for (var j = 0; j < 2; j++) {
      var sourcej = oac.createBufferSource();
      sourcej.buffer = getSpecificFile({rate: 38000, channels:1});
      sourcej.loop = true;
      sourcej.start(0);
      for (var i = 0; i < 4; i++) {
        var gainij = oac.createGain();
        gainij.gain.value = 0.5;
        gainij.connect(gainsi[i]);
        sourcej.connect(gainij);
      }
    }
    return oac;
  },
  name: "Simple mixing with gains"
});

registerTestCase({
  func: function() {
    var duration_adjusted = DURATION / 8;
    var oac = new OfflineAudioContext(1, duration_adjusted * samplerate, samplerate);
    var i,l;
    var decay = 10;
    var duration = 4;
    var len = samplerate * duration;
    var buffer = ac.createBuffer(2, len, oac.sampleRate)
    var iL = buffer.getChannelData(0)
    var iR = buffer.getChannelData(1)
    // Simple exp decay loop
    for(i=0,l=buffer.length;i<l;i++) {
      iL[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, decay);
      iR[i] = (Math.random() * 2 - 1) * Math.pow(1 - i / len, decay);
    }
    var convolver = oac.createConvolver();
    convolver.buffer = buffer;
    convolver.connect(oac.destination);

    var audiobuffer = getSpecificFile({rate: samplerate, channels:1});
    var source0 = oac.createBufferSource();
    source0.buffer = audiobuffer;
    source0.loop = true;
    source0.connect(convolver);
    source0.start(0);
    return oac;
  },
  name: "Convolution reverb"
});

registerTestCase({
  func: function() {
    // this test case is very slow in chrome, reduce the total duration to avoid
    // timeout
    var duration_adjusted = DURATION / 8;
    var oac = new OfflineAudioContext(1, duration_adjusted * samplerate, samplerate);
    var duration = DURATION * samplerate;
    var audiobuffer = getSpecificFile({rate: samplerate, channels:1});
    var offset = 0;
    while (offset < duration / samplerate) {
      var grain = oac.createBufferSource();
      var gain = oac.createGain();
      grain.connect(gain);
      gain.connect(oac.destination);
      grain.buffer = audiobuffer;
      // get a random 100-ish ms with enveloppes
      var start = offset * Math.random() * 0.5;
      var end = start + 0.005 * (0.999 * Math.random());
      grain.start(offset, start, end);
      gain.gain.setValueAtTime(offset, 0);
      gain.gain.linearRampToValueAtTime(.5, offset + 0.005);
      var startRelease = Math.max(offset + (end - start), 0);
      gain.gain.setValueAtTime(0.5, startRelease);
      gain.gain.linearRampToValueAtTime(0.0, startRelease + 0.05);

      // some overlap
      offset += 0.005;
    }
    return oac;
  },
  name: "Granular synthesis"
});

registerTestCase({
  func: function() {
    var samplerate = 44100;
    var duration  = DURATION;
    var oac = new OfflineAudioContext(1, duration * samplerate, 44100);
    var offset = 0;
    while (offset < duration) {
      var note = oac.createOscillator();
      var env = oac.createGain();
      note.type = "sawtooth";
      note.frequency.value = 110;
      note.connect(env);
      env.gain.setValueAtTime(0, 0);
      env.gain.setValueAtTime(0.5, offset);
      env.gain.setTargetAtTime(0, offset+0.01, 0.1);
      env.connect(oac.destination);
      note.start(offset);
      note.stop(offset + 1.0);
      offset += 140 / 60 / 4; // 140 bpm
    }
    return oac;
  },
  name: "Synth"
});

registerTestCase({
  func: function() {
    var samplerate = 44100;
    var duration  = DURATION;
    var oac = new OfflineAudioContext(1, duration * samplerate, samplerate);
    var offset = 0;
    var osc = oac.createOscillator();
    osc.type = "sawtooth";
    var enveloppe = oac.createGain();
    enveloppe.gain.setValueAtTime(0, 0);
    var filter = oac.createBiquadFilter();
    osc.connect(enveloppe);
    enveloppe.connect(filter);
    filter.connect(oac.destination);
    filter.frequency.setValueAtTime(0.0, 0.0);
    filter.Q.setValueAtTime(20, 0.0);
    osc.start(0);
    osc.frequency.setValueAtTime(110, 0);

    while (offset < duration) {
      enveloppe.gain.setValueAtTime(1.0, offset);
      enveloppe.gain.setTargetAtTime(0.0, offset, 0.1);
      filter.frequency.setValueAtTime(0, offset);
      filter.frequency.setTargetAtTime(3500, offset, 0.03);
      offset += 140 / 60 / 16;
    }
    return oac;
  },
  name: "Substractive synth"
});

registerTestCase({
  func: function () {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    var panner = oac.createStereoPanner();
    source0.buffer = getSpecificFile({rate: oac.samplerate, channels:2});
    source0.loop = true;
    panner.pan = 0.1;
    source0.connect(panner);
    panner.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Stereo Panning"
});

registerTestCase({
  func: function () {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var source0 = oac.createBufferSource();
    var panner = oac.createStereoPanner();
    source0.buffer = getSpecificFile({rate: oac.samplerate, channels:2});
    source0.loop = true;
    panner.pan.setValueAtTime(-0.1, 0.0);
    panner.pan.setValueAtTime(0.2, 0.5);
    source0.connect(panner);
    panner.connect(oac.destination);
    source0.start(0);
    return oac;
  },
  name: "Stereo Panning with Automation"
});

registerTestCase({
  func: function () {
    var oac = new OfflineAudioContext(2, DURATION * samplerate, samplerate);
    var osc = oac.createOscillator();
    osc.type = 'sawtooth';
    var freq = 2000;
    osc.frequency.value = freq;
    osc.frequency.linearRampToValueAtTime(20, 10.0);
    osc.connect(oac.destination);
    osc.start(0);
    return oac;
  },
  name: "Periodic Wave with Automation"
});



if (typeof(window) == "undefined") {
  exports.benchmarks = benchmarks;
}

