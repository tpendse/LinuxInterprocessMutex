#pragma once

#include <utility>
#include <string>
#include <mutex>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>   /* For mode constants */
#include <fcntl.h>      /* For O_* constants  */

/*
* This struct is what is mapped to memory.
* !! Do no add pointers to this struct, including nested ones !!
* !! Pointers allocated by one process might not be accessible to other processes !!
*/
struct shared_region_layout {
	std::mutex mutex;       // Mutex lock
	unsigned int counter;   // Counting concurrent usages
	long timestamp;         // Timestamp when created
};


/*
* - This class manages named memory mapped files. It maps and allocates 
*   shared_region_layout in the shared memory.
* - Lifetime management, i.e. construction destruction of the shared memory 
*   has to be done be the caller
* - Each SharedRegion instance stores an allocation to the specified shared 
*   memory region.
* - On destruction, the allocated memory is unmapped and freed, however, 
*   the underlying shared file is not deleted
* - To cleanup the shared file, the caller has to call Destroy() explicitly
* - If Destroy() called from another process, the allocated memory stays 
*   mapped until the specific instance is destroyed. However, the sharing will break
* ! This class is not meant to be exported from the final shared library
*/
class SharedRegion
{
private:
	const std::string     _name;
	shared_region_layout* _region;
	bool                  _isCreated;

public:
	SharedRegion(const char *name) : _name(name), _isCreated(false), _region(nullptr) {}

	virtual ~SharedRegion() {
		this->Unmap();
	}

	const std::string& Name() const { return _name; }
	const bool IsCreated() const { return _isCreated; }

	bool Create()
	{
		if (_isCreated)
			this->Destroy();

		auto fileDescriptor = shm_open(this->_name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
		if (fileDescriptor != -1)
		{
			(void)! ftruncate(fileDescriptor, sizeof(shared_region_layout));

			_region = (shared_region_layout*) mmap(
				NULL, 
				sizeof(shared_region_layout), 
				PROT_READ | PROT_WRITE, 
				MAP_SHARED, 
				fileDescriptor, 0);

			if (_region != MAP_FAILED) {
				_isCreated = true;
				// Safe to close the file descriptor
				close(fileDescriptor);
			}
			else
				_region = nullptr; // MAP_FAILED is not nullptr or 0x0, so setting it explicitly
		}

		return _isCreated;
	}

	shared_region_layout* Get() const
	{
		return _isCreated ? _region : nullptr;
	}

	void Unmap()
	{
		// Free up allocated mmap-ed memory
		if (_region)
		{
			munmap(_region, sizeof(shared_region_layout));
			_region = nullptr;
		}
	}

	void Destroy()
	{
		_isCreated = false;

		// Close the shared memory object first, then the memory map
		if (_region) {
			shm_unlink(this->_name.c_str());
		}
		this->Unmap();
	}
};
