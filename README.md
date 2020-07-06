run_as_exe
==========

Normally, the /proc/self/exe link can only be changed with `CAP_SYS_ADMIN`.

This is a PoC to demonstrate that the /proc/self/exe link can be changed
without any special privileges. All it uses is ptrace and execve.

Example:

```
$ make -j4
# Lots of output. The musl library gets compiled in.

$ ./run_as_exe
run_as_exe: Usage: ./run_as_exe <masquerade_exe_path> cmd...

$ ./run_as_exe false /bin/ls -lh /proc/self/exe
lrwxrwxrwx 1 user user 0 Jul  6 15:53 /proc/self/exe -> /bin/false

$ ./run_as_exe sudo /bin/bash -c 'echo bash is running as $(readlink /proc/$$/exe)'
bash is running as /usr/bin/sudo
```

Implementation
--------------

This is how it works:
1) The current process referred to as the _victim_ forks a child process referred to as the _surgeon_
2) The victim invokes `prctl(PR_SET_PTRACER, surgeon_pid)` to allow the surgeon to operate
3) The surgeon ptraces the victim, traps execve(), and lets the victim go
4) The victim invokes `execve(masquerade_exe_path, cmd)`
5) The surgeon traps right after the victim's execve().
   It uses the [compel](https://criu.org/Compel) library to inject a parasite code, and invokes it
6) The parasite code (see parasite.c) loads the ELF pointed by `cmd`, and runs it.
The parasite code uses the musl library to facilitate ELF loading

Limitations
-----------

Only the x86-64 platform is supported due to lack of time.

License
--------

`run_as_exe` is licensed under the
[Apache 2.0 license](https://www.apache.org/licenses/LICENSE-2.0).
