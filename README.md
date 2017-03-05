# rivus
Rivus is a lightweight fiber/coroutine library, the I/O management of which is based on Epoll. It provides nearly all of the synchronizations which POSIX threads provides(inlcudes mutex, semaphore, read-write lock, condition variable), and their APIs are very similar.

Downloading and Building
------------------------
If you have installed git, you can clone the project by the following command:<br>
```
$ git clone https://github.com/tongren-wang/rivus.git
```
Before building the project, please make sure you have installed "CMake".<br>
Now, change directory to "rivus/", and run the following commands to build it:<br>
```
$ cmake --DCMAKE_BUILD_TYPE=release ./
$ make
```
After all is done, you can find library file `librivus.a` under the directory "rivus/libs/".<br>
