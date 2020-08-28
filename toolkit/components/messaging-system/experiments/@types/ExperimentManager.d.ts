export interface FeatureConfig {
  featureId: "cfr" | "aboutwelcome";
  enabled: boolean;
  value: { [key: string]: any } | null;
}

export interface Branch {
  slug: string;
  ratio: number;
  feature: FeatureConfig;
}

interface BucketConfig {
  namespace: string;
  randomizationUnit: string;
  start: number;
  count: number;
  total: number;
}

export interface RecipeArgs {
  slug: string;
  isEnrollmentPaused: boolean;
  experimentType?: string;
  branches: Branch[];
  bucketConfig: BucketConfig;
}

export interface Recipe {
  id: string;
  // Processed by Remote Settings, Normandy
  filter_expression?: string;
  // Processed by RemoteSettingsExperimentLoader
  targeting?: string;
  arguments: RecipeArgs;
}

export interface Enrollment {
  slug: string;
  enrollmentId: string;
  branch: Branch;
  active: boolean;
  experimentType: string;
  source: string;
  // Shown in about:studies
  userFacingName: string;
  userFacingDescription: string;
  lastSeen: string;
}
