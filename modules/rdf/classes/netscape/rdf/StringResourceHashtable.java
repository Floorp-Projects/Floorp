/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package netscape.rdf;

import netscape.rdf.Resource;
/*
 * a hashtable for recording String->IUnit mappings. 
 * very very economical on space and pretty good on performance. 
 *
 * author: guha
 */
class StringResourceHashtable 
{
    Object[]	keyData; 
    short[]  	nextList; 
    short[]   	map; 
    int     	size;
    int     	mapSize = 512; 
    int     	mask ; 
    private int next  = 0; 
    
    StringResourceHashtable(int size) 
    { 
        this.size = size; 
        mask = mapSize-1; 
        init(); 
    }
 
    int stringHash(Object str) 
    {
        return (str.hashCode() >> 4) & mask;
    }

    void init() 
    {
        keyData = new Object[size*2];
        nextList = new short[size];
        map  = new short[mapSize];

        clear();
    }

    void free(short n) 
    {
        if (next != -1) 
	{
            nextList[n] = (short)next;
            keyData[(n*2)] = null;
            keyData[(n*2)+1] = null;

            next = n;
        } 
	else 
	{
	    next = n;
	}
    }

    short getNext() 
    {
	short ret;

        if (next == -1) 
	{
            grow();
	    ret = getNext();
        } 
	else 
	{
            short ans = (short) next;
            next = nextList[next];

            ret = ans;
        }

	return ret;
    }

    void put(Object str, Resource u) 
    {
        if (u == null) 
	{
	    remove(str);
	}

        int hs = stringHash(str);
        int index =  map[hs];

        while (index != -1) 
	{
            Object nstr = (Object) keyData[2*index];
            short ni = nextList[index];

            if (nstr.equals(str)) 
	    {
                keyData[(2*index)+1] = u;

                return;
            } 
	    else 
	    {
                index = ni;
            }
        }

        int old = map[hs];
        int nx = getNext();

        keyData[(2*nx)] = str;
        keyData[(2*nx)+1] = u;
        nextList[nx] = (short)old;
        map[hs] = (short)nx;
    }

    Resource get(Object str) 
    {
        int hs = stringHash(str);
        int index =  map[hs];

        while (index != -1) 
        {
            Object nstr = (Object) keyData[2*index];
            int next = nextList[index];
            boolean equals;

            if (str.equals(nstr)) 
            {
               return (Resource)keyData[(2*index)+1];
            } 
            else 
            {
                index = next;
            }
        }

        return null;
    }

    void grow() 
    {
        int oldSize = size;

        size = (int)(size * 1.2);

        Object[] okeyData = keyData;
        short[] onextList = nextList;
        short[] omap = map;
        int onext    = next;

        init();

        System.arraycopy(okeyData, 0, keyData, 0, oldSize*2);
        System.arraycopy(onextList, 0, nextList, 0, oldSize);
        System.arraycopy(omap, 0, map, 0, mapSize);

	if (onext == -1) 
	{
	    next = oldSize;
	}
	else 
	{
	    next = onext;
	    nextList[onext] = (short) oldSize;
	}
    }

    void clear() 
    {
        next = 0;
        int n = 0;
        keyData[n] = null;

        while (n < size-1) 
	{
            keyData[2*n] = null;
            keyData[(2*n)+1] = null;
            nextList[n] = (short)(n+1);
            nextList[n+1] = -1;

            n++;
        }
        n = 0;

        while (n < mapSize) 
	{
            map[n] = -1;
            n++;
        }
    }

    void remove(Object str) 
    {
        int hs = stringHash(str);
        int index =  map[hs];
        int pr    = index;

        while (index != -1) 
	{
            Object nstr = (Object)keyData[2*index];
            short ni = nextList[index];

            if (str.equals(nstr)) 
	    {
                if (pr == index) 
		{
                    map[hs] = (short)ni;
                } 
		else 
		{
                    nextList[pr] = (short)ni;
                }

                free((short)index);

                return;
            } 
	    else 
	    {
                pr = index;
                index = ni;
            }
        }
    }
}





















