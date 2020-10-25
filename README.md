This repository contains a micro benchmarks suite for several CAF features. The
suite makes it easy to compare different CAF versions. For example, to run the
benchmarks for 0.16.5 and 0.17.6, simply configure and run two builds:

```sh
./configure --tag=0.16.5
./configure --tag=0.17.6
make run
```

Note that a *tag* may be a git tag, commit sha or branch name. However, when
passing a commit sha or branch name, the build scaffold assumes a recent CAF
version.
