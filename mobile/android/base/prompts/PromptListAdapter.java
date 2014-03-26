package org.mozilla.gecko.prompts;

import org.mozilla.gecko.menu.MenuItemActionView;
import org.mozilla.gecko.R;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.widget.GeckoActionProvider;

import org.json.JSONArray;
import org.json.JSONObject;
import org.json.JSONException;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckedTextView;
import android.widget.TextView;
import android.widget.ListView;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;
import android.util.TypedValue;

import java.util.ArrayList;

public class PromptListAdapter extends ArrayAdapter<PromptListItem> {
    private static final int VIEW_TYPE_ITEM = 0;
    private static final int VIEW_TYPE_GROUP = 1;
    private static final int VIEW_TYPE_ACTIONS = 2;
    private static final int VIEW_TYPE_COUNT = 3;

    private static final String LOGTAG = "GeckoPromptListAdapter";

    private final int mResourceId;
    private Drawable mBlankDrawable;
    private Drawable mMoreDrawable;
    private static int mGroupPaddingSize;
    private static int mLeftRightTextWithIconPadding;
    private static int mTopBottomTextWithIconPadding;
    private static int mIconSize;
    private static int mMinRowSize;
    private static int mIconTextPadding;
    private static float mTextSize;
    private static boolean mInitialized = false;

    PromptListAdapter(Context context, int textViewResourceId, PromptListItem[] objects) {
        super(context, textViewResourceId, objects);
        mResourceId = textViewResourceId;
        init();
    }

    private void init() {
        if (!mInitialized) {
            Resources res = getContext().getResources();
            mGroupPaddingSize = (int) (res.getDimension(R.dimen.prompt_service_group_padding_size));
            mLeftRightTextWithIconPadding = (int) (res.getDimension(R.dimen.prompt_service_left_right_text_with_icon_padding));
            mTopBottomTextWithIconPadding = (int) (res.getDimension(R.dimen.prompt_service_top_bottom_text_with_icon_padding));
            mIconTextPadding = (int) (res.getDimension(R.dimen.prompt_service_icon_text_padding));
            mIconSize = (int) (res.getDimension(R.dimen.prompt_service_icon_size));
            mMinRowSize = (int) (res.getDimension(R.dimen.menu_item_row_height));
            mTextSize = res.getDimension(R.dimen.menu_item_textsize);

            mInitialized = true;
        }
    }

    @Override
    public int getItemViewType(int position) {
        PromptListItem item = getItem(position);
        if (item.isGroup) {
            return VIEW_TYPE_GROUP;
        } else if (item.showAsActions) {
            return VIEW_TYPE_ACTIONS;
        } else {
            return VIEW_TYPE_ITEM;
        }
    }

    @Override
    public int getViewTypeCount() {
        return VIEW_TYPE_COUNT;
    }

    private Drawable getMoreDrawable(Resources res) {
        if (mMoreDrawable == null) {
            mMoreDrawable = res.getDrawable(R.drawable.menu_item_more);
        }
        return mMoreDrawable;
    }

    private Drawable getBlankDrawable(Resources res) {
        if (mBlankDrawable == null) {
            mBlankDrawable = res.getDrawable(R.drawable.blank);
        }
        return mBlankDrawable;
    }

    public void toggleSelected(int position) {
        PromptListItem item = getItem(position);
        item.setSelected(!item.getSelected());
    }

    private void maybeUpdateIcon(PromptListItem item, TextView t) {
        if (item.getIcon() == null && !item.inGroup && !item.isParent) {
            t.setCompoundDrawablesWithIntrinsicBounds(null, null, null, null);
            return;
        }

        Drawable d = null;
        Resources res = getContext().getResources();
        // Set the padding between the icon and the text.
        t.setCompoundDrawablePadding(mIconTextPadding);
        if (item.getIcon() != null) {
            // We want the icon to be of a specific size. Some do not
            // follow this rule so we have to resize them.
            Bitmap bitmap = ((BitmapDrawable) item.getIcon()).getBitmap();
            d = new BitmapDrawable(res, Bitmap.createScaledBitmap(bitmap, mIconSize, mIconSize, true));
        } else if (item.inGroup) {
            // We don't currently support "indenting" items with icons
            d = getBlankDrawable(res);
        }

        Drawable moreDrawable = null;
        if (item.isParent) {
            moreDrawable = getMoreDrawable(res);
        }

        if (d != null || moreDrawable != null) {
            t.setCompoundDrawablesWithIntrinsicBounds(d, null, moreDrawable, null);
        }
    }

    private void maybeUpdateCheckedState(ListView list, int position, PromptListItem item, ViewHolder viewHolder) {
        viewHolder.textView.setEnabled(!item.disabled && !item.isGroup);
        viewHolder.textView.setClickable(item.isGroup || item.disabled);
        if (viewHolder.textView instanceof CheckedTextView) {
            // Apparently just using ct.setChecked(true) doesn't work, so this
            // is stolen from the android source code as a way to set the checked
            // state of these items
            list.setItemChecked(position, item.getSelected());
        }
    }

    boolean isSelected(int position){
        return getItem(position).getSelected();
    }

    ArrayList<Integer> getSelected() {
        int length = getCount();

        ArrayList<Integer> selected = new ArrayList<Integer>();
        for (int i = 0; i< length; i++) {
            if (isSelected(i)) {
                selected.add(i);
            }
        }

        return selected;
    }

    int getSelectedIndex() {
        int length = getCount();
        for (int i = 0; i< length; i++) {
            if (isSelected(i)) {
                return i;
            }
        }
        return -1;
    }

    private View getActionView(PromptListItem item) {
        GeckoActionProvider provider = GeckoActionProvider.getForType(item.getIntent().getType(), getContext());
        provider.setIntent(item.getIntent());
        return provider.onCreateActionView();
    }

    private void updateActionView(final PromptListItem item, final MenuItemActionView view, final ListView list, final int position) {
        view.setTitle(item.label);
        view.setIcon(item.getIcon());
        view.setSubMenuIndicator(item.isParent);
        view.setMenuItemClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                ListView.OnItemClickListener listener = list.getOnItemClickListener();
                if (listener != null) {
                    listener.onItemClick(list, view, position, position);
                }

                final GeckoActionProvider provider = GeckoActionProvider.getForType(item.getIntent().getType(), getContext());
                IntentChooserPrompt prompt = new IntentChooserPrompt(getContext(), provider);
                prompt.show(item.label, getContext(), new IntentHandler() {
                    @Override
                    public void onIntentSelected(final Intent intent, final int p) {
                        provider.chooseActivity(p);
                    }

                    @Override
                    public void onCancelled() {
                        // do nothing
                    }
                });
            }
        });
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        PromptListItem item = getItem(position);
        int type = getItemViewType(position);
        ViewHolder viewHolder = null;

        if (convertView == null) {
            if (type == VIEW_TYPE_ACTIONS) {
                convertView = getActionView(item);
            } else {
                int resourceId = mResourceId;
                if (item.isGroup) {
                    resourceId = R.layout.list_item_header;
                }

                LayoutInflater mInflater = LayoutInflater.from(getContext());
                convertView = mInflater.inflate(resourceId, null);
                convertView.setMinimumHeight(mMinRowSize);

                TextView tv = (TextView) convertView.findViewById(android.R.id.text1);
                tv.setTextSize(TypedValue.COMPLEX_UNIT_PX, mTextSize);
                viewHolder = new ViewHolder(tv, tv.getPaddingLeft(), tv.getPaddingRight(),
                                            tv.getPaddingTop(), tv.getPaddingBottom());

                convertView.setTag(viewHolder);
            }
        } else {
            viewHolder = (ViewHolder) convertView.getTag();
        }

        if (type == VIEW_TYPE_ACTIONS) {
            updateActionView(item, (MenuItemActionView) convertView, (ListView) parent, position);
        } else {
            viewHolder.textView.setText(item.label);
            maybeUpdateCheckedState((ListView) parent, position, item, viewHolder);
            maybeUpdateIcon(item, viewHolder.textView);
        }

        return convertView;
    }

    private static class ViewHolder {
        public final TextView textView;
        public final int paddingLeft;
        public final int paddingRight;
        public final int paddingTop;
        public final int paddingBottom;

        ViewHolder(TextView aTextView, int aLeft, int aRight, int aTop, int aBottom) {
            textView = aTextView;
            paddingLeft = aLeft;
            paddingRight = aRight;
            paddingTop = aTop;
            paddingBottom = aBottom;
        }
    }
}
