/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// When the platform supports STL, the functions are implemented using a
// templated spreadsort algorithm (http://sourceforge.net/projects/spreadsort/),
// part of the Boost C++ library collection. Otherwise, the C standard library's
// qsort() will be used.

#include "sort.h"

#include <cassert>
#include <cstring>  // memcpy
#include <new>      // nothrow new

#ifdef NO_STL
#include <cstdlib>      // qsort
#else
#include <algorithm>    // std::sort
#include <vector>
#include "spreadsort.hpp" // TODO (ajm) upgrade to spreadsortv2.
#endif

#ifdef NO_STL
#define COMPARE_DEREFERENCED(XT, YT)        \
    do                                      \
    {                                       \
        if ((XT) > (YT))                    \
        {                                   \
            return 1;                       \
        }                                   \
        else if ((XT) < (YT))               \
        {                                   \
            return -1;                      \
        }                                   \
                                            \
        return 0;                           \
    }                                       \
    while(0)

#define COMPARE_FOR_QSORT(X, Y, TYPE)                               \
    do                                                              \
    {                                                               \
        TYPE xT = static_cast<TYPE>(*static_cast<const TYPE*>(X));  \
        TYPE yT = static_cast<TYPE>(*static_cast<const TYPE*>(Y));  \
        COMPARE_DEREFERENCED(xT, yT);                               \
    }                                                               \
    while(0)

#define COMPARE_KEY_FOR_QSORT(SORT_KEY_X, SORT_KEY_Y, TYPE)         \
    do                                                              \
    {                                                               \
        TYPE xT = static_cast<TYPE>(*static_cast<TYPE*>             \
            (static_cast<const SortKey*>(SORT_KEY_X)->key));        \
        TYPE yT = static_cast<TYPE>(*static_cast<TYPE*>             \
            (static_cast<const SortKey*>(SORT_KEY_Y)->key));        \
        COMPARE_DEREFERENCED(xT, yT);                               \
    }                                                               \
    while(0)

#define KEY_QSORT(SORT_KEY, KEY, NUM_OF_ELEMENTS, KEY_TYPE, COMPARE_FUNC)     \
    do                                                                        \
    {                                                                         \
        KEY_TYPE* keyT = (KEY_TYPE*)(key);                                    \
        for (WebRtc_UWord32 i = 0; i < (NUM_OF_ELEMENTS); i++)                \
        {                                                                     \
            ptrSortKey[i].key = &keyT[i];                                     \
            ptrSortKey[i].index = i;                                          \
        }                                                                     \
                                                                              \
        qsort((SORT_KEY), (NUM_OF_ELEMENTS), sizeof(SortKey), (COMPARE_FUNC));\
    }                                                                         \
    while(0)
#endif

namespace webrtc
{
#ifdef NO_STL
    struct SortKey
    {
        void* key;
        WebRtc_UWord32 index;
    };
#else
    template<typename KeyType>
    struct SortKey
    {
        KeyType key;
        WebRtc_UWord32 index;
    };
#endif

    namespace // Unnamed namespace provides internal linkage.
    {
#ifdef NO_STL
        int CompareWord8(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_Word8);
        }

        int CompareUWord8(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_UWord8);
        }

        int CompareWord16(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_Word16);
        }

        int CompareUWord16(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_UWord16);
        }

        int CompareWord32(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_Word32);
        }

        int CompareUWord32(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_UWord32);
        }

        int CompareWord64(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_Word64);
        }

        int CompareUWord64(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, WebRtc_UWord64);
        }

        int CompareFloat32(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, float);
        }

        int CompareFloat64(const void* x, const void* y)
        {
            COMPARE_FOR_QSORT(x, y, double);
        }

        int CompareKeyWord8(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_Word8);
        }

        int CompareKeyUWord8(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_UWord8);
        }

        int CompareKeyWord16(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_Word16);
        }

        int CompareKeyUWord16(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_UWord16);
        }

        int CompareKeyWord32(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_Word32);
        }

        int CompareKeyUWord32(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_UWord32);
        }

        int CompareKeyWord64(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_Word64);
        }

        int CompareKeyUWord64(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, WebRtc_UWord64);
        }

        int CompareKeyFloat32(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, float);
        }

        int CompareKeyFloat64(const void* sortKeyX, const void* sortKeyY)
        {
            COMPARE_KEY_FOR_QSORT(sortKeyX, sortKeyY, double);
        }
#else
        template <typename KeyType>
        struct KeyLessThan
        {
            bool operator()(const SortKey<KeyType>& sortKeyX,
                const SortKey<KeyType>& sortKeyY) const
            {
                return sortKeyX.key < sortKeyY.key;
            }
        };

        template <typename KeyType>
        struct KeyRightShift
        {
            KeyType operator()(const SortKey<KeyType>& sortKey,
                const unsigned offset) const
            {
                return sortKey.key >> offset;
            }
        };

        template <typename DataType>
        inline void IntegerSort(void* data, WebRtc_UWord32 numOfElements)
        {
            DataType* dataT = static_cast<DataType*>(data);
            boost::integer_sort(dataT, dataT + numOfElements);
        }

        template <typename DataType, typename IntegerType>
        inline void FloatSort(void* data, WebRtc_UWord32 numOfElements)
        {
            DataType* dataT = static_cast<DataType*>(data);
            IntegerType cVal = 0;
            boost::float_sort_cast(dataT, dataT + numOfElements, cVal);
        }

        template <typename DataType>
        inline void StdSort(void* data, WebRtc_UWord32 numOfElements)
        {
            DataType* dataT = static_cast<DataType*>(data);
            std::sort(dataT, dataT + numOfElements);
        }

        template<typename KeyType>
        inline WebRtc_Word32 SetupKeySort(void* key,
                                          SortKey<KeyType>*& ptrSortKey,
                                          WebRtc_UWord32 numOfElements)
        {
            ptrSortKey = new(std::nothrow) SortKey<KeyType>[numOfElements];
            if (ptrSortKey == NULL)
            {
                return -1;
            }

            KeyType* keyT = static_cast<KeyType*>(key);
            for (WebRtc_UWord32 i = 0; i < numOfElements; i++)
            {
                ptrSortKey[i].key = keyT[i];
                ptrSortKey[i].index = i;
            }

            return 0;
        }

        template<typename KeyType>
        inline WebRtc_Word32 TeardownKeySort(void* data,
                                             SortKey<KeyType>* ptrSortKey,
            WebRtc_UWord32 numOfElements, WebRtc_UWord32 sizeOfElement)
        {
            WebRtc_UWord8* ptrData = static_cast<WebRtc_UWord8*>(data);
            WebRtc_UWord8* ptrDataSorted = new(std::nothrow) WebRtc_UWord8
                [numOfElements * sizeOfElement];
            if (ptrDataSorted == NULL)
            {
                return -1;
            }

            for (WebRtc_UWord32 i = 0; i < numOfElements; i++)
            {
                memcpy(ptrDataSorted + i * sizeOfElement, ptrData +
                       ptrSortKey[i].index * sizeOfElement, sizeOfElement);
            }
            memcpy(ptrData, ptrDataSorted, numOfElements * sizeOfElement);
            delete[] ptrSortKey;
            delete[] ptrDataSorted;
            return 0;
        }

        template<typename KeyType>
        inline WebRtc_Word32 IntegerKeySort(void* data, void* key,
                                            WebRtc_UWord32 numOfElements,
                                            WebRtc_UWord32 sizeOfElement)
        {
            SortKey<KeyType>* ptrSortKey;
            if (SetupKeySort<KeyType>(key, ptrSortKey, numOfElements) != 0)
            {
                return -1;
            }

            boost::integer_sort(ptrSortKey, ptrSortKey + numOfElements,
                KeyRightShift<KeyType>(), KeyLessThan<KeyType>());

            if (TeardownKeySort<KeyType>(data, ptrSortKey, numOfElements,
                    sizeOfElement) != 0)
            {
                return -1;
            }

            return 0;
        }

        template<typename KeyType>
        inline WebRtc_Word32 StdKeySort(void* data, void* key,
                                        WebRtc_UWord32 numOfElements,
                                        WebRtc_UWord32 sizeOfElement)
        {
            SortKey<KeyType>* ptrSortKey;
            if (SetupKeySort<KeyType>(key, ptrSortKey, numOfElements) != 0)
            {
                return -1;
            }

            std::sort(ptrSortKey, ptrSortKey + numOfElements,
                KeyLessThan<KeyType>());

            if (TeardownKeySort<KeyType>(data, ptrSortKey, numOfElements,
                    sizeOfElement) != 0)
            {
                return -1;
            }

            return 0;
        }
#endif
    }

    WebRtc_Word32 Sort(void* data, WebRtc_UWord32 numOfElements, Type type)
    {
        if (data == NULL)
        {
            return -1;
        }

#ifdef NO_STL
        switch (type)
        {
        case TYPE_Word8:
            qsort(data, numOfElements, sizeof(WebRtc_Word8), CompareWord8);
            break;
        case TYPE_UWord8:
            qsort(data, numOfElements, sizeof(WebRtc_UWord8), CompareUWord8);
            break;
        case TYPE_Word16:
            qsort(data, numOfElements, sizeof(WebRtc_Word16), CompareWord16);
            break;
        case TYPE_UWord16:
            qsort(data, numOfElements, sizeof(WebRtc_UWord16), CompareUWord16);
            break;
        case TYPE_Word32:
            qsort(data, numOfElements, sizeof(WebRtc_Word32), CompareWord32);
            break;
        case TYPE_UWord32:
            qsort(data, numOfElements, sizeof(WebRtc_UWord32), CompareUWord32);
            break;
        case TYPE_Word64:
            qsort(data, numOfElements, sizeof(WebRtc_Word64), CompareWord64);
            break;
        case TYPE_UWord64:
            qsort(data, numOfElements, sizeof(WebRtc_UWord64), CompareUWord64);
            break;
        case TYPE_Float32:
            qsort(data, numOfElements, sizeof(float), CompareFloat32);
            break;
        case TYPE_Float64:
            qsort(data, numOfElements, sizeof(double), CompareFloat64);
            break;
        default:
            return -1;
        }
#else
        // Fall back to std::sort for 64-bit types and floats due to compiler
	// warnings and VS 2003 build crashes respectively with spreadsort.
        switch (type)
        {
        case TYPE_Word8:
            IntegerSort<WebRtc_Word8>(data, numOfElements);
            break;
        case TYPE_UWord8:
            IntegerSort<WebRtc_UWord8>(data, numOfElements);
            break;
        case TYPE_Word16:
            IntegerSort<WebRtc_Word16>(data, numOfElements);
            break;
        case TYPE_UWord16:
            IntegerSort<WebRtc_UWord16>(data, numOfElements);
            break;
        case TYPE_Word32:
            IntegerSort<WebRtc_Word32>(data, numOfElements);
            break;
        case TYPE_UWord32:
            IntegerSort<WebRtc_UWord32>(data, numOfElements);
            break;
        case TYPE_Word64:
            StdSort<WebRtc_Word64>(data, numOfElements);
            break;
        case TYPE_UWord64:
            StdSort<WebRtc_UWord64>(data, numOfElements);
            break;
        case TYPE_Float32:
            StdSort<float>(data, numOfElements);
            break;
        case TYPE_Float64:
            StdSort<double>(data, numOfElements);
            break;
        }
#endif
        return 0;
    }

    WebRtc_Word32 KeySort(void* data, void* key, WebRtc_UWord32 numOfElements,
                          WebRtc_UWord32 sizeOfElement, Type keyType)
    {
        if (data == NULL)
        {
            return -1;
        }

        if (key == NULL)
        {
            return -1;
        }

        if ((WebRtc_UWord64)numOfElements * sizeOfElement > 0xffffffff)
        {
            return -1;
        }

#ifdef NO_STL
        SortKey* ptrSortKey = new(std::nothrow) SortKey[numOfElements];
        if (ptrSortKey == NULL)
        {
            return -1;
        }

        switch (keyType)
        {
        case TYPE_Word8:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_Word8,
                CompareKeyWord8);
            break;
        case TYPE_UWord8:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_UWord8,
                CompareKeyUWord8);
            break;
        case TYPE_Word16:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_Word16,
                CompareKeyWord16);
            break;
        case TYPE_UWord16:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_UWord16,
                CompareKeyUWord16);
            break;
        case TYPE_Word32:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_Word32,
                CompareKeyWord32);
            break;
        case TYPE_UWord32:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_UWord32,
                CompareKeyUWord32);
            break;
        case TYPE_Word64:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_Word64,
                CompareKeyWord64);
            break;
        case TYPE_UWord64:
            KEY_QSORT(ptrSortKey, key, numOfElements, WebRtc_UWord64,
                CompareKeyUWord64);
            break;
        case TYPE_Float32:
            KEY_QSORT(ptrSortKey, key, numOfElements, float,
                CompareKeyFloat32);
            break;
        case TYPE_Float64:
            KEY_QSORT(ptrSortKey, key, numOfElements, double,
                CompareKeyFloat64);
            break;
        default:
            return -1;
        }

        // Shuffle into sorted position based on index map.
        WebRtc_UWord8* ptrData = static_cast<WebRtc_UWord8*>(data);
        WebRtc_UWord8* ptrDataSorted = new(std::nothrow) WebRtc_UWord8
            [numOfElements * sizeOfElement];
        if (ptrDataSorted == NULL)
        {
            return -1;
        }

        for (WebRtc_UWord32 i = 0; i < numOfElements; i++)
        {
            memcpy(ptrDataSorted + i * sizeOfElement, ptrData +
                ptrSortKey[i].index * sizeOfElement, sizeOfElement);
        }
        memcpy(ptrData, ptrDataSorted, numOfElements * sizeOfElement);

        delete[] ptrSortKey;
        delete[] ptrDataSorted;

        return 0;
#else
        // Fall back to std::sort for 64-bit types and floats due to compiler
	// warnings and errors respectively with spreadsort.
        switch (keyType)
        {
        case TYPE_Word8:
            return IntegerKeySort<WebRtc_Word8>(data, key, numOfElements,
                                                sizeOfElement);
        case TYPE_UWord8:
            return IntegerKeySort<WebRtc_UWord8>(data, key, numOfElements,
                                                 sizeOfElement);
        case TYPE_Word16:
            return IntegerKeySort<WebRtc_Word16>(data, key, numOfElements,
                                                 sizeOfElement);
        case TYPE_UWord16:
            return IntegerKeySort<WebRtc_UWord16>(data, key, numOfElements,
                                                  sizeOfElement);
        case TYPE_Word32:
            return IntegerKeySort<WebRtc_Word32>(data, key, numOfElements,
                                                 sizeOfElement);
        case TYPE_UWord32:
            return IntegerKeySort<WebRtc_UWord32>(data, key, numOfElements,
                                                  sizeOfElement);
        case TYPE_Word64:
            return StdKeySort<WebRtc_Word64>(data, key, numOfElements,
                                             sizeOfElement);
        case TYPE_UWord64:
            return StdKeySort<WebRtc_UWord64>(data, key, numOfElements,
                                              sizeOfElement);
        case TYPE_Float32:
            return StdKeySort<float>(data, key, numOfElements, sizeOfElement);
        case TYPE_Float64:
            return StdKeySort<double>(data, key, numOfElements, sizeOfElement);
        }
        assert(false);
        return -1;
#endif
    }
} // namespace webrtc
