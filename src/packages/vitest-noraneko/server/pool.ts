import { createMethodsRPC, ProcessPool, TestSpecification } from "vitest/node";

import { RunnerRPC, RunnerTaskResultPack, RunnerTestCase, RunnerTestFile, RuntimeRPC } from "vitest";
import {generateFileHash, getTasks} from "@vitest/runner/utils"
import { relative } from "pathe";
import WebSocket, { WebSocketServer } from "ws";
import { BirpcReturn, createBirpc } from "birpc";
import { WSPoolFunctions, WSRunnerFunctions } from "./types";
import { VitestRunner } from "@vitest/runner";

async function initWSS() {
  const wss = new WebSocketServer({port:5201});
  let resolve : Function | null = null;
  let reject : Function | null = null;
  let promise = new Promise<WebSocket>((s,j)=>{resolve = s;reject=j})
  wss.on("connection",(ws)=>{
    resolve!;
    (resolve as Function)(ws)
  });
  return promise
}

class NoranekoTestPool implements ProcessPool {
  name: string = "vitest-noraneko-pool";
  
  async runTests(files: TestSpecification[],invalidates?: string[]) {
    const ws = await initWSS();
    const wsRPC = createBirpc<WSRunnerFunctions,WSPoolFunctions>(this,{
      post: data => ws.send(data),
      on: callback => ws.on('message', callback),
      // these are required when using WebSocket
      serialize: v => JSON.stringify(v),
      deserialize: v => JSON.parse(v),
    })
    for (const spec of files) {
      const ctx = spec.project.vitest;
      const project = spec.project;
      // fullpath of test file
      // const path = spec.moduleId;
      await wsRPC.registerConfig(project.serializedConfig);
      createBirpc<VitestRunner,{}>(this,{
        post: data => ws.send(data),
        on: callback => ws.on('message', callback),
        // these are required when using WebSocket
        serialize: v => JSON.stringify(v),
        deserialize: v => JSON.parse(v),
      })

      // clear for when rerun
      ctx.state.clearFiles(project);
      const path = relative(project.config.root, spec.moduleId);
      
      // Register Test Files
      {
        const testFile: RunnerTestFile = {
          id: generateFileHash(path, project.config.name),
          name: path,
          mode: 'run',
          meta: {typecheck:true},
          projectName: project.name,
          filepath: path,
          type: 'suite',
          tasks:[],
          result: {
            state:"pass"
          },
          file: null!,
        }
        testFile.file = testFile

        const taskTest: RunnerTestCase = {
          type: 'test',
          name: 'custom test',
          id: 'custom-test',
          context: {} as any,
          suite: testFile,
          mode: 'run',
          meta: {},
          result: {
            state:"pass"
          },
          file: testFile,
        }

        testFile.tasks.push(taskTest)

        ctx.state.collectFiles(project,[testFile])
        ctx.state.updateTasks(getTasks(testFile).map(task => [
          task.id,
          task.result,
          task.meta,
        ]));
      }
    }
  }
  collectTests(files: TestSpecification[],invalidates?: string[]){
    for (const file of files) {
      console.log(file.moduleId);
    }
  }
  async close() : Promise<void>{

  }

}

export default () => new NoranekoTestPool()