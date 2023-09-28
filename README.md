<div style="margin-left: auto; margin-right: auto; width: 70%;">
<h1 style="text-align: center;">Interprocess Mutex in Linux</h1>

### Comparison to Windows
* On Windows, Microsoft's libraries provide `CreateMutexA`, `WaitForSingleObject` & `ReleaseMutex` to allow named interprocess mutexes. Linux does not have anything in-built.
* On Linux, we need to place the `std::mutex` in shared memory which can be accessible by all processes that can link to it.

### Shared Regions
* The `SharedRegion` manages the creation, access and deletion of a struct linked to a memory-mapped file. This class, however, does not manage the lifecycle of the shared memory. Invoking the lifecycle methods is the responsibility of the caller.
* The class operates in the following way:
    * The memory layout of the shared region is defined by the struct `shared_region_layout`.
    * On construction the name of the shared region is stored. This is the name of the linked file that will be created under `/run/shm/` (symbolic link to `/dev/shm/`).
    * On creation `SharedRegion::Create()` will create a new or open an existing POSIX shared memory object with the given name and return the file descriptor.
    * Using the file descriptor, we then allocate an instance of `shared_region_layout` mapped to the shared memory object.
    * On destruction, the allocated mapped memory is freed. The memory-mapped files are not deleted. To delete the file, the caller must invoke `SharedRegion::Delete()` explicitly.
    * Since destruction and unmapping are different behaviours, if another process destroys the shared memory, all other processes are unaffected. However, the sharing of states between the processes will end.

### Shared Mutex
* The `SharedMutex` class manages the lifecycle of a memory-mapped mutex struct. It uses the `SharedRegion` to create and access the memory-mapped file.
* The `shared_region_layout` contains the following attributes:
    * `std::mutex`: the actual lock
    * `counter`: a reference count
    * `timestamp`: milliseconds since the epoch when the shared region was created
* On construction,
    * A new `SharedRegion` is created - ready to be shared by other processes simultaneously. Locking and unlocking the mutex will be visible by all other processes.
    * The `timestamp` is checked. If the timestamp is too old, this might indicate a shared region that was not properly destroyed. In this case, the shared region is deleted and a new one is created.
    * If the a new region is created (i.e. `timestamp` is default value 0), the current time in milliseconds is set as the `timestamp`.
    * The reference count is increased by 1.
* On destruction or release,
    * The reference count is decreased by 1.
    * If the reference count is 0, the shared region is destroyed.

### Restrictions
* If a process crashes with a `SharedMutex` locked, the shared region is not destroyed. This can leave zombie memory-mapped files in the `/dev/shm/` folder which still have their mutexes locked. If the lock isn't released before the timestamp becomes stale, we need to detect or explicitly delete such files.
* The `shared_region_layout` cannot have pointer attributes, even nested ones. This is because pointers assigned from one process' memory will not be visible/addressable by others.

### Testing
* Due to the multi-process nature of the setup, we could not use Catch2 tests. We created a new bare-bones unit testing framework `LinuxMultiProcessTests`
* To test behaviour with multiple processes many tests use `fork()` to fork a new child process.
* It is important to not do the following:
    * Assert anything in the child process.
    * Assert in parent process before ending/killing the child process.

</div>