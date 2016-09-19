#!/usr/bin/env python
# -*- coding: utf-8 -*-

from functools import partial

import pprint
pp = pprint.PrettyPrinter(indent=2)
# matplotlib/seaborn font garbage
import matplotlib.font_manager as fm
font0 = fm.FontProperties()
font0.set_family("monospace")
font0.set_name("mononoki")
import seaborn as sns
sns.set_style("whitegrid", {'grid.linestyle': ':'})
sns.plt.rcParams.update({'mathtext.fontset' : 'custom',
                         'mathtext.rm' : 'Bitstream Vera Sans',
                         'mathtext.it' : 'Bitstream Vera Sans:italic',
                         'mathtext.bf' : 'Bitstream Vera Sans:bold',
                         'mathtext.bf' : 'Bitstream Vera Sans:bold',
                         'mathtext.tt' : 'mononoki',
                         'mathtext.cal' : 'MathJax_Caligraphic'})

#sns.set_style("ticks")
sns.set_palette("Set2")

import datetime
import time

import collections
import math

import json
import numpy as np

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
def get_fdata(bench, proc_set, size_set, bpsz_set, x_axis):
    fdata = {}
    for proc in proc_set:
        for size in size_set:
            for bpsz in bpsz_set:
                fname = fname_tmpl.format(bench, proc, size, bpsz)
                if x_axis == "proc":
                    tag = str(proc)
                    pad = max(map(ndigits, proc_set))
                elif x_axis == "size":
                    tag = str(size)
                    pad = max(map(ndigits, size_set))
                elif x_axis == "pbsz":
                    tag = str(bpsz)
                    pad = max(map(ndigits, bpsz_set))
                fdata[fname] = tparse(fname, tag.zfill(pad))
                for impl in fdata[fname]:
                    fdata[fname][impl]["proc"] = proc 
    #print(json.dumps(fdata, indent=2))
    return fdata

# make data for Plot
def make_pdata(fdata, stat_type):
    pdata = {}
    for lkey, log in fdata.items():
        for key, stat_set in log.items():
            if key not in pdata:
                pdata[key] = [];
            for i in stat_set:
                if i == stat_type:
                    pdata[key] += [ (stat_set["proc"], float(stat_set[i])) ]
    for pkey in pdata:
        pdata[pkey] = sorted(pdata[pkey])
    return pdata

def rpvs(pkey, size):
    return str(size) in pkey
def rpvs_all(pkey):
    return "cont" in pkey
def fpvs_all(pkey):
    return "cont" in pkey
def svt(pkey):
    return true
def fbvs(pkey):
    return "cont006291456" in pkey

selector = {
    "size-v-time": svt,
    "reduce_procs-v-speed-s": partial(rpvs, size=2**19 * 3),
    "reduce_procs-v-speed-m": partial(rpvs, size=2**21 * 3),
    "reduce_procs-v-speed-l": partial(rpvs, size=2**23 * 3),
    "reduce_procs-v-speed-a": rpvs_all,
    "foreach_procs-v-speed-a": fpvs_all,
    "foreach_bpbits-v-speed": fbvs
}
        

################################################################################

fname_tmpl = "log/{0:s}_{1:d}_{2:d}_{3:d}.log"
proc_set = [1, 2, 4]
size_set = map(lambda x: x * (2**15 * 3), [2**0, 2**2, 2**4, 2**6, 2**8, 2**10])
#bpsz_set = [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
bpsz_set = [10]
unre_set = [2**0, 2**1, 2**2, 2**3, 2**4, 2**5, 2**6]


print size_set
fdata = get_fdata("reduce", proc_set, size_set, bpsz_set, "size")
pdata = make_pdata(fdata, "min")
pp.pprint(pdata)

pdata = { k: zip(*v) for k,v in pdata.items() }

for k,v in pdata.items():
    print v

# plotting
#sns.plt.axhline(1, color='#777777', linestyle='-', label="std::vector<double>")
for pkey, pvals in pdata.items():
    if not selector["reduce_procs-v-speed-s"](pkey):
        continue
    print pkey
    plab = "".join(i for i in pkey if i.isalpha())
    ptag = "".join(i for i in pkey if i.isdigit())
    print ptag
    plab += ", " + ptag.strip("0") + " elements"
    baseline = pdata["vec" + ptag][1][0]
    sns.plt.plot(pvals[0], [y / baseline for y in pvals[1]], marker='d', label=plab)

handles, labels = sns.plt.gca().get_legend_handles_labels()
labels, handles = zip(*sorted(zip(labels, handles), key=lambda t: len(t[0])))
leg = sns.plt.legend(handles, labels, loc='upper right', fancybox=True, frameon=True, prop=font0)
leg.get_frame().set_alpha(1.0)

#sns.plt.xscale('log', basex=2)
#sns.plt.yscale('log', basey=10)
#sns.plt.ylim(0.35, 1.75)
sns.plt.ylim(0.5, 1.3)
sns.plt.xlim(1, 4)
#sns.plt.ylim(0, 275)
#sns.plt.gca().xaxis.grid(False)
#sns.plt.gca().yaxis.set_major_locator(mpl.ticker.MultipleLocator(0.25))
#sns.plt.xticks(range(0, 16))
# 1
sns.plt.title('Scaling (reduce with +)')
sns.plt.xlabel('# threads')
sns.plt.ylabel('relative speedup')
t = sns.plt.figtext(0.512, 0.12, 'bench: reduce, branching factor: 10',
                    fontsize=10, fontproperties=font0, ha='center')
# 2
sns.plt.title('Effect of BP_SIZE on runtime')
sns.plt.xlabel(r'BP_SIZE ($\log_2~$of branches/node)')
sns.plt.ylabel('relative speedup')
t = sns.plt.figtext(0.512, 0.12, 'bench: foreach, #elements: 6291456, #threads: 4',
                    fontsize=10, fontproperties=font0, ha='center')
# 2
sns.plt.title('Runtime (foreach with *)')
sns.plt.xlabel('# elements')
sns.plt.ylabel('time (s)')
t = sns.plt.figtext(0.512, 0.12, 'bench: foreach, #elements: 6291456, #threads: 4',
                    fontsize=10, fontproperties=font0, ha='center')

def rpvs(pkey, size):
    return str(size) in pkey
def rpvs_all(pkey):
    return "cont" in pkey
def fpvs_all(pkey):
    return "cont" in pkey
def svt(pkey):
    return true
def fbvs(pkey):
    return "cont006291456" in pkey

selector = {
    "size-v-time": svt,
    "reduce_procs-v-speed-s": partial(rpvs, size=2**19 * 3),
    "reduce_procs-v-speed-m": partial(rpvs, size=2**21 * 3),
    "reduce_procs-v-speed-l": partial(rpvs, size=2**23 * 3),
    "reduce_procs-v-speed-a": rpvs_all,
    "foreach_procs-v-speed-a": fpvs_all,
    "foreach_bpbits-v-speed": fbvs
}
        
t.set_bbox(dict(color='white', alpha=1.0, edgecolor='grey'))

sns.plt.savefig('newest.png')
sns.plt.close()

#   pdata_bpszs = []
#   for pkey, pvals in pdata.items():
#       if "stdv" in pkey:
#           continue
#       ptag = "".join(i for i in pkey if i.isdigit())
#       pdata_bpszs += [(int(ptag), pvals[0]/pdata["stdv_reduce10"][0])]

#   pdata_bpszs.sort()
#   print pdata_bpszs
#   x_bpszs, y_bpszs = zip(*pdata_bpszs)
#   sns.plt.plot(x_bpszs, y_bpszs, marker='d', label="cts::ctvector<double>")

"""
# plotting
sns.plt.axhline(1, color='#777777', linestyle='-', label="std::vector<double>")
for pkey, pvals in pdata.items():
    if "stdv" in pkey:
        continue
    plabel = "ctvec"
    ptag = "".join(i for i in pkey if i.isdigit())
    plabel += ", " + str(2**int(ptag)) + " branches/node"
    pvals.sort()
    pvals.reverse()
    print(pvals)
    sns.plt.plot([ptag], pvals, marker='d', label=plabel)
    if "stdv" in pkey:
        continue
    #plabel = "".join(i for i in pkey if not i.isdigit())
    plabel = "cts::ctvector<double>"
    ptag = "".join(i for i in pkey if i.isdigit())
    plabel += ", " + ptag + " elements"
    pvals.sort()
    pvals.reverse()
    baseline = pdata["stdv_reduce" + ptag][0];
    pvals_speedup = [y / baseline for y in pvals]
    print(proc_set)
    print(pvals_speedup)
    sns.plt.plot(proc_set, pvals_speedup, marker='d', label=plabel)
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

#sns.plt.xscale('log', basex=2)
#sns.plt.yscale('log', basey=10)
#sns.plt.ylim(0.35, 1.75)
#sns.plt.ylim(0.18, 1000)
#sns.plt.xlim(0, 16)
#sns.plt.ylim(0, 275)
#sns.plt.gca().xaxis.grid(False)
sns.plt.gca().yaxis.set_major_locator(mpl.ticker.MultipleLocator(0.25))
sns.plt.xticks(range(0, 16))

handles, labels = sns.plt.gca().get_legend_handles_labels()
labels, handles = zip(*sorted(zip(labels, handles), key=lambda t: len(t[0])))
leg = sns.plt.legend(handles, labels, loc='upper right', fancybox=True, frameon=True, prop=font0)
leg.get_frame().set_alpha(1.0)

sns.plt.title('Effect of BP_SIZE on runtime')
sns.plt.xlabel(r'BP_SIZE ($\log_2~$of branches/node)')
sns.plt.ylabel('relative speedup')
t = sns.plt.figtext(0.512, 0.12, 'bench=reduce, #elements=6291456, #threads=4',
                    fontsize=10, fontproperties=font0, ha='center')
t.set_bbox(dict(color='white', alpha=1.0, edgecolor='grey'))

#sns.plt.savefig('cont-foreach_sz-vs-time.png')
#sns.plt.savefig('cont-foreach_bpsz-vs-time.png')
sns.plt.savefig('cont-reduce-test.png')
sns.plt.close()
"""
