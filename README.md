# contentious

### containers that mediate parallelism

This package provides a namespace (`contentious`) that contains the following template classes:

  * a persistent vector (psvector), 
  * a transient vector (trvector), 
  * a "contentious" vector (ctvector) that allows for multiple readers and writers on its values;

the following functions:

  * reduce, which performs a reduce operation on a ctvector, 
  * foreach, which performs a foreach operation, 
  * stencil, which provides a more general interface for performing concurrent and parallel operations, 
  * some example binary operations (plus, mult) that can be used with the above, 
  * some example index mappings (all-to-one, identity, offset) that can be used with the above

the following constants:

  * HWCONC: the number of concurrent threads to spawn when performing automated parallel operations 
  * BPBITS: the number of bits to use to store branches or leaves in the bit-partitioned trie used to implement the above data structures;

And the following types:
  * a "binary operator" type for use with the above functions, 
  * a "index mapping" type for use with the aforementioned functions 
  * a "task" type that gives the user more flexibility over how the concurrent threads behave if desired, 
  * a "partition" type that lets the user choose how the vectors are partitioned as a function of each thread's integer ID

in addition to a test suite and microbenchmark suite, the latter of which compares the performance of this method against common parallelism techniques, such as OpenMP, C++'s async feature, and the use of AVX registers.

The goal of this package is to provide a proof of concept for managing data dependencies among values in a vector such that those vectors can be parallelization.
