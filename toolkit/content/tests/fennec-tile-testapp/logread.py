#!/usr/bin/python
import re, sys

interesting_re = re.compile("(js_Execute|CallHook) ([^ ]+) ([^ ]+ )?([^ ]+ms)")
class Entry:
    def __init__(self, kind, depth, file, linenum, func, timetaken):
        self.kind = kind
        self.depth = depth
        self.file = file
        self.linenum = linenum
        self.func = func
        self.timetaken = timetaken
        self.calls = 0
        self.duration = 0

    def __str__(self):
        return " ".join(map(str,[self.kind, self.depth, self.file, self.linenum, self.func, self.timetaken]))
    
    def id(self):
        if self.kind == "js_Execute":
            return self.file
        else:
            if self.file and self.linenum:
                strout = "%s:%d" % (self.file, self.linenum)
                if self.func:
                    strout = "%s %s" % (self.func, strout)
                return strout
            elif self.func:
                return self.func
            else:
                print("No clue what my id is:"+self)
           
    def call(self, timetaken):
        self.calls += 1
        self.duration += timetaken

def parse_line(line):
    m = interesting_re.search(line)
    if not m:
        return None

    ms_index = line.find("ms")
    depth = m.start() - ms_index - 3
    kind = m.group(1)
    func = None
    file = None
    linenum = None
    if kind == "CallHook":
        func = m.group(2)
        file = m.group(3)
        colpos = file.rfind(":")
        (file,linenum) = file[:colpos], file[colpos+1:-1]
        if linenum == "0":
            linenum = None
        else:
            linenum = int(linenum)
        offset = 1
    else:
        file = m.group(3)
    
    timetaken = None
    try:
        timetaken = float(m.group(4)[:-2])
    except:
        return None
    return Entry(kind, depth, file, linenum, func, timetaken)

def compare(x,y):
    diff = x[1].calls - y[1].calls
    if diff == 0:
        return int(x[1].duration - y[1].duration)
    elif diff > 0:
        return 1
    elif diff < 0:
        return -1

def frequency(ls):
    dict = {}
    for item in ls:
        id = item.id()
        stat = None
        if not id in dict:
            stat = dict[id] = item
        else:
            stat = dict[id]
        stat.call(item.timetaken)

    ls = dict.items()
    ls.sort(compare)
    ls = filter(lambda (_,item): item.duration > 20, ls)
#    ls = filter(lambda (_,item): item.file and item.file.find("browser.js") != -1 and item.linenum <= 1223 and item.linenum >1067, ls)
    for key, item in ls:
        print(item.calls,key, str(item.duration)+"ms")

def go():
    file = sys.argv[1]
    
    ls = filter(lambda x: x != None, map(parse_line, open(file).readlines()))
    
    frequency(ls)
    print ls[0]

go()

