
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
public class Test2{
private int int_field_super = 1;
public int name_int_super = 1;
public String name_string_super = "aaa";
public final int public_super_int = 1;
protected int protected_super_int = 1;
protected static int protected_static_super_int = 1;
public static int public_static_super_int = 1;
public static String public_static_super_object;


protected Test2(){
}

public Test2(String s){
  System.out.println("Set name_string_super to "+s);
  name_string_super = s;

}

public void Set_int_field_super(int field){
    System.out.println("set method - int_field_super = "+int_field_super);
    int_field_super = field;
}

public int Get_int_field_super(){
    System.out.println("IN super class! +" +int_field_super);
    return int_field_super;
}

private int Get_int_field_super_private(){
    System.out.println("aaaaaaaaaa");
    return 20;
}

private int Get_int_field_super_private_static(){
    System.out.println("aaaaaaaaaa");
    return 20;
}

public static int Get_int_field_super_static(){
    System.out.println("Get_int_field_super_static");
    return 0;
}

public int Test2_override(int i){
     return i;
}

public static int Test2_override_static(int i){
     return i;
}

protected void Print_string_super_protected(String s){
    System.out.println("The String is: "+s);
}

void Print_string_super_nomod(String s){
    System.out.println("The String is: "+s);
}

protected static void Print_string_super_protected_static(String s){
    System.out.println("The String is: "+s);
}

static void Print_string_super_nomod_static(String s){
    System.out.println("The String is: "+s);
}

public int Test1_method3_override(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3 passed!");
      return 11;
}

public int Test2_method3(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method3 passed!");
    return 121;
}

private int Test2_method31(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method31 passed!");
    return 121;
}

protected int Test2_method32(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method32 passed!");
    return 121;
}

int Test2_method33(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method33 passed!");
    return 121;
}

public static int Test2_method3_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method3_static passed!");
    return 121;
}

private static int Test2_method31_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method31_static passed!");
    return 121;
}

protected static int Test2_method32_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method32_static passed!");
    return 121;
}

static int Test2_method33_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
    System.out.println("Test2_method33_static passed!");
    return 121;
}

}
