#! /bin/sh

make && ./runner && \
    sed -i -e 's/HWCONC = 1/HWCONC = 2/g' ../contentious/contentious_constants.h && make && ./runner && \
    sed -i -e 's/HWCONC = 2/HWCONC = 4/g' ../contentious/contentious_constants.h && make && ./runner && \
    sed -i -e 's/HWCONC = 4/HWCONC = 1/g' ../contentious/contentious_constants.h
