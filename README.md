#external_sort
Multithread external sorting realization for c++14 (gcc 4.8)

Build: `make`

Usage: `bin/external_sort input_file output_file [memory_limit [threads_count [no_of_ways_for_merge]]]`

`memory_limit` specifies memory pool for sorting in MB defaulting to 10 and always between 0 and 256

`threads_count` specifies number of thread used defaulting to 4

`no_of_ways_for_merge` specifies how many chunks will be used in one merge operation defaulting to 2.


To generate some test data and check out correctness use smth like this:
```
dd if=/dev/urandom of=in10MB bs=1M count=10
bin/external_sort in10MB out-onechunk 10 1
bin/external_sort in10MB out-2-2 1 2 2
bin/external_sort in10MB out-2-3 1 2 3
bin/external_sort in10MB out-2-4 1 2 4
diff out-2-2 out-2-3
diff out-2-2 out-2-4
```
