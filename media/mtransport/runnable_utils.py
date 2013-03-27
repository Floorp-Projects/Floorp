# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
MAX_ARGS = 15

boilerplate = "/* This Source Code Form is subject to the terms of the Mozilla Public\n\
 * License, v. 2.0. If a copy of the MPL was not distributed with this\n\
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */\n"

def gen_args_type(args, member):
    if member:
        ret = ["C o"]
    else:
        ret = []
    ret.append("M m")
    for arg in range(0, args):
        ret.append("A%d a%d"%(arg, arg))
    return ", ".join(ret)

def gen_args(args, member):
    if member:
        ret = ["o"]
    else:
        ret = []
    ret.append("m")
    for arg in range(0, args):
        ret.append("a%d"%(arg))
    return ", ".join(ret)

def gen_args_(args):
    ret = []
    for arg in range(0, args):
        ret.append("a%d_"%(arg))
    return ", ".join(ret)

def gen_init(args, r = False, member = False):
    if member:
        ret = ["o_(o)"]
    else:
        ret = []
    ret.append("m_(m)")

    if r:
        ret.append("r_(r)")

    for arg in range(0, args):
        ret.append("a%d_(a%d)"%(arg, arg))
    return ", ".join(ret)

def gen_typenames(args, member):
    if member:
        ret = ["typename C"]
    else:
        ret = []
    ret.append("typename M")

    for arg in range(0, args):
        ret.append("typename A%d"%(arg))
    return ", ".join(ret)

def gen_types(args, member):
    if member:
        ret = ["C"]
    else:
        ret = []
    ret.append("M")
    for arg in range(0, args):
        ret.append("A%d"%(arg))
    return ", ".join(ret)


def generate_class_template(args, ret = False, member = True):
    print "// %d arguments --"%args
    if member:
        nm = "m"
    else:
        nm = "nm"

    if not ret:
        print "template<"+ gen_typenames(args, member) + "> class runnable_args_%s_%d : public runnable_args_base {"%(nm, args)
    else:
        print "template<"+ gen_typenames(args, member) + ", typename R> class runnable_args_%s_%d_ret : public runnable_args_base {"%(nm, args)

    print " public:"

    if not ret:
        print "  runnable_args_%s_%d("%(nm, args) + gen_args_type(args, member) + ") :"
        print "    " + gen_init(args, False, member) + "  {}"
    else:
        print "  runnable_args_%s_%d_ret("%(nm, args) + gen_args_type(args, member) + ", R *r) :"
        print "    " + gen_init(args, True, member) + "  {}"
        print "  virtual bool returns_value() const { return true; }"
    print
    print "  NS_IMETHOD Run() {"
    if ret:
        print "    *r_ =",
    else:
        print "   ",
    if member:
        print "((*o_).*m_)(" + gen_args_(args) + ");"
    else:
        print "m_(" + gen_args_(args) + ");"

    print "    return NS_OK;"
    print "  }"
    print
    print " private:"
    if member:
        print "  C o_;"
    print "  M m_;"
    if ret:
        print "  R* r_;"
    for arg in range(0, args):
        print "  A%d a%d_;"%(arg, arg)
    print "};"
    print
    print
    print

def generate_function_template(args, member):
    if member:
        nm = "m"
        NM = "";
    else:
        nm = "nm"
        NM = "NM";
        
    print "// %d arguments --"%args
    print "template<" + gen_typenames(args, member) + ">"
    print "runnable_args_%s_%d<"%(nm, args) + gen_types(args, member) + ">* WrapRunnable%s("%NM + gen_args_type(args, member) + ") {"
    print "  return new runnable_args_%s_%d<"%(nm, args) + gen_types(args, member) + ">"
    print "    (" + gen_args(args, member) + ");"
    print "}"
    print

def generate_function_template_ret(args, member):
    if member:
        nm = "m"
        NM = "";
    else:
        nm = "nm"
        NM = "NM";
    print "// %d arguments --"%args
    print "template<" + gen_typenames(args, member) + ", typename R>"
    print "runnable_args_%s_%d_ret<"%(nm, args) + gen_types(args, member) + ", R>* WrapRunnable%sRet("%NM + gen_args_type(args, member) + ", R* r) {"
    print "  return new runnable_args_%s_%d_ret<"%(nm, args) + gen_types(args, member) + ", R>"
    print "    (" + gen_args(args, member) + ", r);"
    print "}"
    print


print boilerplate
print

for num_args in range (0, MAX_ARGS):
    generate_class_template(num_args, False, False)
    generate_class_template(num_args, True, False)
    generate_class_template(num_args, False, True)
    generate_class_template(num_args, True, True)


print
print
print

for num_args in range(0, MAX_ARGS):
    generate_function_template(num_args, False)
    generate_function_template_ret(num_args, False)
    generate_function_template(num_args, True)
    generate_function_template_ret(num_args, True)



