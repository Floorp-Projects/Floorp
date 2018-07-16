package com.leanplum;
import android.annotation.TargetApi;
import android.app.job.JobParameters;
import android.app.job.JobService;

/**
 * Leanplum GCM registration Job Service to start registration service.
 *
 * @author Anna Orlova
 */
@TargetApi(21)
public class LeanplumGcmRegistrationJobService extends JobService {
    public static final int JOB_ID = -63722755;

    @Override
    public boolean onStartJob(JobParameters jobParameters) {
        LeanplumNotificationHelper.startPushRegistrationService(this, "GCM");
        return false;
    }

    @Override
    public boolean onStopJob(JobParameters jobParameters) {
        return false;
    }
}