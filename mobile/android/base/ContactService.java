/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map.Entry;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.ThreadUtils;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.app.AlertDialog;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.DialogInterface;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;
import android.provider.ContactsContract;
import android.provider.ContactsContract.CommonDataKinds.BaseTypes;
import android.provider.ContactsContract.CommonDataKinds.Email;
import android.provider.ContactsContract.CommonDataKinds.Event;
import android.provider.ContactsContract.CommonDataKinds.GroupMembership;
import android.provider.ContactsContract.CommonDataKinds.Im;
import android.provider.ContactsContract.CommonDataKinds.Nickname;
import android.provider.ContactsContract.CommonDataKinds.Note;
import android.provider.ContactsContract.CommonDataKinds.Organization;
import android.provider.ContactsContract.CommonDataKinds.Phone;
import android.provider.ContactsContract.CommonDataKinds.StructuredName;
import android.provider.ContactsContract.CommonDataKinds.StructuredPostal;
import android.provider.ContactsContract.CommonDataKinds.Website;
import android.provider.ContactsContract.Data;
import android.provider.ContactsContract.Groups;
import android.provider.ContactsContract.RawContacts;
import android.provider.ContactsContract.RawContacts.Entity;
import android.telephony.PhoneNumberUtils;
import android.util.Log;

public class ContactService implements GeckoEventListener {
    private static final String LOGTAG = "GeckoContactService";
    private static final boolean DEBUG = false;

    private final static int GROUP_ACCOUNT_NAME = 0;
    private final static int GROUP_ACCOUNT_TYPE = 1;
    private final static int GROUP_ID = 2;
    private final static int GROUP_TITLE = 3;
    private final static int GROUP_AUTO_ADD = 4;

    private final static String CARRIER_COLUMN = Data.DATA5;
    private final static String CUSTOM_DATA_COLUMN = Data.DATA1;

    // Pre-Honeycomb versions of Android have a "My Contacts" system group that all contacts are
    // assigned to by default for a given account. After Honeycomb, an AUTO_ADD database column
    // was added to denote groups that contacts are automatically added to
    private final static String PRE_HONEYCOMB_DEFAULT_GROUP = "System Group: My Contacts";
    private final static String MIMETYPE_ADDITIONAL_NAME = "org.mozilla.gecko/additional_name";
    private final static String MIMETYPE_SEX = "org.mozilla.gecko/sex";
    private final static String MIMETYPE_GENDER_IDENTITY = "org.mozilla.gecko/gender_identity";
    private final static String MIMETYPE_KEY = "org.mozilla.gecko/key";
    private final static String MIMETYPE_MOZILLA_CONTACTS_FLAG = "org.mozilla.gecko/contact_flag";

    private final EventDispatcher mEventDispatcher;

    private String mAccountName;
    private String mAccountType;
    private String mGroupTitle;
    private long mGroupId;
    private boolean mGotDeviceAccount;

    private HashMap<String, String> mColumnNameConstantsMap;
    private HashMap<String, String> mMimeTypeConstantsMap;
    private HashMap<String, Integer> mAddressTypesMap;
    private HashMap<String, Integer> mPhoneTypesMap;
    private HashMap<String, Integer> mEmailTypesMap;
    private HashMap<String, Integer> mWebsiteTypesMap;
    private HashMap<String, Integer> mImTypesMap;

    private final ContentResolver mContentResolver;
    private final GeckoApp mActivity;

    ContactService(EventDispatcher eventDispatcher, GeckoApp activity) {
        mEventDispatcher = eventDispatcher;
        mActivity = activity;
        mContentResolver = mActivity.getContentResolver();

        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Android:Contacts:Clear",
            "Android:Contacts:Find",
            "Android:Contacts:GetAll",
            "Android:Contacts:GetCount",
            "Android:Contact:Remove",
            "Android:Contact:Save");
    }

    public void destroy() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Android:Contacts:Clear",
            "Android:Contacts:Find",
            "Android:Contacts:GetAll",
            "Android:Contacts:GetCount",
            "Android:Contact:Remove",
            "Android:Contact:Save");
    }

    @Override
    public void handleMessage(final String event, final JSONObject message) {
        // If the account chooser dialog needs shown to the user, the message handling becomes
        // asychronous so it needs posted to a background thread from the UI thread when the
        // account chooser dialog is dismissed by the user.
        Runnable handleMessage = new Runnable() {
            @Override
            public void run() {
                try {
                    if (DEBUG) {
                        Log.d(LOGTAG, "Event: " + event + "\nMessage: " + message.toString(3));
                    }

                    final JSONObject messageData = message.getJSONObject("data");
                    final String requestID = messageData.getString("requestID");

                    // Options may not exist for all operations
                    JSONObject contactOptions = messageData.optJSONObject("options");

                    if ("Android:Contacts:Find".equals(event)) {
                        findContacts(contactOptions, requestID);
                    } else if ("Android:Contacts:GetAll".equals(event)) {
                        getAllContacts(messageData, requestID);
                    } else if ("Android:Contacts:Clear".equals(event)) {
                        clearAllContacts(contactOptions, requestID);
                    } else if ("Android:Contact:Save".equals(event)) {
                        saveContact(contactOptions, requestID);
                    } else if ("Android:Contact:Remove".equals(event)) {
                        removeContact(contactOptions, requestID);
                    } else if ("Android:Contacts:GetCount".equals(event)) {
                        getContactsCount(requestID);
                    } else {
                        throw new IllegalArgumentException("Unexpected event: " + event);
                    }
                } catch (JSONException e) {
                    throw new IllegalArgumentException("Message: " + e);
                }
            }
        };

        // Get the account name/type if they haven't been set yet
        if (!mGotDeviceAccount) {
            getDeviceAccount(handleMessage);
        } else {
            handleMessage.run();
        }
    }

    private void findContacts(final JSONObject contactOptions, final String requestID) {
        long[] rawContactIds = findContactsRawIds(contactOptions);
        Log.i(LOGTAG, "Got " + (rawContactIds != null ? rawContactIds.length : "null") + " raw contact IDs");

        final String[] sortOptions = getSortOptionsFromJSON(contactOptions);

        if (rawContactIds == null || sortOptions == null) {
            sendCallbackToJavascript("Android:Contacts:Find:Return:KO", requestID, null, null);
        } else {
            sendCallbackToJavascript("Android:Contacts:Find:Return:OK", requestID,
                                     new String[] {"contacts"},
                                     new Object[] {getContactsAsJSONArray(rawContactIds, sortOptions[0],
                                                                          sortOptions[1])});
        }
    }

    private void getAllContacts(final JSONObject contactOptions, final String requestID) {
        long[] rawContactIds = getAllRawContactIds();
        Log.i(LOGTAG, "Got " + rawContactIds.length + " raw contact IDs");

        final String[] sortOptions = getSortOptionsFromJSON(contactOptions);

        if (rawContactIds == null || sortOptions == null) {
            // There's no failure message for getAll
            return;
        } else {
            sendCallbackToJavascript("Android:Contacts:GetAll:Next", requestID,
                                     new String[] {"contacts"},
                                     new Object[] {getContactsAsJSONArray(rawContactIds, sortOptions[0],
                                                                          sortOptions[1])});
        }
    }

    private static String[] getSortOptionsFromJSON(final JSONObject contactOptions) {
        String sortBy = null;
        String sortOrder = null;

        try {
            final JSONObject findOptions = contactOptions.getJSONObject("findOptions");
            sortBy = findOptions.optString("sortBy").toLowerCase();
            sortOrder = findOptions.optString("sortOrder").toLowerCase();

            if ("".equals(sortBy)) {
                sortBy = null;
            }
            if ("".equals(sortOrder)) {
                sortOrder = "ascending";
            }

            // Only "familyname" and "givenname" are valid sortBy values and only "ascending"
            // and "descending" are valid sortOrder values
            if ((sortBy != null && !"familyname".equals(sortBy) && !"givenname".equals(sortBy)) ||
                (!"ascending".equals(sortOrder) && !"descending".equals(sortOrder))) {
                return null;
            }
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }

        return new String[] {sortBy, sortOrder};
    }

    private long[] findContactsRawIds(final JSONObject contactOptions) {
        List<Long> rawContactIds = new ArrayList<Long>();
        Cursor cursor = null;

        try {
            final JSONObject findOptions = contactOptions.getJSONObject("findOptions");
            String filterValue = findOptions.optString("filterValue");
            JSONArray filterBy = findOptions.optJSONArray("filterBy");
            final String filterOp = findOptions.optString("filterOp");
            final int filterLimit = findOptions.getInt("filterLimit");
            final int substringMatching = findOptions.getInt("substringMatching");

            // If filter value is undefined, avoid all the logic below and just return
            // all available raw contact IDs
            if ("".equals(filterValue) || "".equals(filterOp)) {
                long[] allRawContactIds = getAllRawContactIds();

                // Truncate the raw contacts IDs array if necessary
                if (filterLimit > 0 && allRawContactIds.length > filterLimit) {
                    long[] truncatedRawContactIds = new long[filterLimit];
                    System.arraycopy(allRawContactIds, 0, truncatedRawContactIds, 0, filterLimit);
                    return truncatedRawContactIds;
                }
                return allRawContactIds;
            }

            // "match" can only be used with the "tel" field
            if ("match".equals(filterOp)) {
                for (int i = 0; i < filterBy.length(); i++) {
                    if (!"tel".equals(filterBy.getString(i))) {
                        Log.w(LOGTAG, "\"match\" filterBy option is only valid for the \"tel\" field");
                        return null;
                    }
                }
            }

            // Only select contacts from the selected account
            String selection = null;
            String[] selectionArgs = null;

            if (mAccountName != null) {
                selection = RawContacts.ACCOUNT_NAME + "=? AND " + RawContacts.ACCOUNT_TYPE + "=?";
                selectionArgs = new String[] {mAccountName, mAccountType};
            }


            final String[] columnsToGet;

            // If a filterBy value was not specified, search all columns
            if (filterBy == null || filterBy.length() == 0) {
                columnsToGet = null;
            } else {
                // Only get the columns given in the filterBy array
                List<String> columnsToGetList = new ArrayList<String>();

                columnsToGetList.add(Data.RAW_CONTACT_ID);
                columnsToGetList.add(Data.MIMETYPE);
                for (int i = 0; i < filterBy.length(); i++) {
                    final String field = filterBy.getString(i);

                    // If one of the filterBy fields is the ID, just return the filter value
                    // which should be the ID
                    if ("id".equals(field)) {
                        try {
                            return new long[] {Long.valueOf(filterValue)};
                        } catch (NumberFormatException e) {
                            // If the ID couldn't be converted to a long, it's invalid data
                            // so return null for failure
                            return null;
                        }
                    }

                    final String columnName = getColumnNameConstant(field);

                    if (columnName != null) {
                        columnsToGetList.add(columnName);
                    } else {
                        Log.w(LOGTAG, "Unknown filter option: " + field);
                    }
                }

                columnsToGet = columnsToGetList.toArray(new String[columnsToGetList.size()]);
            }

            // Execute the query
            cursor = mContentResolver.query(Data.CONTENT_URI, columnsToGet, selection,
                                            selectionArgs, null);

            if (cursor.getCount() > 0) {
                cursor.moveToPosition(-1);
                while (cursor.moveToNext()) {
                    String mimeType = cursor.getString(cursor.getColumnIndex(Data.MIMETYPE));

                    // Check if the current mimetype is one of the types to filter by
                    if (filterBy != null && filterBy.length() > 0) {
                        for (int i = 0; i < filterBy.length(); i++) {
                            String currentFilterBy = filterBy.getString(i);

                            if (mimeType.equals(getMimeTypeOfField(currentFilterBy))) {
                                String columnName = getColumnNameConstant(currentFilterBy);
                                int columnIndex = cursor.getColumnIndex(columnName);
                                String databaseValue = cursor.getString(columnIndex);

                                boolean isPhone = false;
                                if (Phone.CONTENT_ITEM_TYPE.equals(mimeType)) {
                                    isPhone = true;
                                } else if (GroupMembership.CONTENT_ITEM_TYPE.equals(mimeType)) {
                                    // Translate the group ID to the group name for matching
                                    try {
                                        databaseValue = getGroupName(Long.valueOf(databaseValue));
                                    } catch (NumberFormatException e) {
                                        Log.e(LOGTAG, "Number Format Exception", e);
                                        continue;
                                    }
                                } else if (databaseValue == null) {
                                    continue;
                                }

                                // Check if the value matches the filter value
                                if (isFindMatch(filterOp, filterValue, databaseValue, isPhone, substringMatching)) {
                                    addMatchToList(cursor, rawContactIds);
                                    break;
                                }
                            }
                        }
                    } else {
                        // If no filterBy options were given, check each column for a match
                        int numColumns = cursor.getColumnCount();
                        for (int i = 0; i < numColumns; i++) {
                            String databaseValue = cursor.getString(i);
                            if (databaseValue != null && isFindMatch(filterOp, filterValue, databaseValue, false, substringMatching)) {
                                addMatchToList(cursor, rawContactIds);
                                break;
                            }
                        }
                    }

                    // If the max found contacts size has been hit, stop looking for contacts
                    // A filter limit of 0 denotes there is no limit
                    if (filterLimit > 0 && filterLimit <= rawContactIds.size()) {
                        break;
                    }
                }
            }
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        } finally {
            if (cursor != null) {
                cursor.close();
            }
        }

        // Return the contact IDs list converted to an array
        return convertLongListToArray(rawContactIds);
    }

    private boolean isFindMatch(final String filterOp, String filterValue, String databaseValue,
                                final boolean isPhone, final int substringMatching) {
        Log.i(LOGTAG, "matching: filterOp: " + filterOp);
        if (DEBUG) {
            Log.d(LOGTAG, "matching: filterValue: " + filterValue);
            Log.d(LOGTAG, "matching: databaseValue: " + databaseValue);
        }
        Log.i(LOGTAG, "matching: isPhone: " + isPhone);
        Log.i(LOGTAG, "matching: substringMatching: " + substringMatching);

        if (databaseValue == null) {
            return false;
        }

        filterValue = filterValue.toLowerCase();
        databaseValue = databaseValue.toLowerCase();

        switch (filterOp) {
            case "match":
                // If substring matching is a positive number, only pay attention to the last X characters
                // of both the filter and database values
                if (substringMatching > 0) {
                    databaseValue = substringStartFromEnd(cleanPhoneNumber(databaseValue), substringMatching);
                    filterValue = substringStartFromEnd(cleanPhoneNumber(filterValue), substringMatching);
                    return databaseValue.startsWith(filterValue);
                }

                return databaseValue.equals(filterValue);
            case "equals":
                if (isPhone) {
                    return PhoneNumberUtils.compare(filterValue, databaseValue);
                }

                return databaseValue.equals(filterValue);
            case "contains":
                if (isPhone) {
                    filterValue = cleanPhoneNumber(filterValue);
                    databaseValue = cleanPhoneNumber(databaseValue);
                }

                return databaseValue.contains(filterValue);
            case "startsWith":
                // If a phone number, remove non-dialable characters and then only pay attention to
                // the last X digits given by the substring matching values (see bug 877302)
                if (isPhone) {
                    String cleanedDatabasePhone = cleanPhoneNumber(databaseValue);
                    if (substringMatching > 0) {
                        cleanedDatabasePhone = substringStartFromEnd(cleanedDatabasePhone, substringMatching);
                    }

                    if (cleanedDatabasePhone.startsWith(filterValue)) {
                        return true;
                    }
                }

                return databaseValue.startsWith(filterValue);
        }

        return false;
    }

    private static String cleanPhoneNumber(String phone) {
        return phone.replace(" ", "").replace("(", "").replace(")", "").replace("-", "");
    }

    private static String substringStartFromEnd(final String string, final int distanceFromEnd) {
        int stringLen = string.length();
        if (stringLen < distanceFromEnd) {
            return string;
        }
        return string.substring(stringLen - distanceFromEnd);
    }

    private static void addMatchToList(final Cursor cursor, List<Long> rawContactIds) {
        long rawContactId = cursor.getLong(cursor.getColumnIndex(Data.RAW_CONTACT_ID));
        if (!rawContactIds.contains(rawContactId)) {
            rawContactIds.add(rawContactId);
        }
    }

    private JSONArray getContactsAsJSONArray(final long[] rawContactIds, final String sortBy, final String sortOrder) {
        List<JSONObject> contactsList = new ArrayList<JSONObject>();
        JSONArray contactsArray = new JSONArray();

        // Get each contact as a JSON object
        for (int i = 0; i < rawContactIds.length; i++) {
            contactsList.add(getContactAsJSONObject(rawContactIds[i]));
        }

        // Sort the contacts
        if (sortBy != null) {
            Collections.sort(contactsList, new ContactsComparator(sortBy, sortOrder));
        }

        // Convert the contacts list to a JSON array
        for (int i = 0; i < contactsList.size(); i++) {
            contactsArray.put(contactsList.get(i));
        }

        return contactsArray;
    }

    private JSONObject getContactAsJSONObject(long rawContactId) {
        // ContactManager wants a contact object with it's properties wrapped in an array of objects
        JSONObject contact = new JSONObject();
        JSONObject contactProperties = new JSONObject();

        JSONArray names = new JSONArray();
        JSONArray givenNames = new JSONArray();
        JSONArray familyNames = new JSONArray();
        JSONArray honorificPrefixes = new JSONArray();
        JSONArray honorificSuffixes = new JSONArray();
        JSONArray additionalNames = new JSONArray();
        JSONArray nicknames = new JSONArray();
        JSONArray addresses = new JSONArray();
        JSONArray phones = new JSONArray();
        JSONArray emails = new JSONArray();
        JSONArray organizations = new JSONArray();
        JSONArray jobTitles = new JSONArray();
        JSONArray notes = new JSONArray();
        JSONArray urls = new JSONArray();
        JSONArray impps = new JSONArray();
        JSONArray categories = new JSONArray();
        String bday = null;
        String anniversary = null;
        String sex = null;
        String genderIdentity = null;
        JSONArray key = new JSONArray();

        // Get all the data columns
        final String[] columnsToGet = getAllColumns();

        Uri rawContactUri = ContentUris.withAppendedId(RawContacts.CONTENT_URI, rawContactId);
        Uri entityUri = Uri.withAppendedPath(rawContactUri, Entity.CONTENT_DIRECTORY);

        Cursor cursor = mContentResolver.query(entityUri, columnsToGet, null, null, null);
        cursor.moveToPosition(-1);
        while (cursor.moveToNext()) {
            String mimeType = cursor.getString(cursor.getColumnIndex(Data.MIMETYPE));

            // Put the proper fields for each mimetype into the JSON arrays
            try {
                if (StructuredName.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    final String displayName = cursor.getString(cursor.getColumnIndex(StructuredName.DISPLAY_NAME));
                    final String givenName = cursor.getString(cursor.getColumnIndex(StructuredName.GIVEN_NAME));
                    final String familyName = cursor.getString(cursor.getColumnIndex(StructuredName.FAMILY_NAME));
                    final String prefix = cursor.getString(cursor.getColumnIndex(StructuredName.PREFIX));
                    final String suffix = cursor.getString(cursor.getColumnIndex(StructuredName.SUFFIX));

                    if (displayName != null) {
                        names.put(displayName);
                    }
                    if (givenName != null) {
                        givenNames.put(givenName);
                    }
                    if (familyName != null) {
                        familyNames.put(familyName);
                    }
                    if (prefix != null) {
                        honorificPrefixes.put(prefix);
                    }
                    if (suffix != null) {
                        honorificSuffixes.put(suffix);
                    }

                } else if (MIMETYPE_ADDITIONAL_NAME.equals(mimeType)) {
                    additionalNames.put(cursor.getString(cursor.getColumnIndex(CUSTOM_DATA_COLUMN)));

                } else if (Nickname.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    nicknames.put(cursor.getString(cursor.getColumnIndex(Nickname.NAME)));

                } else if (StructuredPostal.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    initAddressTypesMap();
                    getAddressDataAsJSONObject(cursor, addresses);

                } else if (Phone.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    initPhoneTypesMap();
                    getPhoneDataAsJSONObject(cursor, phones);

                } else if (Email.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    initEmailTypesMap();
                    getGenericDataAsJSONObject(cursor, emails, Email.ADDRESS, Email.TYPE, Email.LABEL, mEmailTypesMap);

                } else if (Organization.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    getOrganizationDataAsJSONObject(cursor, organizations, jobTitles);

                } else if (Note.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    notes.put(cursor.getString(cursor.getColumnIndex(Note.NOTE)));

                } else if (Website.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    initWebsiteTypesMap();
                    getGenericDataAsJSONObject(cursor, urls, Website.URL, Website.TYPE, Website.LABEL, mWebsiteTypesMap);

                } else if (Im.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    initImTypesMap();
                    getGenericDataAsJSONObject(cursor, impps, Im.DATA, Im.TYPE, Im.LABEL, mImTypesMap);

                } else if (GroupMembership.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    long groupId = cursor.getLong(cursor.getColumnIndex(GroupMembership.GROUP_ROW_ID));
                    String groupName = getGroupName(groupId);
                    if (!doesJSONArrayContainString(categories, groupName)) {
                        categories.put(groupName);
                    }

                } else if (Event.CONTENT_ITEM_TYPE.equals(mimeType)) {
                    int type = cursor.getInt(cursor.getColumnIndex(Event.TYPE));
                    String date = cursor.getString(cursor.getColumnIndex(Event.START_DATE));

                    // Add the time info onto the date so it correctly parses into a JS date object
                    date += "T00:00:00";

                    switch (type) {
                        case Event.TYPE_BIRTHDAY:
                            bday = date;
                            break;

                        case Event.TYPE_ANNIVERSARY:
                            anniversary = date;
                            break;
                    }

                } else if (MIMETYPE_SEX.equals(mimeType)) {
                    sex = cursor.getString(cursor.getColumnIndex(CUSTOM_DATA_COLUMN));

                } else if (MIMETYPE_GENDER_IDENTITY.equals(mimeType)) {
                    genderIdentity = cursor.getString(cursor.getColumnIndex(CUSTOM_DATA_COLUMN));

                } else if (MIMETYPE_KEY.equals(mimeType)) {
                    key.put(cursor.getString(cursor.getColumnIndex(CUSTOM_DATA_COLUMN)));
                }
            } catch (JSONException e) {
                throw new IllegalArgumentException(e);
            }
        }
        cursor.close();

        try {
            // Add the fields to the contact properties object
            contactProperties.put("name", names);
            contactProperties.put("givenName", givenNames);
            contactProperties.put("familyName", familyNames);
            contactProperties.put("honorificPrefix", honorificPrefixes);
            contactProperties.put("honorificSuffix", honorificSuffixes);
            contactProperties.put("additionalName", additionalNames);
            contactProperties.put("nickname", nicknames);
            contactProperties.put("adr", addresses);
            contactProperties.put("tel", phones);
            contactProperties.put("email", emails);
            contactProperties.put("org", organizations);
            contactProperties.put("jobTitle", jobTitles);
            contactProperties.put("note", notes);
            contactProperties.put("url", urls);
            contactProperties.put("impp", impps);
            contactProperties.put("category", categories);
            contactProperties.put("key", key);

            putPossibleNullValueInJSONObject("bday", bday, contactProperties);
            putPossibleNullValueInJSONObject("anniversary", anniversary, contactProperties);
            putPossibleNullValueInJSONObject("sex", sex, contactProperties);
            putPossibleNullValueInJSONObject("genderIdentity", genderIdentity, contactProperties);

            // Add the raw contact ID and the properties to the contact
            contact.put("id", String.valueOf(rawContactId));
            contact.put("updated", null);
            contact.put("published", null);
            contact.put("properties", contactProperties);
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }

        if (DEBUG) {
            try {
                Log.d(LOGTAG, "Got contact: " + contact.toString(3));
            } catch (JSONException e) {}
        }

        return contact;
    }

    private boolean bool(int integer) {
        return integer != 0;
    }

    private void getGenericDataAsJSONObject(Cursor cursor, JSONArray array, final String dataColumn,
                                            final String typeColumn, final String typeLabelColumn,
                                            final HashMap<String, Integer> typeMap) throws JSONException {
        String value = cursor.getString(cursor.getColumnIndex(dataColumn));
        int typeConstant = cursor.getInt(cursor.getColumnIndex(typeColumn));
        String type;
        if (typeConstant == BaseTypes.TYPE_CUSTOM) {
            type = cursor.getString(cursor.getColumnIndex(typeLabelColumn));
        } else {
            type = getKeyFromMapValue(typeMap, typeConstant);
        }

        // Since an object may have multiple types, it may have already been added,
        // but still needs the new type added
        boolean found = false;
        if (type != null) {
            for (int i = 0; i < array.length(); i++) {
                JSONObject object = array.getJSONObject(i);
                if (value.equals(object.getString("value"))) {
                    found = true;

                    JSONArray types = object.getJSONArray("type");
                    if (!doesJSONArrayContainString(types, type)) {
                        types.put(type);
                        break;
                    }
                }
            }
        }

        // If an existing object wasn't found, make a new one
        if (!found) {
            JSONObject object = new JSONObject();
            JSONArray types = new JSONArray();
            object.put("value", value);
            types.put(type);
            object.put("type", types);
            object.put("pref", bool(cursor.getInt(cursor.getColumnIndex(Data.IS_SUPER_PRIMARY))));

            array.put(object);
        }
    }

    private void getPhoneDataAsJSONObject(Cursor cursor, JSONArray phones) throws JSONException {
        String value = cursor.getString(cursor.getColumnIndex(Phone.NUMBER));
        int typeConstant = cursor.getInt(cursor.getColumnIndex(Phone.TYPE));
        String type;
        if (typeConstant == Phone.TYPE_CUSTOM) {
            type = cursor.getString(cursor.getColumnIndex(Phone.LABEL));
        } else {
            type = getKeyFromMapValue(mPhoneTypesMap, typeConstant);
        }

        // Since a phone may have multiple types, it may have already been added,
        // but still needs the new type added
        boolean found = false;
        if (type != null) {
            for (int i = 0; i < phones.length(); i++) {
                JSONObject phone = phones.getJSONObject(i);
                if (value.equals(phone.getString("value"))) {
                    found = true;

                    JSONArray types = phone.getJSONArray("type");
                    if (!doesJSONArrayContainString(types, type)) {
                        types.put(type);
                        break;
                    }
                }
            }
        }

        // If an existing phone wasn't found, make a new one
        if (!found) {
            JSONObject phone = new JSONObject();
            JSONArray types = new JSONArray();
            phone.put("value", value);
            phone.put("type", type);
            types.put(type);
            phone.put("type", types);
            phone.put("carrier", cursor.getString(cursor.getColumnIndex(CARRIER_COLUMN)));
            phone.put("pref", bool(cursor.getInt(cursor.getColumnIndex(Phone.IS_SUPER_PRIMARY))));

            phones.put(phone);
        }
    }

    private void getAddressDataAsJSONObject(Cursor cursor, JSONArray addresses) throws JSONException {
        String streetAddress = cursor.getString(cursor.getColumnIndex(StructuredPostal.STREET));
        String locality = cursor.getString(cursor.getColumnIndex(StructuredPostal.CITY));
        String region = cursor.getString(cursor.getColumnIndex(StructuredPostal.REGION));
        String postalCode = cursor.getString(cursor.getColumnIndex(StructuredPostal.POSTCODE));
        String countryName = cursor.getString(cursor.getColumnIndex(StructuredPostal.COUNTRY));
        int typeConstant = cursor.getInt(cursor.getColumnIndex(StructuredPostal.TYPE));
        String type;
        if (typeConstant == StructuredPostal.TYPE_CUSTOM) {
            type = cursor.getString(cursor.getColumnIndex(StructuredPostal.LABEL));
        } else {
            type = getKeyFromMapValue(mAddressTypesMap, typeConstant);
        }

        // Since an email may have multiple types, it may have already been added,
        // but still needs the new type added
        boolean found = false;
        if (type != null) {
            for (int i = 0; i < addresses.length(); i++) {
                JSONObject address = addresses.getJSONObject(i);
                if (streetAddress.equals(address.getString("streetAddress")) &&
                    locality.equals(address.getString("locality")) &&
                    region.equals(address.getString("region")) &&
                    countryName.equals(address.getString("countryName")) &&
                    postalCode.equals(address.getString("postalCode"))) {
                    found = true;

                    JSONArray types = address.getJSONArray("type");
                    if (!doesJSONArrayContainString(types, type)) {
                        types.put(type);
                        break;
                    }
                }
            }
        }

        // If an existing email wasn't found, make a new one
        if (!found) {
            JSONObject address = new JSONObject();
            JSONArray types = new JSONArray();
            address.put("streetAddress", streetAddress);
            address.put("locality", locality);
            address.put("region", region);
            address.put("countryName", countryName);
            address.put("postalCode", postalCode);
            types.put(type);
            address.put("type", types);
            address.put("pref", bool(cursor.getInt(cursor.getColumnIndex(StructuredPostal.IS_SUPER_PRIMARY))));

            addresses.put(address);
        }
    }

    private void getOrganizationDataAsJSONObject(Cursor cursor, JSONArray organizations,
                                                 JSONArray jobTitles) throws JSONException {
        int organizationColumnIndex = cursor.getColumnIndex(Organization.COMPANY);
        int titleColumnIndex = cursor.getColumnIndex(Organization.TITLE);

        if (!cursor.isNull(organizationColumnIndex)) {
            organizations.put(cursor.getString(organizationColumnIndex));
        }
        if (!cursor.isNull(titleColumnIndex)) {
            jobTitles.put(cursor.getString(titleColumnIndex));
        }
    }

    private class ContactsComparator implements Comparator<JSONObject> {
        final String mSortBy;
        final String mSortOrder;

        public ContactsComparator(final String sortBy, final String sortOrder) {
            mSortBy = sortBy.toLowerCase();
            mSortOrder = sortOrder.toLowerCase();
        }

        @Override
        public int compare(JSONObject left, JSONObject right) {
            // Determine if sorting by "family name, given name" or "given name, family name"
            boolean familyFirst = false;
            if ("familyname".equals(mSortBy)) {
                familyFirst = true;
            }

            JSONObject leftProperties;
            JSONObject rightProperties;
            try {
                leftProperties = left.getJSONObject("properties");
                rightProperties = right.getJSONObject("properties");
            } catch (JSONException e) {
                throw new IllegalArgumentException(e);
            }

            JSONArray leftFamilyNames = leftProperties.optJSONArray("familyName");
            JSONArray leftGivenNames = leftProperties.optJSONArray("givenName");
            JSONArray rightFamilyNames = rightProperties.optJSONArray("familyName");
            JSONArray rightGivenNames = rightProperties.optJSONArray("givenName");

            // If any of the name arrays didn't exist (are null), create empty arrays
            // to avoid doing a bunch of null checking below
            if (leftFamilyNames == null) {
                leftFamilyNames = new JSONArray();
            }
            if (leftGivenNames == null) {
                leftGivenNames = new JSONArray();
            }
            if (rightFamilyNames == null) {
                rightFamilyNames = new JSONArray();
            }
            if (rightGivenNames == null) {
                rightGivenNames = new JSONArray();
            }

            int maxArrayLength = max(leftFamilyNames.length(), leftGivenNames.length(),
                                     rightFamilyNames.length(), rightGivenNames.length());

            int index = 0;
            int compareResult;
            do {
                // Join together the given name and family name per the pattern above
                String leftName = "";
                String rightName = "";

                if (familyFirst) {
                    leftName = leftFamilyNames.optString(index, "") + leftGivenNames.optString(index, "");
                    rightName = rightFamilyNames.optString(index, "") + rightGivenNames.optString(index, "");
                } else {
                    leftName = leftGivenNames.optString(index, "") + leftFamilyNames.optString(index, "");
                    rightName = rightGivenNames.optString(index, "") + rightFamilyNames.optString(index, "");
                }

                index++;
                compareResult = leftName.compareTo(rightName);

            } while (compareResult == 0 && index < maxArrayLength);

            // If descending order, flip the result
            if (compareResult != 0 && "descending".equals(mSortOrder)) {
                compareResult = -compareResult;
            }

            return compareResult;
        }
    }

    private void clearAllContacts(final JSONObject contactOptions, final String requestID) {
        ArrayList<ContentProviderOperation> deleteOptions = new ArrayList<ContentProviderOperation>();

        // Delete all contacts from the selected account
        ContentProviderOperation.Builder deleteOptionsBuilder = ContentProviderOperation.newDelete(RawContacts.CONTENT_URI);
        if (mAccountName != null) {
            deleteOptionsBuilder.withSelection(RawContacts.ACCOUNT_NAME + "=?", new String[] {mAccountName})
                                .withSelection(RawContacts.ACCOUNT_TYPE + "=?", new String[] {mAccountType});
        }

        deleteOptions.add(deleteOptionsBuilder.build());

        // Clear the contacts
        String returnStatus = "KO";
        if (applyBatch(deleteOptions) != null) {
            returnStatus = "OK";
        }

        Log.i(LOGTAG, "Sending return status: " + returnStatus);

        sendCallbackToJavascript("Android:Contacts:Clear:Return:" + returnStatus, requestID,
                                 new String[] {"contactID"}, new Object[] {"undefined"});

    }

    private boolean deleteContact(String rawContactId) {
        ContentProviderOperation deleteOptions = ContentProviderOperation.newDelete(RawContacts.CONTENT_URI)
                                                 .withSelection(RawContacts._ID + "=?",
                                                 new String[] {rawContactId})
                                                 .build();

        ArrayList<ContentProviderOperation> deleteOptionsList = new ArrayList<ContentProviderOperation>();
        deleteOptionsList.add(deleteOptions);

        return checkForPositiveCountInResults(applyBatch(deleteOptionsList));
    }

    private void removeContact(final JSONObject contactOptions, final String requestID) {
        String rawContactId;
        try {
            rawContactId = contactOptions.getString("id");
            Log.i(LOGTAG, "Removing contact with ID: " + rawContactId);
        } catch (JSONException e) {
            // We can't continue without a raw contact ID
            sendCallbackToJavascript("Android:Contact:Remove:Return:KO", requestID, null, null);
            return;
        }

        String returnStatus = "KO";
        if(deleteContact(rawContactId)) {
            returnStatus = "OK";
        }

        sendCallbackToJavascript("Android:Contact:Remove:Return:" + returnStatus, requestID,
                                 new String[] {"contactID"}, new Object[] {rawContactId});
    }

    private void saveContact(final JSONObject contactOptions, final String requestID) {
        try {
            String reason = contactOptions.getString("reason");
            JSONObject contact = contactOptions.getJSONObject("contact");
            JSONObject contactProperties = contact.getJSONObject("properties");

            if ("update".equals(reason)) {
                updateContact(contactProperties, contact.getLong("id"), requestID);
            } else {
                insertContact(contactProperties, requestID);
            }
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }
    }

    private void insertContact(final JSONObject contactProperties, final String requestID) throws JSONException {
        ArrayList<ContentProviderOperation> newContactOptions = new ArrayList<ContentProviderOperation>();

        // Account to save the contact under
        newContactOptions.add(ContentProviderOperation.newInsert(RawContacts.CONTENT_URI)
                         .withValue(RawContacts.ACCOUNT_NAME, mAccountName)
                         .withValue(RawContacts.ACCOUNT_TYPE, mAccountType)
                         .build());

        List<ContentValues> newContactValues = getContactValues(contactProperties);

        for (ContentValues values : newContactValues) {
            newContactOptions.add(ContentProviderOperation.newInsert(Data.CONTENT_URI)
                                  .withValueBackReference(Data.RAW_CONTACT_ID, 0)
                                  .withValues(values)
                                  .build());
        }

        String returnStatus = "KO";
        Long newRawContactId = -1L;

        // Insert the contact!
        ContentProviderResult[] insertResults = applyBatch(newContactOptions);

        if (insertResults != null) {
            try {
                // Get the ID of the newly created contact
                newRawContactId = getRawContactIdFromContentProviderResults(insertResults);

                if (newRawContactId != null) {
                    returnStatus = "OK";
                }
            } catch (NumberFormatException e) {
                Log.e(LOGTAG, "NumberFormatException", e);
            }

            Log.i(LOGTAG, "Newly created contact ID: " + newRawContactId);
        }

        Log.i(LOGTAG, "Sending return status: " + returnStatus);

        sendCallbackToJavascript("Android:Contact:Save:Return:" + returnStatus, requestID,
                                 new String[] {"contactID", "reason"},
                                 new Object[] {newRawContactId, "create"});
    }

    private void updateContact(final JSONObject contactProperties, final long rawContactId, final String requestID) throws JSONException {
        // Why is updating a contact so weird and horribly inefficient? Because Android doesn't
        // like multiple values for contact fields, but the Mozilla contacts API calls for this.
        // This means the Android update function is essentially completely useless. Why not just
        // delete the contact and re-insert it? Because that would change the contact ID and the
        // Mozilla contacts API shouldn't have this behavior. The solution is to delete each
        // row from the contacts data table that belongs to the contact, and insert the new
        // fields. But then why not just delete all the data from the data in one go and
        // insert the new data in another? Because if all the data relating to a contact is
        // deleted, Android will "conviently" remove the ID making it impossible to insert data
        // under the old ID. To work around this, we put a Mozilla contact flag in the database

        ContentProviderOperation removeOptions = ContentProviderOperation.newDelete(Data.CONTENT_URI)
                                                 .withSelection(Data.RAW_CONTACT_ID + "=? AND " +
                                                 Data.MIMETYPE + " != '" + MIMETYPE_MOZILLA_CONTACTS_FLAG + "'",
                                                 new String[] {String.valueOf(rawContactId)})
                                                 .build();

        ArrayList<ContentProviderOperation> removeOptionsList = new ArrayList<ContentProviderOperation>();
        removeOptionsList.add(removeOptions);

        ContentProviderResult[] removeResults = applyBatch(removeOptionsList);

        // Check if the remove failed
        if (removeResults == null || !checkForPositiveCountInResults(removeResults)) {
            Log.w(LOGTAG, "Null or 0 remove results");

            sendCallbackToJavascript("Android:Contact:Save:Return:KO", requestID, null, null);
            return;
        }

        List<ContentValues> updateContactValues = getContactValues(contactProperties);
        ArrayList<ContentProviderOperation> updateContactOptions = new ArrayList<ContentProviderOperation>();

        for (ContentValues values : updateContactValues) {
            updateContactOptions.add(ContentProviderOperation.newInsert(Data.CONTENT_URI)
                                  .withValue(Data.RAW_CONTACT_ID, rawContactId)
                                  .withValues(values)
                                  .build());
        }

        String returnStatus = "KO";

        // Update the contact!
        applyBatch(updateContactOptions);

        sendCallbackToJavascript("Android:Contact:Save:Return:OK", requestID,
                                 new String[] {"contactID", "reason"},
                                 new Object[] {rawContactId, "update"});
    }

    private List<ContentValues> getContactValues(final JSONObject contactProperties) throws JSONException {
        List<ContentValues> contactValues = new ArrayList<ContentValues>();

        // Add the contact to the default group so it is shown in other apps
        // like the Contacts or People app
        ContentValues defaultGroupValues = new ContentValues();
        defaultGroupValues.put(Data.MIMETYPE, GroupMembership.CONTENT_ITEM_TYPE);
        defaultGroupValues.put(GroupMembership.GROUP_ROW_ID, mGroupId);
        contactValues.add(defaultGroupValues);

        // Create all the values that will be inserted into the new contact
        getNameValues(contactProperties.optJSONArray("name"),
                      contactProperties.optJSONArray("givenName"),
                      contactProperties.optJSONArray("familyName"),
                      contactProperties.optJSONArray("honorificPrefix"),
                      contactProperties.optJSONArray("honorificSuffix"),
                      contactValues);

        getGenericValues(MIMETYPE_ADDITIONAL_NAME, CUSTOM_DATA_COLUMN,
                         contactProperties.optJSONArray("additionalName"), contactValues);

        getNicknamesValues(contactProperties.optJSONArray("nickname"), contactValues);

        getAddressesValues(contactProperties.optJSONArray("adr"), contactValues);

        getPhonesValues(contactProperties.optJSONArray("tel"), contactValues);

        getEmailsValues(contactProperties.optJSONArray("email"), contactValues);

        //getPhotosValues(contactProperties.optJSONArray("photo"), contactValues);

        getGenericValues(Organization.CONTENT_ITEM_TYPE, Organization.COMPANY,
                         contactProperties.optJSONArray("org"), contactValues);

        getGenericValues(Organization.CONTENT_ITEM_TYPE, Organization.TITLE,
                         contactProperties.optJSONArray("jobTitle"), contactValues);

        getNotesValues(contactProperties.optJSONArray("note"), contactValues);

        getWebsitesValues(contactProperties.optJSONArray("url"), contactValues);

        getImsValues(contactProperties.optJSONArray("impp"), contactValues);

        getCategoriesValues(contactProperties.optJSONArray("category"), contactValues);

        getEventValues(contactProperties.optString("bday"), Event.TYPE_BIRTHDAY, contactValues);

        getEventValues(contactProperties.optString("anniversary"), Event.TYPE_ANNIVERSARY, contactValues);

        getCustomMimetypeValues(contactProperties.optString("sex"), MIMETYPE_SEX, contactValues);

        getCustomMimetypeValues(contactProperties.optString("genderIdentity"), MIMETYPE_GENDER_IDENTITY, contactValues);

        getGenericValues(MIMETYPE_KEY, CUSTOM_DATA_COLUMN, contactProperties.optJSONArray("key"),
                         contactValues);

        return contactValues;
    }

    private void getGenericValues(final String mimeType, final String dataType, final JSONArray fields,
                                  List<ContentValues> newContactValues) throws JSONException {
        if (fields == null) {
            return;
        }

        for (int i = 0; i < fields.length(); i++) {
            ContentValues contentValues = new ContentValues();
            contentValues.put(Data.MIMETYPE, mimeType);
            contentValues.put(dataType, fields.getString(i));
            newContactValues.add(contentValues);
        }
    }

    private void getNameValues(final JSONArray displayNames, final JSONArray givenNames,
                               final JSONArray familyNames, final JSONArray prefixes,
                               final JSONArray suffixes, List<ContentValues> newContactValues) throws JSONException {
        int maxLen = max((displayNames != null ? displayNames.length() : 0),
                         (givenNames != null ? givenNames.length() : 0),
                         (familyNames != null ? familyNames.length() : 0),
                         (prefixes != null ? prefixes.length() : 0),
                         (suffixes != null ? suffixes.length() : 0));

        for (int i = 0; i < maxLen; i++) {
            ContentValues contentValues = new ContentValues();
            contentValues.put(Data.MIMETYPE, StructuredName.CONTENT_ITEM_TYPE);

            final String displayName = (displayNames != null ? displayNames.optString(i, null) : null);
            final String givenName = (givenNames != null ? givenNames.optString(i, null) : null);
            final String familyName = (familyNames != null ? familyNames.optString(i, null) : null);
            final String prefix = (prefixes != null ? prefixes.optString(i, null) : null);
            final String suffix = (suffixes != null ? suffixes.optString(i, null) : null);

            if (displayName != null) {
                contentValues.put(StructuredName.DISPLAY_NAME, displayName);
            }
            if (givenName != null) {
                contentValues.put(StructuredName.GIVEN_NAME, givenName);
            }
            if (familyName != null) {
                contentValues.put(StructuredName.FAMILY_NAME, familyName);
            }
            if (prefix != null) {
                contentValues.put(StructuredName.PREFIX, prefix);
            }
            if (suffix != null) {
                contentValues.put(StructuredName.SUFFIX, suffix);
            }

            newContactValues.add(contentValues);
        }
    }

    private void getNicknamesValues(final JSONArray nicknames, List<ContentValues> newContactValues) throws JSONException {
        if (nicknames == null) {
            return;
        }

        for (int i = 0; i < nicknames.length(); i++) {
            ContentValues contentValues = new ContentValues();
            contentValues.put(Data.MIMETYPE, Nickname.CONTENT_ITEM_TYPE);
            contentValues.put(Nickname.NAME, nicknames.getString(i));
            contentValues.put(Nickname.TYPE, Nickname.TYPE_DEFAULT);
            newContactValues.add(contentValues);
        }
    }

    private void getAddressesValues(final JSONArray addresses, List<ContentValues> newContactValues) throws JSONException {
        if (addresses == null) {
            return;
        }

        for (int i = 0; i < addresses.length(); i++) {
            JSONObject address = addresses.getJSONObject(i);
            JSONArray addressTypes = address.optJSONArray("type");

            if (addressTypes != null) {
                for (int j = 0; j < addressTypes.length(); j++) {
                    // Translate the address type string to an integer constant
                    // provided by the ContactsContract API
                    final String type = addressTypes.getString(j);
                    final int typeConstant = getAddressType(type);

                    newContactValues.add(createAddressContentValues(address, typeConstant, type));
                }
            } else {
                newContactValues.add(createAddressContentValues(address, -1, null));
            }
        }
    }

    private ContentValues createAddressContentValues(final JSONObject address, final int typeConstant,
                                                     final String type) throws JSONException {
        ContentValues contentValues = new ContentValues();
        contentValues.put(Data.MIMETYPE, StructuredPostal.CONTENT_ITEM_TYPE);
        contentValues.put(StructuredPostal.STREET, address.optString("streetAddress"));
        contentValues.put(StructuredPostal.CITY, address.optString("locality"));
        contentValues.put(StructuredPostal.REGION, address.optString("region"));
        contentValues.put(StructuredPostal.POSTCODE, address.optString("postalCode"));
        contentValues.put(StructuredPostal.COUNTRY, address.optString("countryName"));

        if (type != null) {
            contentValues.put(StructuredPostal.TYPE, typeConstant);

            // If a custom type, add a label
            if (typeConstant == BaseTypes.TYPE_CUSTOM) {
                contentValues.put(StructuredPostal.LABEL, type);
            }
        }

        if (address.has("pref")) {
            contentValues.put(Data.IS_SUPER_PRIMARY, address.getBoolean("pref") ? 1 : 0);
        }

        return contentValues;
    }

    private void getPhonesValues(final JSONArray phones, List<ContentValues> newContactValues) throws JSONException {
        if (phones == null) {
            return;
        }

        for (int i = 0; i < phones.length(); i++) {
            JSONObject phone = phones.getJSONObject(i);
            JSONArray phoneTypes = phone.optJSONArray("type");
            ContentValues contentValues;

            if (phoneTypes != null && phoneTypes.length() > 0) {
                for (int j = 0; j < phoneTypes.length(); j++) {
                    // Translate the phone type string to an integer constant
                    // provided by the ContactsContract API
                    final String type = phoneTypes.getString(j);
                    final int typeConstant = getPhoneType(type);

                    contentValues = createContentValues(Phone.CONTENT_ITEM_TYPE, phone.optString("value"),
                                                        typeConstant, type, phone.optBoolean("pref"));
                    if (phone.has("carrier")) {
                        contentValues.put(CARRIER_COLUMN, phone.optString("carrier"));
                    }
                    newContactValues.add(contentValues);
                }
            } else {
                contentValues = createContentValues(Phone.CONTENT_ITEM_TYPE, phone.optString("value"),
                                                    -1, null, phone.optBoolean("pref"));
                if (phone.has("carrier")) {
                    contentValues.put(CARRIER_COLUMN, phone.optString("carrier"));
                }
                newContactValues.add(contentValues);
            }
        }
    }

    private void getEmailsValues(final JSONArray emails, List<ContentValues> newContactValues) throws JSONException {
        if (emails == null) {
            return;
        }

        for (int i = 0; i < emails.length(); i++) {
            JSONObject email = emails.getJSONObject(i);
            JSONArray emailTypes = email.optJSONArray("type");

            if (emailTypes != null && emailTypes.length() > 0) {
                for (int j = 0; j < emailTypes.length(); j++) {
                    // Translate the email type string to an integer constant
                    // provided by the ContactsContract API
                    final String type = emailTypes.getString(j);
                    final int typeConstant = getEmailType(type);

                    newContactValues.add(createContentValues(Email.CONTENT_ITEM_TYPE,
                                                             email.optString("value"),
                                                             typeConstant, type,
                                                             email.optBoolean("pref")));
                }
            } else {
                newContactValues.add(createContentValues(Email.CONTENT_ITEM_TYPE,
                                                         email.optString("value"),
                                                         -1, null, email.optBoolean("pref")));
            }
        }
    }

    private void getPhotosValues(final JSONArray photos, List<ContentValues> newContactValues) throws JSONException {
        if (photos == null) {
            return;
        }

        // TODO: implement this
    }

    private void getNotesValues(final JSONArray notes, List<ContentValues> newContactValues) throws JSONException {
        if (notes == null) {
            return;
        }

        for (int i = 0; i < notes.length(); i++) {
            ContentValues contentValues = new ContentValues();
            contentValues.put(Data.MIMETYPE, Note.CONTENT_ITEM_TYPE);
            contentValues.put(Note.NOTE, notes.getString(i));
            newContactValues.add(contentValues);
        }
    }

    private void getWebsitesValues(final JSONArray websites, List<ContentValues> newContactValues) throws JSONException {
        if (websites == null) {
            return;
        }

        for (int i = 0; i < websites.length(); i++) {
            JSONObject website = websites.getJSONObject(i);
            JSONArray websiteTypes = website.optJSONArray("type");

            if (websiteTypes != null && websiteTypes.length() > 0) {
                for (int j = 0; j < websiteTypes.length(); j++) {
                    // Translate the website type string to an integer constant
                    // provided by the ContactsContract API
                    final String type = websiteTypes.getString(j);
                    final int typeConstant = getWebsiteType(type);

                    newContactValues.add(createContentValues(Website.CONTENT_ITEM_TYPE,
                                                             website.optString("value"),
                                                             typeConstant, type,
                                                             website.optBoolean("pref")));
                }
            } else {
                newContactValues.add(createContentValues(Website.CONTENT_ITEM_TYPE,
                                                         website.optString("value"),
                                                         -1, null, website.optBoolean("pref")));
            }
        }
    }

    private void getImsValues(final JSONArray ims, List<ContentValues> newContactValues) throws JSONException {
        if (ims == null) {
            return;
        }

        for (int i = 0; i < ims.length(); i++) {
            JSONObject im = ims.getJSONObject(i);
            JSONArray imTypes = im.optJSONArray("type");

            if (imTypes != null && imTypes.length() > 0) {
                for (int j = 0; j < imTypes.length(); j++) {
                    // Translate the IM type string to an integer constant
                    // provided by the ContactsContract API
                    final String type = imTypes.getString(j);
                    final int typeConstant = getImType(type);

                    newContactValues.add(createContentValues(Im.CONTENT_ITEM_TYPE,
                                                             im.optString("value"),
                                                             typeConstant, type,
                                                             im.optBoolean("pref")));
                }
            } else {
                newContactValues.add(createContentValues(Im.CONTENT_ITEM_TYPE,
                                                         im.optString("value"),
                                                         -1, null, im.optBoolean("pref")));
            }
        }
    }

    private void getCategoriesValues(final JSONArray categories, List<ContentValues> newContactValues) throws JSONException {
        if (categories == null) {
            return;
        }

        for (int i = 0; i < categories.length(); i++) {
            String category = categories.getString(i);

            if ("my contacts".equals(category.toLowerCase()) ||
                PRE_HONEYCOMB_DEFAULT_GROUP.equalsIgnoreCase(category)) {
                Log.w(LOGTAG, "New contacts are implicitly added to the default group.");
                continue;
            }

            // Find the group ID of the given category
            long groupId = getGroupId(category);

            // Create the group if it doesn't already exist
            if (groupId == -1) {
                groupId = createGroup(category);
                // If the group is still -1, we failed to create the group
                if (groupId == -1) {
                    // Only log the category name if in debug
                    if (DEBUG) {
                        Log.d(LOGTAG, "Failed to create new group for category \"" + category + "\"");
                    } else {
                        Log.w(LOGTAG, "Failed to create new group for given category.");
                    }
                    continue;
                }
            }

            ContentValues contentValues = new ContentValues();
            contentValues.put(Data.MIMETYPE, GroupMembership.CONTENT_ITEM_TYPE);
            contentValues.put(GroupMembership.GROUP_ROW_ID, groupId);
            newContactValues.add(contentValues);

            newContactValues.add(contentValues);
        }
    }

    private void getEventValues(final String event, final int type, List<ContentValues> newContactValues) {
        if (event == null || event.length() < 11) {
            return;
        }

        ContentValues contentValues = new ContentValues();
        contentValues.put(Data.MIMETYPE, Event.CONTENT_ITEM_TYPE);
        contentValues.put(Event.START_DATE, event.substring(0, 10));
        contentValues.put(Event.TYPE, type);
        newContactValues.add(contentValues);
    }

    private void getCustomMimetypeValues(final String value, final String mimeType, List<ContentValues> newContactValues) {
        if (value == null || "null".equals(value)) {
            return;
        }

        ContentValues contentValues = new ContentValues();
        contentValues.put(Data.MIMETYPE, mimeType);
        contentValues.put(CUSTOM_DATA_COLUMN, value);
        newContactValues.add(contentValues);
    }

    private void getMozillaContactFlagValues(List<ContentValues> newContactValues) {
        try {
            JSONArray mozillaContactsFlag = new JSONArray();
            mozillaContactsFlag.put("1");
            getGenericValues(MIMETYPE_MOZILLA_CONTACTS_FLAG, CUSTOM_DATA_COLUMN, mozillaContactsFlag, newContactValues);
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }
    }

    private ContentValues createContentValues(final String mimeType, final String value, final int typeConstant,
                                              final String type, final boolean preferredValue) {
        ContentValues contentValues = new ContentValues();
        contentValues.put(Data.MIMETYPE, mimeType);
        contentValues.put(Data.DATA1, value);
        contentValues.put(Data.IS_SUPER_PRIMARY, preferredValue ? 1 : 0);

        if (type != null) {
            contentValues.put(Data.DATA2, typeConstant);

            // If a custom type, add a label
            if (typeConstant == BaseTypes.TYPE_CUSTOM) {
                contentValues.put(Data.DATA3, type);
            }
        }

        return contentValues;
    }

    private void getContactsCount(final String requestID) {
        Cursor cursor = getAllRawContactIdsCursor();
        Integer numContacts = cursor.getCount();
        cursor.close();

        sendCallbackToJavascript("Android:Contacts:Count", requestID, new String[] {"count"},
                                 new Object[] {numContacts});
    }

    private void sendCallbackToJavascript(final String subject, final String requestID,
                                          final String[] argNames, final Object[] argValues) {
        // Check the same number of argument names and arguments were given
        if (argNames != null && argNames.length != argValues.length) {
            throw new IllegalArgumentException("Argument names and argument values lengths do not match. " +
                                               "Names length = " + argNames.length + ", Values length = " +
                                               argValues.length);
        }

        try {
            JSONObject callbackMessage = new JSONObject();
            callbackMessage.put("requestID", requestID);

            if (argNames != null) {
                for (int i = 0; i < argNames.length; i++) {
                    callbackMessage.put(argNames[i], argValues[i]);
                }
            }

            GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent(subject, callbackMessage.toString()));
        } catch (JSONException e) {
            throw new IllegalArgumentException(e);
        }
    }

    private ContentProviderResult[] applyBatch(ArrayList<ContentProviderOperation> operations) {
        try {
            return mContentResolver.applyBatch(ContactsContract.AUTHORITY, operations);
        } catch (RemoteException e) {
            Log.e(LOGTAG, "RemoteException", e);
        } catch (OperationApplicationException e) {
            Log.e(LOGTAG, "OperationApplicationException", e);
        }
        return null;
    }

    private void getDeviceAccount(final Runnable handleMessage) {
        Account[] accounts = AccountManager.get(mActivity).getAccounts();

        if (accounts.length == 0) {
            Log.w(LOGTAG, "No accounts available");
            gotDeviceAccount(handleMessage);
        } else if (accounts.length > 1) {
            // Show the accounts chooser dialog if more than one dialog exists
            showAccountsDialog(accounts, handleMessage);
        } else {
            // If only one account exists, use it
            mAccountName = accounts[0].name;
            mAccountType = accounts[0].type;
            gotDeviceAccount(handleMessage);
        }

        mGotDeviceAccount = true;
    }

    private void showAccountsDialog(final Account[] accounts, final Runnable handleMessage) {
        String[] accountNames = new String[accounts.length];
        for (int i = 0; i < accounts.length; i++) {
            accountNames[i] = accounts[i].name;
        }

        final AlertDialog.Builder builder = new AlertDialog.Builder(mActivity);
        builder.setTitle(mActivity.getResources().getString(R.string.contacts_account_chooser_dialog_title2))
            .setSingleChoiceItems(accountNames, 0, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int position) {
                    // Set the account name and type when an item is selected and dismiss the dialog
                    mAccountName = accounts[position].name;
                    mAccountType = accounts[position].type;
                    dialog.dismiss();
                    gotDeviceAccount(handleMessage);
                }
            });

        mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                builder.show();
            }
        });
    }

    private void gotDeviceAccount(final Runnable handleMessage) {
        // Force the handleMessage runnable and getDefaultGroupId to run on the background thread
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                getDefaultGroupId();

                // Don't log a user's account if not debug mode. Otherwise, just log a message
                // saying that we got an account to use
                if (mAccountName == null) {
                    Log.i(LOGTAG, "No device account selected. Leaving account as null.");
                } else if (DEBUG) {
                    Log.d(LOGTAG, "Using account: " + mAccountName + " (type: " + mAccountType + ")");
                } else {
                    Log.i(LOGTAG, "Got device account to use for contact operations.");
                }
                handleMessage.run();
            }
        };

        ThreadUtils.postToBackgroundThread(runnable);
    }

    private void getDefaultGroupId() {
        Cursor cursor = getAllGroups();

        cursor.moveToPosition(-1);
        while (cursor.moveToNext()) {
            // Check if the account name and type for the group match the account name and type of
            // the account we're working with
            final String groupAccountName = cursor.getString(GROUP_ACCOUNT_NAME);
            if (!groupAccountName.equals(mAccountName)) {
                continue;
            }

            final String groupAccountType = cursor.getString(GROUP_ACCOUNT_TYPE);
            if (!groupAccountType.equals(mAccountType)) {
                continue;
            }

            // For all honeycomb and up, the default group is the first one which has the AUTO_ADD flag set
            if (isAutoAddGroup(cursor)) {
                mGroupTitle = cursor.getString(GROUP_TITLE);
                mGroupId = cursor.getLong(GROUP_ID);
                break;
            } else if (PRE_HONEYCOMB_DEFAULT_GROUP.equals(cursor.getString(GROUP_TITLE))) {
                mGroupId = cursor.getLong(GROUP_ID);
                mGroupTitle = PRE_HONEYCOMB_DEFAULT_GROUP;
                break;
            }
        }
        cursor.close();

        if (mGroupId == 0) {
            Log.w(LOGTAG, "Default group ID not found. Newly created contacts will not belong to any groups.");
        } else if (DEBUG) {
            Log.i(LOGTAG, "Using group ID: " + mGroupId + " (" + mGroupTitle + ")");
        }
    }

    private static boolean isAutoAddGroup(Cursor cursor) {
        // For Honeycomb and up, the default group is the first one which has the AUTO_ADD flag set.
        // For everything below Honeycomb, use the default "System Group: My Contacts" group
        return (Versions.feature11Plus &&
                !cursor.isNull(GROUP_AUTO_ADD) &&
                cursor.getInt(GROUP_AUTO_ADD) != 0);
    }

    private long getGroupId(String groupName) {
        long groupId = -1;
        Cursor cursor = getGroups(Groups.TITLE + " = '" + groupName + "'");

        cursor.moveToPosition(-1);
        while (cursor.moveToNext()) {
            String groupAccountName = cursor.getString(GROUP_ACCOUNT_NAME);
            String groupAccountType = cursor.getString(GROUP_ACCOUNT_TYPE);

            // Check if the account name and type for the group match the account name and type of
            // the account we're working with or the default "Phone" account if no account was found
            if (groupAccountName.equals(mAccountName) && groupAccountType.equals(mAccountType) ||
                (mAccountName == null && "Phone".equals(groupAccountType))) {
                if (groupName.equals(cursor.getString(GROUP_TITLE))) {
                    groupId = cursor.getLong(GROUP_ID);
                    break;
                }
            }
        }
        cursor.close();

        return groupId;
    }

    private String getGroupName(long groupId) {
        Cursor cursor = getGroups(Groups._ID + " = " + groupId);

        if (cursor.getCount() == 0) {
            cursor.close();
            return null;
        }

        cursor.moveToPosition(0);
        String groupName = cursor.getString(cursor.getColumnIndex(Groups.TITLE));
        cursor.close();

        return groupName;
    }

    private Cursor getAllGroups() {
        return getGroups(null);
    }

    private Cursor getGroups(String selectArg) {
        String[] columns = new String[] {
            Groups.ACCOUNT_NAME,
            Groups.ACCOUNT_TYPE,
            Groups._ID,
            Groups.TITLE,
            (Versions.feature11Plus ? Groups.AUTO_ADD : Groups._ID)
        };

        if (selectArg != null) {
            selectArg = "AND " + selectArg;
        } else {
            selectArg = "";
        }

        return mContentResolver.query(Groups.CONTENT_URI, columns,
                                      Groups.ACCOUNT_TYPE + " NOT NULL AND " +
                                      Groups.ACCOUNT_NAME + " NOT NULL " + selectArg, null, null);
    }

    private long createGroup(String groupName) {
        if (DEBUG) {
            Log.d(LOGTAG, "Creating group: " + groupName);
        }

        ArrayList<ContentProviderOperation> newGroupOptions = new ArrayList<ContentProviderOperation>();

        // Create the group under the account we're using
        // If no account is selected, use a default account name/type for the group
        newGroupOptions.add(ContentProviderOperation.newInsert(Groups.CONTENT_URI)
                                .withValue(Groups.ACCOUNT_NAME, (mAccountName == null ? "Phone" : mAccountName))
                                .withValue(Groups.ACCOUNT_TYPE, (mAccountType == null ? "Phone" : mAccountType))
                                .withValue(Groups.TITLE, groupName)
                                .withValue(Groups.GROUP_VISIBLE, true)
                                .build());

        applyBatch(newGroupOptions);

        // Return the ID of the newly created group
        return getGroupId(groupName);
    }

    private long[] getAllRawContactIds() {
        Cursor cursor = getAllRawContactIdsCursor();

        // Put the ids into an array
        long[] ids = new long[cursor.getCount()];
        int index = 0;
        cursor.moveToPosition(-1);
        while(cursor.moveToNext()) {
            ids[index] = cursor.getLong(cursor.getColumnIndex(RawContacts._ID));
            index++;
        }
        cursor.close();

        return ids;
    }

    private Cursor getAllRawContactIdsCursor() {
        // When a contact is deleted, it actually just sets the deleted field to 1 until the
        // sync adapter actually deletes the contact later so ignore any contacts with the deleted
        // flag set
        String selection = RawContacts.DELETED + "=0";
        String[] selectionArgs = null;

        // Only get contacts from the selected account
        if (mAccountName != null) {
            selection += " AND " + RawContacts.ACCOUNT_NAME + "=? AND " + RawContacts.ACCOUNT_TYPE + "=?";
            selectionArgs = new String[] {mAccountName, mAccountType};
        }

        // Get the ID's of all contacts and use the number of contact ID's as
        // the total number of contacts
        return mContentResolver.query(RawContacts.CONTENT_URI, new String[] {RawContacts._ID},
                                      selection, selectionArgs, null);
    }

    private static Long getRawContactIdFromContentProviderResults(ContentProviderResult[] results) throws NumberFormatException {
        for (int i = 0; i < results.length; i++) {
            if (results[i].uri == null) {
                continue;
            }

            String uri = results[i].uri.toString();
            // Check if the uri is from the raw contacts table
            if (uri.contains("raw_contacts")) {
                // The ID is the after the final forward slash in the URI
                return Long.parseLong(uri.substring(uri.lastIndexOf("/") + 1));
            }
        }

        return null;
    }

    private static boolean checkForPositiveCountInResults(ContentProviderResult[] results) {
        for (int i = 0; i < results.length; i++) {
            Integer count = results[i].count;

            if (DEBUG) {
                Log.d(LOGTAG, "Results count: " + count);
            }

            if (count != null && count > 0) {
                return true;
            }
        }

        return false;
    }

    private static long[] convertLongListToArray(List<Long> list) {
        long[] array = new long[list.size()];

        for (int i = 0; i < list.size(); i++) {
            array[i] = list.get(i);
        }

        return array;
    }

    private static boolean doesJSONArrayContainString(final JSONArray array, final String value) {
        for (int i = 0; i < array.length(); i++) {
            if (value.equals(array.optString(i))) {
                return true;
            }
        }

        return false;
    }

    private static int max(int... values) {
        int max = values[0];
        for (int value : values) {
            if (value > max) {
                max = value;
            }
        }
        return max;
    }

    private static void putPossibleNullValueInJSONObject(final String key, final Object value, JSONObject jsonObject) throws JSONException{
        if (value != null) {
            jsonObject.put(key, value);
        } else {
            jsonObject.put(key, JSONObject.NULL);
        }
    }

    private static String getKeyFromMapValue(final HashMap<String, Integer> map, int value) {
        for (Entry<String, Integer> entry : map.entrySet()) {
            if (value == entry.getValue()) {
                return entry.getKey();
            }
        }
        return null;
    }

    private String getColumnNameConstant(String field) {
        initColumnNameConstantsMap();
        return mColumnNameConstantsMap.get(field.toLowerCase());
    }

    private void initColumnNameConstantsMap() {
        if (mColumnNameConstantsMap != null) {
            return;
        }
        mColumnNameConstantsMap = new HashMap<String, String>();

        mColumnNameConstantsMap.put("name", StructuredName.DISPLAY_NAME);
        mColumnNameConstantsMap.put("givenname", StructuredName.GIVEN_NAME);
        mColumnNameConstantsMap.put("familyname", StructuredName.FAMILY_NAME);
        mColumnNameConstantsMap.put("honorificprefix", StructuredName.PREFIX);
        mColumnNameConstantsMap.put("honorificsuffix", StructuredName.SUFFIX);
        mColumnNameConstantsMap.put("additionalname", CUSTOM_DATA_COLUMN);
        mColumnNameConstantsMap.put("nickname", Nickname.NAME);
        mColumnNameConstantsMap.put("adr", StructuredPostal.STREET);
        mColumnNameConstantsMap.put("email", Email.ADDRESS);
        mColumnNameConstantsMap.put("url", Website.URL);
        mColumnNameConstantsMap.put("category", GroupMembership.GROUP_ROW_ID);
        mColumnNameConstantsMap.put("tel", Phone.NUMBER);
        mColumnNameConstantsMap.put("org", Organization.COMPANY);
        mColumnNameConstantsMap.put("jobTitle", Organization.TITLE);
        mColumnNameConstantsMap.put("note", Note.NOTE);
        mColumnNameConstantsMap.put("impp", Im.DATA);
        mColumnNameConstantsMap.put("sex", CUSTOM_DATA_COLUMN);
        mColumnNameConstantsMap.put("genderidentity", CUSTOM_DATA_COLUMN);
        mColumnNameConstantsMap.put("key", CUSTOM_DATA_COLUMN);
    }

    private String getMimeTypeOfField(String field) {
        initMimeTypeConstantsMap();
        return mMimeTypeConstantsMap.get(field.toLowerCase());
    }

    private void initMimeTypeConstantsMap() {
        if (mMimeTypeConstantsMap != null) {
            return;
        }
        mMimeTypeConstantsMap = new HashMap<String, String>();

        mMimeTypeConstantsMap.put("name", StructuredName.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("givenname", StructuredName.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("familyname", StructuredName.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("honorificprefix", StructuredName.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("honorificsuffix", StructuredName.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("additionalname", MIMETYPE_ADDITIONAL_NAME);
        mMimeTypeConstantsMap.put("nickname", Nickname.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("email", Email.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("url", Website.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("category", GroupMembership.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("tel", Phone.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("org", Organization.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("jobTitle", Organization.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("note", Note.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("impp", Im.CONTENT_ITEM_TYPE);
        mMimeTypeConstantsMap.put("sex", MIMETYPE_SEX);
        mMimeTypeConstantsMap.put("genderidentity", MIMETYPE_GENDER_IDENTITY);
        mMimeTypeConstantsMap.put("key", MIMETYPE_KEY);
    }

    private int getAddressType(String addressType) {
        initAddressTypesMap();
        Integer type = mAddressTypesMap.get(addressType.toLowerCase());
        return type != null ? type : StructuredPostal.TYPE_CUSTOM;
    }

    private void initAddressTypesMap() {
        if (mAddressTypesMap != null) {
            return;
        }
        mAddressTypesMap = new HashMap<String, Integer>();

        mAddressTypesMap.put("home", StructuredPostal.TYPE_HOME);
        mAddressTypesMap.put("work", StructuredPostal.TYPE_WORK);
    }

    private int getPhoneType(String phoneType) {
        initPhoneTypesMap();
        Integer type = mPhoneTypesMap.get(phoneType.toLowerCase());
        return type != null ? type : Phone.TYPE_CUSTOM;
    }

    private void initPhoneTypesMap() {
        if (mPhoneTypesMap != null) {
            return;
        }
        mPhoneTypesMap = new HashMap<String, Integer>();

        mPhoneTypesMap.put("home", Phone.TYPE_HOME);
        mPhoneTypesMap.put("mobile", Phone.TYPE_MOBILE);
        mPhoneTypesMap.put("work", Phone.TYPE_WORK);
        mPhoneTypesMap.put("fax home", Phone.TYPE_FAX_HOME);
        mPhoneTypesMap.put("fax work", Phone.TYPE_FAX_WORK);
        mPhoneTypesMap.put("pager", Phone.TYPE_PAGER);
        mPhoneTypesMap.put("callback", Phone.TYPE_CALLBACK);
        mPhoneTypesMap.put("car", Phone.TYPE_CAR);
        mPhoneTypesMap.put("company main", Phone.TYPE_COMPANY_MAIN);
        mPhoneTypesMap.put("isdn", Phone.TYPE_ISDN);
        mPhoneTypesMap.put("main", Phone.TYPE_MAIN);
        mPhoneTypesMap.put("fax other", Phone.TYPE_OTHER_FAX);
        mPhoneTypesMap.put("other fax", Phone.TYPE_OTHER_FAX);
        mPhoneTypesMap.put("radio", Phone.TYPE_RADIO);
        mPhoneTypesMap.put("telex", Phone.TYPE_TELEX);
        mPhoneTypesMap.put("tty", Phone.TYPE_TTY_TDD);
        mPhoneTypesMap.put("ttd", Phone.TYPE_TTY_TDD);
        mPhoneTypesMap.put("work mobile", Phone.TYPE_WORK_MOBILE);
        mPhoneTypesMap.put("work pager", Phone.TYPE_WORK_PAGER);
        mPhoneTypesMap.put("assistant", Phone.TYPE_ASSISTANT);
        mPhoneTypesMap.put("mms", Phone.TYPE_MMS);
    }

    private int getEmailType(String emailType) {
        initEmailTypesMap();
        Integer type = mEmailTypesMap.get(emailType.toLowerCase());
        return type != null ? type : Email.TYPE_CUSTOM;
    }

    private void initEmailTypesMap() {
        if (mEmailTypesMap != null) {
            return;
        }
        mEmailTypesMap = new HashMap<String, Integer>();

        mEmailTypesMap.put("home", Email.TYPE_HOME);
        mEmailTypesMap.put("mobile", Email.TYPE_MOBILE);
        mEmailTypesMap.put("work", Email.TYPE_WORK);
    }

    private int getWebsiteType(String webisteType) {
        initWebsiteTypesMap();
        Integer type = mWebsiteTypesMap.get(webisteType.toLowerCase());
        return type != null ? type : Website.TYPE_CUSTOM;
    }

    private void initWebsiteTypesMap() {
        if (mWebsiteTypesMap != null) {
            return;
        }
        mWebsiteTypesMap = new HashMap<String, Integer>();

        mWebsiteTypesMap.put("homepage", Website.TYPE_HOMEPAGE);
        mWebsiteTypesMap.put("blog", Website.TYPE_BLOG);
        mWebsiteTypesMap.put("profile", Website.TYPE_PROFILE);
        mWebsiteTypesMap.put("home", Website.TYPE_HOME);
        mWebsiteTypesMap.put("work", Website.TYPE_WORK);
        mWebsiteTypesMap.put("ftp", Website.TYPE_FTP);
    }

    private int getImType(String imType) {
        initImTypesMap();
        Integer type = mImTypesMap.get(imType.toLowerCase());
        return type != null ? type : Im.TYPE_CUSTOM;
    }

    private void initImTypesMap() {
        if (mImTypesMap != null) {
            return;
        }
        mImTypesMap = new HashMap<String, Integer>();

        mImTypesMap.put("home", Im.TYPE_HOME);
        mImTypesMap.put("work", Im.TYPE_WORK);
    }

    private String[] getAllColumns() {
        return new String[] {Entity.DATA_ID, Data.MIMETYPE, Data.IS_SUPER_PRIMARY,
                             Data.DATA1, Data.DATA2, Data.DATA3, Data.DATA4,
                             Data.DATA5, Data.DATA6, Data.DATA7, Data.DATA8,
                             Data.DATA9, Data.DATA10, Data.DATA11, Data.DATA12,
                             Data.DATA13, Data.DATA14, Data.DATA15};
    }
}
