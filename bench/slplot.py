#!/usr/bin/env python
# -*- coding: utf-8 -*-

# matplotlib/seaborn font garbage
import matplotlib.font_manager as fm
font0 = fm.FontProperties()
font0.set_family("monospace")
font0.set_name("M+ 1mn")
font0.set_weight("medium")

import seaborn as sns
sns.set_style("whitegrid", {'grid.linestyle': ':'})
sns.plt.rcParams.update({'mathtext.fontset' : 'custom',
                         'mathtext.rm' : 'Bitstream Vera Sans',
                         'mathtext.it' : 'Bitstream Vera Sans:italic',
                         'mathtext.bf' : 'Bitstream Vera Sans:bold',
                         'mathtext.bf' : 'Bitstream Vera Sans:bold',
                         'mathtext.tt' : 'mononoki',
                         'mathtext.cal' : 'MathJax_Caligraphic'})
sns.set_palette("Set2")

import datetime
import time

import collections
from collections import OrderedDict

import json

from functools import partial

import pprint
pp = pprint.PrettyPrinter(indent=2)


################################################################################

def ndigits(num):
    return len(str(num))

def tparse(fname, tag):
    tdata = {}
    with open(fname, "r") as timings:
        for timing in timings:
            timing = timing.rstrip()
            if not timing:
                continue
            tkeyvals = timing.split(',')
            tname = tkeyvals[0].split(':')[1]
            tdata[tname+tag] = {}
            for keyval in tkeyvals:
                tdatapair = keyval.split(':')
                if tdatapair[0] == "name":
                    continue
                tdata[tname+tag][tdatapair[0]] = tdatapair[1]
    return tdata

# get data from File
def get_fdata(bench, proc_set, size_set, bpsz_set, curve_type):
    fdata = {}
    for proc in proc_set:
        for size in size_set:
            for bpsz in bpsz_set:
                fname = fname_tmpl.format(bench, proc, size, bpsz)
                if curve_type == "proc":
                    tag = str(proc)
                    pad = max(map(ndigits, proc_set))
                elif curve_type == "size":
                    tag = str(size)
                    pad = max(map(ndigits, size_set))
                elif curve_type == "bpsz":
                    tag = str(bpsz)
                    pad = max(map(ndigits, bpsz_set))
                fdata[fname] = tparse(fname, tag.zfill(pad))
                for impl in fdata[fname]:
                    # possible x axes units
                    fdata[fname][impl]["proc"] = proc
                    fdata[fname][impl]["size"] = size
                    fdata[fname][impl]["bpsz"] = bpsz

    print(json.dumps(fdata, indent=2))
    return fdata

# make data for Plot
def make_pdata(fdata, stat_type, x_axis):
    pdata = {}
    for lkey, log in fdata.items():
        for key, stat_set in log.items():
            if key not in pdata:
                pdata[key] = [];
            for i in stat_set:
                if i == stat_type:
                    pdata[key] += [ (stat_set[x_axis], float(stat_set[i])) ]
    for pkey in pdata:
        pdata[pkey] = sorted(pdata[pkey])
    return pdata

def rpvs(pkey, size):
    return str(size) in pkey
def rpvs_all(pkey):
    return "cont" in pkey
def fpvs_all(pkey):
    return "cont" in pkey
def rsvt(pkey):
    return "16" in pkey
def fsvt(pkey):
    return True
def fbvs(pkey):
    return "cont006291456" in pkey

selector = {
    "reduce_size-v-time": rsvt,
    "reduce_procs-v-speed-s": partial(rpvs, size=2**19 * 3),
    "reduce_procs-v-speed-m": partial(rpvs, size=2**21 * 3),
    "reduce_procs-v-speed-l": partial(rpvs, size=2**23 * 3),
    "reduce_procs-v-speed-a": rpvs_all,
    "foreach_size-v-time": fsvt,
    "foreach_procs-v-speed-a": fpvs_all,
    "foreach_bpbits-v-speed": fbvs
}
        

################################################################################

# filename path/template
log_path = "logs_28-09"
fname_tmpl = log_path + "/{0:s}_{1:d}_{2:d}_{3:d}.log"

# key of dispatch table, etc
#bench_name = "reduce_size-v-time"
#bench_name = "reduce_procs-v-speed-s"
#bench_name = "reduce_procs-v-speed-m"
#bench_name = "reduce_procs-v-speed-l"
#bench_name = "reduce_procs-v-speed-a"
bench_name = "foreach_size-v-time"
#bench_name = "foreach_procs-v-speed-a"
#bench_name = "foreach_bpbits-v-speed"
if "reduce" in bench_name:
    test_name = "reduce"
    op = "+"
elif "foreach" in bench_name:
    test_name = "foreach"
    op = "*"

if "procs-v-speed" in bench_name:
    curve_type = "size"
    x_axis = "proc"
elif "size-v-time" in bench_name:
    curve_type = "proc"
    x_axis = "size"
elif "bpsz-v-speed" in bench_name:
    curve_type = "size"
    x_axis = "bpsz"

# name of saved file
graph_name = "graphs_2016-09-28/" + bench_name

# processor counts
proc_set = [1, 2, 4, 8, 16]
proc_val = 16
procs = proc_set

# vector sizes
size_set = map(lambda x: x * (2**15 * 3), [2**0, 2**2, 2**4, 2**6, 2**8])
size_val_s = 2**19 * 3
size_val_m = 2**21 * 3
size_val_l = 2**23 * 3
if "procs-v-speed-a" in bench_name:
    sizes = size_set
elif bench_name == "reduce_procs-v-speed-s":
    sizes = [size_val_s]
elif bench_name == "reduce_procs-v-speed-m":
    sizes = [size_val_m]
elif bench_name == "reduce_procs-v-speed-l":
    sizes = [size_val_l]
elif "size-v-time" in bench_name:
    sizes = size_set
else:
    sizes = [size_val_m]

# bit partition sizes
bpsz_set = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
bpsz_val = 10
if bench_name == "foreach_bpbits-v-speed":
    bpszs = bpsz_set
else:
    bpszs = [bpsz_val]

# unresolved depth values (unused as of now)
unre_set = [2**0, 2**1, 2**2, 2**3, 2**4, 2**5, 2**6]
unre_val = 8


################################################################################

# file data
print("Reading data from disk...")
fdata = get_fdata(test_name, procs, sizes, bpszs, curve_type)

# plot data
print("Importing data...")
pdata = make_pdata(fdata, "min", x_axis)
pp.pprint(pdata)
pdata = { k: zip(*v) for k,v in pdata.items() }

for k,v in pdata.items():
    print (k,v)

def keysort(s):
    if "reduce" in s[0]:
        keyorder = {k:v for v,k in enumerate(["seq", "vec", "cont", "async", "omp"])}
        for k,v in keyorder.items():
            if k in s[0]:
                return v
        return keyorder.size()
    print s[0]
    return s[0]

pdata = OrderedDict(sorted(pdata.items(), key=keysort))

# plotting
print("Plotting data...")
figtext = ''
if "procs-v-speed" in bench_name:
    for pkey, pvals in pdata.items():
        if not selector[bench_name](pkey):
            continue
        plab = "".join(i for i in pkey if not i.isdigit())
        if "cont" in plab:
            plab = "cont"
        elif "stdv" in plab:
            plab = "stdv"
        ptag = "".join(i for i in pkey if i.isdigit())
        plab += ", " + ptag.strip("0") + " elements"
        if "reduce" in bench_name:
            baseline = pdata["vec" + ptag][1][0]
        elif "foreach" in bench_name:
            baseline = pdata["stdv_foreach" + ptag][1][0]
        if (("seq" in pkey or "vec" in pkey) and "1" in pkey):
            sns.plt.axhline(pvals[1][0] / baseline, color='#777777', linestyle='-', label=plab)
        else:
            sns.plt.plot(pvals[0], [y / baseline for y in pvals[1]], marker='d', label=plab)

    handles, labels = sns.plt.gca().get_legend_handles_labels()
    labels, handles = zip(*sorted(zip(labels, handles), key=lambda t: len(t[0])))
    leg = sns.plt.legend(handles, labels, loc='upper right', fancybox=True, frameon=True, prop=font0)
    leg.get_frame().set_alpha(1.0)
    sns.plt.xlim(1, 4)
    if "reduce" in bench_name:
        sns.plt.ylim(0.0, 1.3)
    elif "foreach" in bench_name:
        sns.plt.ylim(0.3, 1.5)
    sns.plt.title('Scaling (' + test_name + ' with ' + op + ')')
    sns.plt.xlabel('# threads')
    sns.plt.ylabel('relative speedup')
    sns.plt.xticks(proc_set)
    figtext = 'bench: ' + test_name + ', branching factor: 10'
elif "size-v-time" in bench_name:
    for pkey, pvals in pdata.items():
        if not selector[bench_name](pkey):
            print pkey
            continue
        plab = "".join(i for i in pkey if not i.isdigit())
        print plab
        if "cont" in plab:
            plab = "cont"
        elif "stdv" in plab:
            plab = "stdv"
            if "1" not in pkey:
                continue
        ptag = "".join(i for i in pkey if i.isdigit())
        plab += ", " + ptag.strip("0") + " procs"
        sns.plt.plot(pvals[0], pvals[1], marker='d', label=plab)

    handles, labels = sns.plt.gca().get_legend_handles_labels()
    labels, handles = zip(*sorted(zip(labels, handles), key=lambda t: len(t[0])))
    leg = sns.plt.legend(handles, labels, loc='upper left', fancybox=True, frameon=True, prop=font0)
    leg.get_frame().set_alpha(1.0)
    if "reduce" in bench_name:
        sns.plt.ylim(0.01, 300)
    elif "foreach" in bench_name:
        sns.plt.ylim(0.24, 400)
    sns.plt.xscale('log', basex=2)
    sns.plt.yscale('log', basey=10)
    sns.plt.title('Runtime (' + test_name + ' with ' + op + ')')
    sns.plt.xlabel('# elements')
    sns.plt.ylabel('time (s)')
    figtext = 'bench: ' + test_name + ', branching factor: 10'
elif "bpsz-v-speed" in bench_name:
    for pkey, pvals in pdata.items():
        if not selector[bench_name](pkey):
            print pkey
            continue
        plab = "".join(i for i in pkey if not i.isdigit())
        print plab
        if "cont" in plab:
            plab = "cont"
        elif "stdv" in plab:
            plab = "stdv"
        ptag = "".join(i for i in pkey if i.isdigit())
        plab += ", " + ptag.strip("0") + " procs"
        sns.plt.plot(pvals[0], pvals[1], marker='d', label=plab)

    handles, labels = sns.plt.gca().get_legend_handles_labels()
    labels, handles = zip(*sorted(zip(labels, handles), key=lambda t: len(t[0])))
    leg = sns.plt.legend(handles, labels, loc='upper left', fancybox=True, frameon=True, prop=font0)
    leg.get_frame().set_alpha(1.0)
    if "reduce" in bench_name:
        sns.plt.ylim(0.02, 150)
    elif "foreach" in bench_name:
        sns.plt.ylim(0.18, 1000)
    sns.plt.xscale('log', basex=2)
    sns.plt.yscale('log', basey=10)
    sns.plt.title('Runtime (' + test_name + ' with ' + op + ')')
    sns.plt.xlabel('# elements')
    sns.plt.ylabel('time (s)')
    figtext = 'bench: ' + test_name + ', branching factor: 10'


# old options
#sns.plt.ylim(0.35, 1.75)
#sns.plt.xlim(0, 16)
#sns.plt.gca().xaxis.grid(False)
#sns.plt.gca().yaxis.set_major_locator(mpl.ticker.MultipleLocator(0.25))
#sns.plt.xticks(range(0, 16))

# 2
#sns.plt.title('Effect of BP_SIZE on runtime')
#sns.plt.xlabel(r'BP_SIZE ($\log_2~$of branches/node)')
#sns.plt.ylabel('relative speedup')
#t = sns.plt.figtext(0.512, 0.12, 'bench: foreach, #elements: 6291456, #threads: 4',
#                    fontsize=10, fontproperties=font0, ha='center')

t = sns.plt.figtext(0.512, 0.12, figtext, fontsize=10, fontproperties=font0,
                    ha='center')
t.set_bbox(dict(color='white', alpha=1.0, edgecolor='grey'))

print("Saving plot...")
sns.plt.savefig(graph_name + ".png")
sns.plt.close()

"""
# plotting
sns.plt.axhline(1, color='#777777', linestyle='-', label="std::vector<double>")
    if (("seq" in pkey or "vec" in pkey) and "1" in pkey) or \
       (("seq" not in pkey and "vec" not in pkey) and "4" in pkey):
        plabel = "".join(i for i in pkey if not i.isdigit())
        if "seq" not in pkey and "vec" not in pkey:
            if "avx" in pkey:
                ptag = "256 bits"
            elif "omp" in pkey:
                ptag = "4 cores"
            else:
                ptag = "".join(i for i in pkey if i.isdigit()) + " threads"
            plabel += ", " + ptag
        pvals.sort()
        sns.plt.plot(size_set, pvals, marker='d', label=plabel)
    if (("stdv" in pkey) and "4" in pkey) or \
        ("stdv" not in pkey):
        if "stdv" in pkey:
            plabel = "std::vector<double>"
        elif "cont" in pkey:
            ptag = ''.join(i for i in pkey if i.isdigit())
            plabel = "cts::ctvector<double>, threads=" + ptag
        pvals.sort()
        sns.plt.plot(size_set, pvals, marker='d', label=plabel)
"""
