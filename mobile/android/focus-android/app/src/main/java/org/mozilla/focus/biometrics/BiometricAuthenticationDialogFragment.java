package org.mozilla.focus.biometrics;

import android.app.Dialog;
import android.app.DialogFragment;
import android.arch.lifecycle.LifecycleObserver;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.RequiresApi;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatDialogFragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import org.mozilla.focus.R;

@RequiresApi(api = Build.VERSION_CODES.M)
public class BiometricAuthenticationDialogFragment extends AppCompatDialogFragment implements LifecycleObserver {
    public static final String FRAGMENT_TAG = "biometric-dialog-fragment";
    private TextView mErrorTextView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Do not create a new Fragment when the Activity is re-created such as orientation changes.
        setRetainInstance(true);
        setStyle(DialogFragment.STYLE_NORMAL, android.R.style.Theme_Material_Light_Dialog);
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        super.onCancel(dialog);
        Intent startMain = new Intent(Intent.ACTION_MAIN);
        startMain.addCategory(Intent.CATEGORY_HOME);
        startMain.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(startMain);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {

        View v = inflater.inflate(R.layout.biometric_prompt_dialog_content, container,
                false);

        Dialog biometricDialog = getDialog();
        this.setCancelable(true);
        biometricDialog.setCanceledOnTouchOutside(false);
        biometricDialog.setTitle(getContext().getString(R.string.biometric_auth_title,
                getContext().getString(R.string.app_name)));

        TextView description = v.findViewById(R.id.description);
        description.setText(R.string.biometric_auth_description);

        Button mNewSessionButton = v.findViewById(R.id.new_session_button);
        mNewSessionButton.setText(R.string.biometric_auth_new_session);
        mNewSessionButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                newSessionButtonClicked();
                dismiss();
            }
        });

        ImageView mFingerprintContent = v.findViewById(R.id.fingerprint_icon);
        mFingerprintContent.setImageResource(R.drawable.ic_fingerprint);
        mErrorTextView = v.findViewById(R.id.biometric_error_text);

        return v;
    }

    @Override
    public void onResume() {
        super.onResume();
        resetErrorText();
    }

    public interface BiometricAuthenticationListener {
        void onCreateNewSession();
        void onAuthSuccess();
    }

    public void newSessionButtonClicked() {
        final BiometricAuthenticationListener listener = (BiometricAuthenticationListener) getTargetFragment();
        if (listener != null) {
            listener.onCreateNewSession();
        }
        dismiss();
    }

    public void displayError(String errorText) {
        // Display the error text
        mErrorTextView.setText(errorText);
        mErrorTextView.setTextColor(ContextCompat.getColor(mErrorTextView.getContext(), R.color.photonRed50));
    }

    public void onAuthenticated() {
        final BiometricAuthenticationListener listener = (BiometricAuthenticationListener) getTargetFragment();
        if (listener != null) {
            listener.onAuthSuccess();
            dismiss();
        }
    }

    public void onFailure() {
        displayError(getContext().getString(R.string.biometric_auth_not_recognized_error));
    }

    private void resetErrorText() {
        mErrorTextView.setText("");
    }
}
