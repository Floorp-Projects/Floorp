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

public class Test1 extends Test2{

  public final int final_int = 10;
  public static final int static_final_int = 11;
  protected int protected_int = 1;
  int nomodifiers_int = 1;
  private int int_field = 1;
  public int name_int = 1;
  public int name_int_arr[] = {1, 2, 3};
  public boolean name_bool = true;
  public long name_long = 1;
  public double name_double = 1;
  public float name_float = 0;
  public short name_short = 0;
  public byte name_byte = 0;
  public Object name_object = new Object();
  public char name_char = 'a';
  public static int static_name_int = 1;
  public static String static_name_object;
  static int static_name_int_nomodifiers = 1;
  private static int static_name_int_private =1;
  protected static int static_name_int_protected =1;
  public static boolean static_name_bool = true;
  public static int static_name_int_arr[] = {0, 1, 0};
  public static long static_name_long = 1;
  public static double static_name_double = 1;
  public static float static_name_float = 0;
  public static short static_name_short = 0;
  public static byte static_name_byte = 0;
  public static String static_name_string = "aaa";
  public static char static_name_char = 'a';
  public String name_string = "Test string";
  public Object name_obj = new Object();

  public Test1(int int_fld){
      name_int = int_fld;
  } 

  public Test1(float flt){
      System.out.println("name_float will be set to: "+flt);
      name_float = flt;
  } 

  public Test1(String str){
      name_string = str;
      System.out.println("name_string is: " + name_string);
  } 

  private Test1(){
     name_int = 111;

  }

  protected Test1(boolean bool){
     name_bool = bool;
  }

  Test1(short sh){
     name_short = sh;
  }


  public int Test1_test1(int i){
     return i;
  }

  public static void Print_static_string(String string) {
      System.out.println("The string is :"+string);
  }

  public void Print_string(String string) {
      System.out.println("The string is :"+string);
  }

  void Print_string_nomod(String string){
      System.out.println("The String is: "+string);
  }

  static void Print_string_nomod_static(String string){
      System.out.println("The String is: "+string);
  }

  protected void Print_string_protected(String string) {
      System.out.println("The string is :"+string);
  }

  protected static void Print_string_protected_static(String string) {
      System.out.println("The string is :"+string);
  }

  private void Print_string_private(String string) {
      System.out.println("The string is :"+string);
  }

  private static void Print_string_private_static(String string) {
      System.out.println("The string is :"+string);
  }

  public void Set_int_field(int field){
      name_int = field;
      System.out.println("The name_int was set to "+name_int);
  }
  
  public static int Get_static_int_field(){
      System.out.println("The name_int value is "+static_name_int);
      return static_name_int;
  }

  public int Get_int_field(){
      System.out.println("The name_int value is "+name_int);
      return name_int;
  }
  
  public void Set_bool_field(boolean field){
      name_bool = field;
      System.out.println("The name_bool was set to "+name_bool);
  }
  
  public static boolean Get_static_bool_field(){
      System.out.println("The name_bool value is "+static_name_bool);
      return static_name_bool;
  }

  public boolean Get_bool_field(){
      System.out.println("The name_bool value is "+name_bool);
      return name_bool;
  }
  
  public void Set_long_field(long field){
      name_long = field;
      System.out.println("The name_long was set to "+name_long);
  }
  
  public static long Get_static_long_field(){
      System.out.println("The name_long value is "+static_name_long);
      return static_name_long;
  }

  public long Get_long_field(){
      System.out.println("The name_long value is "+name_long);
      return name_long;
  }
  
  public void Set_double_field(double field){
      name_double = field;
      System.out.println("The name_double was set to "+name_double);
  }
  
  public static double Get_static_double_field(){
      System.out.println("The name_double value is "+static_name_double);
      return static_name_double;
  }
  
  public double Get_double_field(){
      System.out.println("The name_double value is "+name_double);
      return name_double;
  }
  
  public void Set_float_field(float field){
      name_float = field;
      System.out.println("The name_float was set to "+name_float);
  }
  
  public static float Get_static_float_field(){
      System.out.println("The name_float value is "+static_name_float);
      return static_name_float;
  }

  public float Get_float_field(){
      System.out.println("The name_float value is "+name_float);
      return name_float;
  }
  
  public void Set_short_field(short field){
      name_short = field;
      System.out.println("The name_short was set to "+name_short);
  }

  public static short Get_static_short_field(){
      System.out.println("The name_short value is "+static_name_short);
      return static_name_short;
  }
  
  public short Get_short_field(){
      System.out.println("The name_short value is "+name_short);
      return name_short;
  }
  
  public void Set_byte_field(byte field){
      name_byte = field;
      System.out.println("The name_byte was set to "+name_byte);
  }
  
  public static byte Get_static_byte_field(){
      System.out.println("The name_byte value is "+static_name_byte);
      return static_name_byte;
  }

  public byte Get_byte_field(){
      System.out.println("The name_byte value is "+name_byte);
      return name_byte;
  }
  
  public void Set_object_field(Object field){
      name_object = field;
      System.out.println("The name_object was set to "+name_object);
  }
  
  public Object Get_object_field(){
      System.out.println("The name_object value is "+name_object);
      return name_object;
  }

  public static String Get_static_string_object(){
      System.out.println("String is: "+static_name_string);
      return static_name_string;
  }
  
  public void Set_char_field(char field){
      name_char = field;
      System.out.println("The name_char was set to "+name_char);
  }
  
  public static char Get_static_char_field(){
      System.out.println("The name_char value is "+static_name_char);
      return static_name_char;
  }
  
  public char Get_char_field(){
      System.out.println("The name_char value is "+name_char);
      return name_char;
  }
  
  public void Test1_method1(){
      System.out.println("Test1_method1 passed!");
  }

  public void Test1_method2(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method2 passed!");
  }

  public static void Test1_method2_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method2_static passed!");
  }

  public boolean Test1_method_bool(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_bool passed!");
      return bb;
  }

  public static boolean Test1_method_bool_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_bool_static passed!");
      return bb;
  }

  public byte Test1_method_byte(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_byte passed!");
      return by;
  }

  public static byte Test1_method_byte_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_byte_static passed!");
      return by;
  }

  public char Test1_method_char(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_char passed!");
      return ch;
  }

  public static char Test1_method_char_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_char_static passed!");
      return ch;
  }

  public short Test1_method_short(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_short passed!");
      return sh;
  }

  public static short Test1_method_short_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_short_static passed!");
      return sh;
  }

  public int Test1_method_int(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_int passed!");
      return in;
  }

  public static int Test1_method_int_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_int_static passed!");
      return in;
  }

  public long Test1_method_long(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_long passed!");
      return lg;
  }

  public static long Test1_method_long_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_long_static passed!");
      return lg;
  }

  public float Test1_method_float(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_float passed!");
      return fl;
  }

  public static float Test1_method_float_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_float_static passed!");
      return fl;
  }

  public double Test1_method_double(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_double passed!");
      return db;
  }

  public static double Test1_method_double_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_double_static passed!");
      return db;
  }

  public String Test1_method_string(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_string passed!");
      return str;
  }

  public static String Test1_method_string_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method_string_static passed!");
      return str;
  }

  public void Test1_method7(boolean bb, byte by, char ch, short sh, int in, long lg, float fl){
      System.out.println("Test1_method2 passed!");
  }

  public int Test1_method3(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3 passed!");
      return 121;
  }

  public int Test1_method3_override(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3 passed!");
      return 121;
  }

  private int Test1_method31(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method31 passed!");
      return 121;
  }

  private static int Test1_method31_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method31_static passed!");
      return 121;
  }

  protected int Test1_method32(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method32 passed!");
      return 121;
  }

  protected static int Test1_method32_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method32_static passed!");
      return 121;
  }

  int Test1_method33(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method33 passed!");
      return 121;
  }

  static int Test1_method33_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method33_static passed!");
      return 121;
  }

  public final int Test1_method3_final(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3_final passed!");
      return 121;
  }

  public static final int Test1_method3_final_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3_final_static passed!");
      return 121;
  }

  public synchronized int Test1_method3_sync(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3_sync passed!");
      return 121;
  }

  public static synchronized int Test1_method3_sync_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3_sync_static passed!");
      return 121;
  }

  public String Test1_method4(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method4 passed!");
      return "test";
  }

  public String[] Test1_method5(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method5 passed!");
      return strarr;
  }

  public static void Test1_method1_static(){
      System.out.println("Test1_method1_static passed!");
  }

  public static void Test1_method7_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl){
      System.out.println("Test1_method7_static passed!");
  }

  public static int Test1_method3_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method3_static passed!");
      return 121;
  }

  public static String Test1_method4_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method4_static passed!");
      return "test";
  }

  public static String[] Test1_method5_static(boolean bb, byte by, char ch, short sh, int in, long lg, float fl, double db, String str, String strarr[]){
      System.out.println("Test1_method5_static passed!");
      return strarr;
  }

  public final int Test1_public_final(int i){
      return i;
  }

  public static final int Test1_public_final_static(int i){
      return i;
  }

  public synchronized int Test1_public_sinc(int i){
      return i;
  }

  public static synchronized int Test1_public_sinc_static(int i){
      return i;
  }

  public int Test2_override(int i){
       return i+1;
  }

  public static int Test2_override_static(int i){
       return i+1;
  }

  public int Test1_thrown_excp(int i){
      int a[] = new int[2];
      a[4] = i;
      return 2+i;
  }

  public static int Test1_thrown_excp_static(int i){
      int a[] = new int[2];
      a[4] = i;
      return 2+i;
  }
}

