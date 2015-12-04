/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.mozilla.gecko.sqlite;

import java.nio.ByteBuffer;
import java.util.ArrayList;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants;

import android.database.AbstractCursor;
import android.database.CursorIndexOutOfBoundsException;
import android.util.Log;

/**
 * A mutable cursor implementation backed by an array of {@code Object}s. Use
 * {@link #newRow()} to add rows. Automatically expands internal capacity
 * as needed.
 *
 * This class provides one missing feature from Android's MatrixCursor:
 * the implementation of getBlob that was inadvertently omitted from API 9 (and
 * perhaps later; it's present in 14).
 *
 * MatrixCursor is all private, so we entirely duplicate it here.
 */
public class MatrixBlobCursor extends AbstractCursor {
    private static final String LOGTAG = "GeckoMatrixCursor";

    private final String[] columnNames;
    private final int columnCount;

    private int rowCount;
    private Throwable allocationStack;

    Object[] data;

    /**
     * Constructs a new cursor with the given initial capacity.
     *
     * @param columnNames names of the columns, the ordering of which
     *  determines column ordering elsewhere in this cursor
     * @param initialCapacity in rows
     */
    @WrapForJNI
    public MatrixBlobCursor(String[] columnNames, int initialCapacity) {
        this.columnNames = columnNames;
        this.columnCount = columnNames.length;

        if (initialCapacity < 1) {
            initialCapacity = 1;
        }

        this.data = new Object[columnCount * initialCapacity];
        if (AppConstants.DEBUG_BUILD) {
            this.allocationStack = new Throwable("allocationStack");
        }
    }

    /**
     * Constructs a new cursor.
     *
     * @param columnNames names of the columns, the ordering of which
     *  determines column ordering elsewhere in this cursor
     */
    @WrapForJNI
    public MatrixBlobCursor(String[] columnNames) {
        this(columnNames, 16);
    }

    /**
     * Closes the Cursor, releasing all of its resources.
     */
    public void close() {
        this.allocationStack = null;
        this.data = null;
        super.close();
    }

    /**
     * Gets value at the given column for the current row.
     */
    protected Object get(int column) {
        if (column < 0 || column >= columnCount) {
            throw new CursorIndexOutOfBoundsException("Requested column: "
                    + column + ", # of columns: " +  columnCount);
        }
        if (mPos < 0) {
            throw new CursorIndexOutOfBoundsException("Before first row.");
        }
        if (mPos >= rowCount) {
            throw new CursorIndexOutOfBoundsException("After last row.");
        }
        return data[mPos * columnCount + column];
    }

    /**
     * Adds a new row to the end and returns a builder for that row. Not safe
     * for concurrent use.
     *
     * @return builder which can be used to set the column values for the new
     *  row
     */
    public RowBuilder newRow() {
        rowCount++;
        int endIndex = rowCount * columnCount;
        ensureCapacity(endIndex);
        int start = endIndex - columnCount;
        return new RowBuilder(start, endIndex);
    }

    /**
     * Adds a new row to the end with the given column values. Not safe
     * for concurrent use.
     *
     * @throws IllegalArgumentException if {@code columnValues.length !=
     *  columnNames.length}
     * @param columnValues in the same order as the the column names specified
     *  at cursor construction time
     */
    @WrapForJNI
    public void addRow(Object[] columnValues) {
        if (columnValues.length != columnCount) {
            throw new IllegalArgumentException("columnNames.length = "
                    + columnCount + ", columnValues.length = "
                    + columnValues.length);
        }

        int start = rowCount++ * columnCount;
        ensureCapacity(start + columnCount);
        System.arraycopy(columnValues, 0, data, start, columnCount);
    }

    /**
     * Adds a new row to the end with the given column values. Not safe
     * for concurrent use.
     *
     * @throws IllegalArgumentException if {@code columnValues.size() !=
     *  columnNames.length}
     * @param columnValues in the same order as the the column names specified
     *  at cursor construction time
     */
    @WrapForJNI
    public void addRow(Iterable<?> columnValues) {
        final int start = rowCount * columnCount;

        if (columnValues instanceof ArrayList<?>) {
            addRow((ArrayList<?>) columnValues, start);
            return;
        }

        final int end = start + columnCount;
        int current = start;

        ensureCapacity(end);
        final Object[] localData = data;
        for (Object columnValue : columnValues) {
            if (current == end) {
                // TODO: null out row?
                throw new IllegalArgumentException(
                        "columnValues.size() > columnNames.length");
            }
            localData[current++] = columnValue;
        }

        if (current != end) {
            // TODO: null out row?
            throw new IllegalArgumentException(
                    "columnValues.size() < columnNames.length");
        }

        // Increase row count here in case we encounter an exception.
        rowCount++;
    }

    /** Optimization for {@link ArrayList}. */
    @WrapForJNI
    private void addRow(ArrayList<?> columnValues, int start) {
        final int size = columnValues.size();
        if (size != columnCount) {
            throw new IllegalArgumentException("columnNames.length = "
                    + columnCount + ", columnValues.size() = " + size);
        }

        final int end = start + columnCount;
        ensureCapacity(end);

        // Take a reference just in case someone calls ensureCapacity
        // and `data` gets replaced by a new array!
        final Object[] localData = data;
        for (int i = 0; i < size; i++) {
            localData[start + i] = columnValues.get(i);
        }

        rowCount++;
    }

    /**
     * Ensures that this cursor has enough capacity. If it needs to allocate
     * a new array, the existing capacity will be at least doubled.
     */
    private void ensureCapacity(final int size) {
        if (size <= data.length) {
            return;
        }

        final Object[] oldData = this.data;
        this.data = new Object[Math.max(size, data.length * 2)];
        System.arraycopy(oldData, 0, this.data, 0, oldData.length);
    }

    /**
     * Builds a row, starting from the left-most column and adding one column
     * value at a time. Follows the same ordering as the column names specified
     * at cursor construction time.
     *
     * Not thread-safe.
     */
    public class RowBuilder {
        private int index;
        private final int endIndex;

        RowBuilder(int index, int endIndex) {
            this.index = index;
            this.endIndex = endIndex;
        }

        /**
         * Sets the next column value in this row.
         *
         * @throws CursorIndexOutOfBoundsException if you try to add too many
         *  values
         * @return this builder to support chaining
         */
        public RowBuilder add(final Object columnValue) {
            if (index == endIndex) {
                throw new CursorIndexOutOfBoundsException("No more columns left.");
            }

            data[index++] = columnValue;
            return this;
        }
    }

    /**
     * Not thread safe.
     */
    public void set(int column, Object value) {
        if (column < 0 || column >= columnCount) {
            throw new CursorIndexOutOfBoundsException("Requested column: "
                    + column + ", # of columns: " +  columnCount);
        }
        if (mPos < 0) {
            throw new CursorIndexOutOfBoundsException("Before first row.");
        }
        if (mPos >= rowCount) {
            throw new CursorIndexOutOfBoundsException("After last row.");
        }
        data[mPos * columnCount + column] = value;
    }

    // AbstractCursor implementation.
    @Override
    public int getCount() {
        return rowCount;
    }

    @Override
    public String[] getColumnNames() {
        return columnNames;
    }

    @Override
    public String getString(int column) {
        Object value = get(column);
        if (value == null) return null;
        return value.toString();
    }

    @Override
    public short getShort(int column) {
        final Object value = get(column);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).shortValue();
        return Short.parseShort(value.toString());
    }

    @Override
    public int getInt(int column) {
        Object value = get(column);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).intValue();
        return Integer.parseInt(value.toString());
    }

    @Override
    public long getLong(int column) {
        Object value = get(column);
        if (value == null) return 0;
        if (value instanceof Number) return ((Number) value).longValue();
        return Long.parseLong(value.toString());
    }

    @Override
    public float getFloat(int column) {
        Object value = get(column);
        if (value == null) return 0.0f;
        if (value instanceof Number) return ((Number) value).floatValue();
        return Float.parseFloat(value.toString());
    }

    @Override
    public double getDouble(int column) {
        Object value = get(column);
        if (value == null) return 0.0d;
        if (value instanceof Number) return ((Number) value).doubleValue();
        return Double.parseDouble(value.toString());
    }

    @Override
    public byte[] getBlob(int column) {
        Object value = get(column);
        if (value == null) return null;
        if (value instanceof byte[]) {
            return (byte[]) value;
        }

        if (value instanceof ByteBuffer) {
            final ByteBuffer bytes = (ByteBuffer) value;
            byte[] byteArray = new byte[bytes.remaining()];
            bytes.get(byteArray);
            return byteArray;
        }
        throw new UnsupportedOperationException("BLOB Object not of known type");
    }

    @Override
    public boolean isNull(int column) {
        return get(column) == null;
    }

    @Override
    protected void finalize() {
        if (AppConstants.DEBUG_BUILD) {
            if (!isClosed()) {
                Log.e(LOGTAG, "Cursor finalized without being closed", this.allocationStack);
            }
        }

        super.finalize();
    }
}
