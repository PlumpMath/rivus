# rivus
Rivus is a lightweight and high performance fiber/coroutine library, which I/O management is based on Epoll. It provides nearly all of the synchronization mechanism which POSIX threads provides(inlcudes mutex, semaphore, read-write lock, condition variable), and their APIs are very similar.

Downloading and Building
------------------------
If you have installed git, you can clone the project by the following command:<br>
```
$ git clone https://github.com/tongren-wang/rivus.git
```
Before building the project, please make sure you have installed these tools: `automake`, `autoconf`, `m4`.<br>
Now, change directory to the project's root directory "rivus/", and run the followint commands to build it:<br>
```
$ aclocal
$ autoconf
$ autoheader
$ automake -a
$ automake
$ ./configure
$ make
```
After all is done, you can find library file `librivus.a` under the directory "rivus/lib/".<br>
And under the other directory "rivus/example/", you can find executable files:  'server', 'client', 'mutex', 'sem', 'rwlock', 'cond'.<br>
These executable files are the output of the example code, you can run them to check if the library work correctly.
