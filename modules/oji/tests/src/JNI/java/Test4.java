
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
public abstract class Test4 extends Test9{
public String name_string;
public int name_int = 0;

public Test4(String s){
   System.out.println("name-string set to "+s);
   name_string = s;
}

private Test4(int i){
   System.out.println("name-int set to "+i);
   name_int = i;
}

protected Test4(){
   name_int = 10;
}

Test4(int i, String str){
   name_string = str;
   name_int  = i;
}

public int Ret_int(){
   return 10;
}

public abstract int Abs_public_abstract_int(int i);

protected abstract int Abs_protected_abstract_int(int i);

abstract int Abs_nomod_abstract_int(int i);


}
