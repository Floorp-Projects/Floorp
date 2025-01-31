import type { RunnerTestFile } from 'vitest'
import type { VitestRunner, VitestRunnerConfig } from 'vitest/suite'
import { VitestTestRunner } from 'vitest/runners'

class NoranekoRunner extends VitestTestRunner implements VitestRunner {
}

export default NoranekoRunner;