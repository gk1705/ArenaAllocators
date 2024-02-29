# ArenaAllocators
Implementation of three types of arena allocators to efficiently allocate and deallocate memory. These allocators reserve a large block or "arena" of memory upfront and then satisfy individual allocation requests from this pre-allocated space to avoid costly heap allocations.

## Core Components:
* **GeneralAllocator:**
	- An allocator that allows for all types of allocations by creating chunks of different sizes. If they are adjacent, chunks are merged when memory is freed. 
	- Features functionality to defragment memory by copying or moving data to the front of the reserved memory and merging the remaining chunks.
	- Depends on memory handles that update when defragmentation occurs.
* **StackAllocator:**
	- Allows for memory allocation like a stack. User can request a marker which they can reset the stack to.
* **PoolAllocator:**
	- Reserves fixed size memory chunks for one object type.
	
## Dependencies
* **Visual Studio:**
	- project is set up to be developed with Microsoft Visual Studio