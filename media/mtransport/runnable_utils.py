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


def runnable_class_name(args, ret=False, member=True):
    if member:
        nm = "m"
    else:
        nm = "nm"

    if ret:
        class_suffix = "_ret"
        enum_specializer = "detail::ReturnsResult"
    else:
        class_suffix = ""
        enum_specializer = "detail::NoResult"

    return "runnable_args_%s_%d%s" % (nm, args, class_suffix), enum_specializer

def generate_class_template(args, ret = False, member = True):
    print "// %d arguments --"%args

    class_name, specializer = runnable_class_name(args, ret, member)
    base_class = "detail::runnable_args_base<%s>" % specializer

    if not ret:
        print "template<"+ gen_typenames(args, member) + "> class %s : public %s {" % (class_name, base_class)
    else:
        print "template<"+ gen_typenames(args, member) + ", typename R> class %s : public %s {" % (class_name, base_class)

    print " public:"

    if not ret:
        print "  %s(" % class_name + gen_args_type(args, member) + ") :"
        print "    " + gen_init(args, False, member) + "  {}"
    else:
        print "  %s(" % class_name + gen_args_type(args, member) + ", R *r) :"
        print "    " + gen_init(args, True, member) + "  {}"
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
        NM = "";
    else:
        NM = "NM";

    class_name, _ = runnable_class_name(args, False, member)

    print "// %d arguments --"%args
    print "template<" + gen_typenames(args, member) + ">"
    print "%s<" % class_name + gen_types(args, member) + ">* WrapRunnable%s("%NM + gen_args_type(args, member) + ") {"
    print "  return new %s<" % class_name + gen_types(args, member) + ">"
    print "    (" + gen_args(args, member) + ");"
    print "}"
    print

def generate_function_template_ret(args, member):
    if member:
        NM = "";
    else:
        NM = "NM";

    class_name, _ = runnable_class_name(args, True, member)

    print "// %d arguments --"%args
    print "template<" + gen_typenames(args, member) + ", typename R>"
    print "%s<" % class_name + gen_types(args, member) + ", R>* WrapRunnable%sRet("%NM + gen_args_type(args, member) + ", R* r) {"
    print "  return new %s<" % class_name + gen_types(args, member) + ", R>"
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



