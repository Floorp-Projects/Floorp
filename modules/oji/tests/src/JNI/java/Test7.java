
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
public class Test7 extends Test4
{

  public int name_int = 0;
  private String str;
  public Test7(String s, String s1){
      super(s); str = s1;

  }
 
  public void Set_int(int i){
     name_int = i;
  }

  int Abs_nomod_abstract_int(int i){
      return i;
  }

  protected int Abs_protected_abstract_int(int i){
      return i;
  }

  public int Abs_public_abstract_int(int i){
      return i;
  }

  int Test9_abs_nomod_int(int i){
      return i;
  }

  protected int Test9_abs_protected_int(int i){
      return i;
  }

  public int Test9_abs_public_int(int i){
      return i;
  }


}
