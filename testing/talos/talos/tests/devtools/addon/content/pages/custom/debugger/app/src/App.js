import React, { Component } from 'react';
import { stepInTest, debugStatement } from './step-in-test.js';
import stepOverTest from './step-over-test.js';
import stepOutTest from './step-out-test.js';

class App extends Component {
  render() {
    return (
      <div className="App">
        <header className="App-header">
          <h1 className="App-title">Welcome to React</h1>
        </header>
        <p className="App-intro">
          To get started, edit <code>src/App.js</code> and save to reload.
        </p>
      </div>
    );
  }
}

window.hitBreakpoint = function breakpoint() {
  stepInTest();
  stepOverTest();
  stepOutTest();
  console.log('hitting a breakpoint');
  return;
}

window.hitDebugStatement = function() {
  debugStatement();
  console.log('hitting a debug statement');
  return;
}

export default App;
