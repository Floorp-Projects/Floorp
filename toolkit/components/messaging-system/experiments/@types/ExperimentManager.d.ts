export interface Branch {
  slug: string;
  ratio: number;
  groups: string[];
  value: any;
}

export interface RecipeArgs {
  slug: string;
  isEnrollmentPaused?: boolean;
  experimentType?: string;
  branches: Branch[];
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
}
