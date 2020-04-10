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

export interface Enrollment {
  slug: string;
  enrollmentId: string;
  branch: Branch;
  active: boolean;
  experimentType: string;
}
