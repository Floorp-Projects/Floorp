#!/usr/bin/env python 
# -*- Mode: python -*-
#======================================================================
# FILE: Store.py
# CREATOR: eric 
#
# DESCRIPTION:
#   
#
#  $Id: Store.py,v 1.2 2001/12/21 18:56:52 mikep%oeone.com Exp $
#  $Locker:  $
#
# (C) COPYRIGHT 2001, Eric Busboom <eric@softwarestudio.org>
# (C) COPYRIGHT 2001, Patrick Lewis <plewis@inetarena.com>  
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of either: 
#
#    The LGPL as published by the Free Software Foundation, version
#    2.1, available at: http://www.fsf.org/copyleft/lesser.html
#
#  Or:
#
#    The Mozilla Public License Version 1.0. You may obtain a copy of
#    the License at http://www.mozilla.org/MPL/
#======================================================================

from LibicalWrap import *
from Error import LibicalError
from Component import Component

class Store:
    """ 
    Base class for several component storage methods 
    """

    class AddFailedError(LibicalError):
        "Failed to add a property to the file store"
        
    class ConstructorFailedError(LibicalError):
        "Failed to create a Store "
    
    def __init__(self):
        pass

    def path(self):
        pass
    
    def mark(self):
        pass
    
    def commit(self): 
        pass
    
    def add_component(self, comp):
        pass
    
    def remove_component(self, comp):
        pass
    
    def count_components(self, kind):
        pass
    
    def select(self, gauge):
        pass
    
    def clearSelect(self):
        pass
    
    def fetch(self, uid):
        pass
    
    def fetchMatch(self, comp):
        pass
    
    def modify(self, oldc, newc):
        pass
    
    def current_component(self):
        pass
    
    def first_component(self):
        pass
    
    def next_component(self):
        pass


class FileStore(Store):

    def __init__(self, file,mode="r",flags=0664):

        _flags = icallangbind_string_to_open_flag(mode)


        if _flags == -1:
            raise Store.ConstructorFailedError("Illegal value for mode: "+mode)

        e1=icalerror_supress("FILE")
        self._ref = icalfileset_new_open(file,_flags,flags)
        icalerror_restore("FILE",e1)

        print self._ref

        if self._ref == None or self._ref == 'NULL':
            raise  Store.ConstructorFailedError(file)

    def __del__(self):
        icalfileset_free(self._ref)

    def path(self):
        return icalfileset_path(self._ref)

    def mark(self):
        icalfileset_mark(self._ref)

    def commit(self): 
        icalfileset_commit(self._ref)

    def add_component(self, comp):
        if not isinstance(comp,Component):
            raise Store.AddFailedError("Argument is not a component")
            
        error = icalfileset_add_component(self._ref,comp)

    def remove_component(self, comp):
        if not isinstance(comp,Component):
            raise Store.AddFailedError("Argument is not a component")

        error = icalfileset_remove_component(self._ref,comp)



    def count_components(self, kind):
        pass

    def select(self, gauge):
        pass

    def clearSelect(self):
        pass

    def fetch(self, uid):
        pass

    def fetchMatch(self, comp):
        pass

    def modify(self, oldc, newc):
        pass

    def current_component(self):
        comp_ref = icalfileset_get_current_component(self._ref)

        if comp_ref == None:
            return None

        return Component(ref=comp_ref)

    def first_component(self):
        comp_ref = icalfileset_get_first_component(self._ref)

        if comp_ref == None:
            return None

        return Component(ref=comp_ref)

    def next_component(self):
        
        comp_ref = icalfileset_get_next_component(self._ref)

        if comp_ref == None:
            return None

        return Component(ref=comp_ref)

