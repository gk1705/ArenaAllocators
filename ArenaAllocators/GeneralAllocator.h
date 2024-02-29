#pragma once

#include <vector>
#include <array>
#include <algorithm>
#include <compare>
#include <cstddef>
#include <map>

template <typename T>
concept MoveConstructible = requires(T t)
{
    T(std::move(t));
};

class GeneralAllocator;

class IMemoryHandleBase
{
public:
	virtual ~IMemoryHandleBase() = default;
	virtual void relocateHandle(uint8_t* new_address) = 0;
	virtual size_t objectSize() const = 0;
	virtual void updateAddress(uint8_t* new_address) = 0;
	virtual uint8_t* getAddress() const = 0;
	virtual size_t getTotalSize() const = 0;
};

// user has to depend on memory handle to ensure pointer address validity remains after defragmentation
template <typename T>
class MemoryHandle : public IMemoryHandleBase
{
public:
	explicit MemoryHandle(GeneralAllocator& allocator, uint8_t* address = nullptr, size_t size = 0)
		: m_allocator(allocator), m_address(address), m_size(size)
	{
	}

	// pointer-like behavior with nullptr checks
	T& operator*() const
	{
		T* ptr = reinterpret_cast<T*>(m_address);
		const bool isValid = m_allocator.isHandleValid(this);
		if (!ptr || !isValid)
		{
			throw std::runtime_error("Dereferencing nullptr in MemoryHandle");
		}
		return *ptr;
	}

	T* operator->() const
	{
		T* ptr = reinterpret_cast<T*>(m_address);
		const bool isValid = m_allocator.isHandleValid(this);
		if (!ptr || !isValid)
		{
			throw std::runtime_error("Dereferencing nullptr in MemoryHandle");
		}
		return ptr;
	}

	T& operator[](size_t index) const
	{
		T* ptr = reinterpret_cast<T*>(m_address);
		const bool isValid = m_allocator.isHandleValid(this);
		if (!ptr || !isValid)
		{
			throw std::runtime_error("Dereferencing nullptr in MemoryHandle");
		}
		return ptr[index];
	}

	void updateAddress(uint8_t* new_address) override
	{
		m_address = new_address;
	}

	uint8_t* getAddress() const override
	{
		return m_address;
	}

	void relocateHandle(uint8_t* new_address) override
	{
		if constexpr (MoveConstructible<T>)
		{
			const size_t amount = getObjectAmount();
			for (size_t i = 0; i < amount; ++i)
			{
				T* ptr = reinterpret_cast<T*>(m_address + i * sizeof(T));
				T* new_location = new (new_address + i * sizeof(T)) T(std::move(*ptr));
				ptr->~T();
			}

			m_address = new_address;
		}
		else
		{
			memmove(new_address, m_address, m_size);
		}
	}

	size_t objectSize() const override
	{
		return sizeof(T);
	}

	size_t getTotalSize() const override
	{
		return m_size;
	}

	size_t getObjectAmount() const
	{
		return m_size / sizeof(T);
	}

private:
	GeneralAllocator& m_allocator;
	uint8_t* m_address;
	size_t m_size;
};

// TODO: look into alignment issues
class GeneralAllocator
{
    static constexpr size_t DEFAULTSIZE = 1024;

    struct GeneralAllocatorChunk
    {
        uint8_t* pChunk;
        size_t size;

        GeneralAllocatorChunk(uint8_t* pChunk, size_t size)
            : pChunk{ pChunk }, size{ size }
        {
        }

        bool isAdjacent(const GeneralAllocatorChunk& other) const
        {
            return pChunk + size == other.pChunk || other.pChunk + other.size == pChunk;
        }

        std::strong_ordering operator<=>(const GeneralAllocatorChunk& other) const
        {
            return pChunk <=> other.pChunk;
        }

        bool operator==(const GeneralAllocatorChunk& other) const
        {
            return pChunk == other.pChunk && size == other.size;
        }
    };

public:
    explicit GeneralAllocator(size_t size = DEFAULTSIZE)
        : m_size{ size }
    {
        m_pMemory = new uint8_t[size];
        m_chunks.emplace_back(m_pMemory, size);

		for (auto& handle : m_handles)
		{
			handle = new MemoryHandle<int>(*this);
		}
    }

    GeneralAllocator(const GeneralAllocator&) = delete;
    GeneralAllocator(GeneralAllocator&&) = delete;

    GeneralAllocator& operator=(const GeneralAllocator&) = delete;
    GeneralAllocator& operator=(GeneralAllocator&&) = delete;

    ~GeneralAllocator()
    {
        if (m_pMemory)
        {
            delete[] m_pMemory;
            m_pMemory = nullptr;
        }
    }

    template <typename T, typename... Args>
	MemoryHandle<T>& allocate(size_t amount = 1, Args&&... args)
	{
		for (auto& chunk : m_chunks)
		{
			const size_t requestedSize = amount * sizeof(T);
			// find a chunk that has enough space to allocate the requested memory
			if (chunk.size >= requestedSize)
			{
				T* data = reinterpret_cast<T*>(chunk.pChunk);
				// shrink the chunk and move the pointer to the next available memory
				chunk.size -= requestedSize;
				chunk.pChunk += requestedSize;

				//construct the objects in the allocated memory
				for (size_t i = 0; i < amount; ++i)
				{
					new (data + i) T(std::forward<Args>(args)...);
				}

				// remove the chunk if it's empty
				if (chunk.size == 0)
				{
					std::erase(m_chunks, chunk);
				}

				return registerHandle<T>(reinterpret_cast<uint8_t*>(data), requestedSize);
			}
		}

		throw std::runtime_error("Out of memory");
	}

    template <typename T>
	void deallocate(MemoryHandle<T> handle)
	{
		// call the destructor for each object
		T* pointer = handle.operator->();
		const size_t amount = handle.getObjectAmount();
		for (size_t i = 0; i < amount; ++i)
		{
			pointer[i].~T();
		}

		uint8_t* pChunkStart = reinterpret_cast<uint8_t*>(pointer);
		const size_t chunkSize = amount * sizeof(T);

		const GeneralAllocatorChunk freeChunk{ pChunkStart, chunkSize };

		const auto insertPosition = std::ranges::lower_bound(m_chunks, freeChunk);
		const auto leftChunk = insertPosition != m_chunks.begin() ? (insertPosition - 1) : m_chunks.end();
		const auto rightChunk = insertPosition != m_chunks.end() ? insertPosition : m_chunks.end();

		// merge the free chunk with the adjacent chunks
		bool merged = false;
		if (leftChunk != m_chunks.end() && leftChunk->isAdjacent(freeChunk))
		{
			leftChunk->size += freeChunk.size;
			merged = true;
		}
		if (rightChunk != m_chunks.end() && freeChunk.isAdjacent(*rightChunk))
		{
			if (merged)
			{
				leftChunk->size += rightChunk->size;
				m_chunks.erase(rightChunk);
			}
			else
			{
				rightChunk->size += freeChunk.size;
				rightChunk->pChunk = freeChunk.pChunk;
				merged = true;
			}
		}

		// remove the handle from the allocator
		if (!m_handleTable.contains(handle.getAddress()))
		{
			throw std::runtime_error("Invalid handle address");
		}
		m_handleTable.erase(handle.getAddress());

    	if (!merged)
		{
			m_chunks.insert(insertPosition, freeChunk);
		}
	}

	size_t getAvailableMemory() const
	{
		size_t availableMemory = 0;
		for (const auto& chunk : m_chunks)
		{
			availableMemory += chunk.size;
		}
		return availableMemory;
	}

	void defragment()
    {
		uint8_t* pCurrentMemoryPosition = m_pMemory;
		for (const auto& handle : m_handleTable)
		{
			if (handle.second->getAddress() == pCurrentMemoryPosition)
			{
				pCurrentMemoryPosition += handle.second->getTotalSize();
				continue;
			}
			handle.second->relocateHandle(pCurrentMemoryPosition);
			handle.second->updateAddress(pCurrentMemoryPosition);
			pCurrentMemoryPosition += handle.second->getTotalSize();
		}

		const size_t remainingSize = m_size - (pCurrentMemoryPosition - m_pMemory);
		m_chunks.clear();
		m_chunks.emplace_back(pCurrentMemoryPosition, remainingSize);

		updateHandleTableAddresses();
    }

	bool isHandleValid(const IMemoryHandleBase* handle) const
    {
		return m_handleTable.contains(handle->getAddress());
	}

	void debugPrintChunks() const
    {
		for (const auto& chunk : m_chunks)
		{
			std::cout << "Chunk: " << static_cast<ptrdiff_t>(chunk.pChunk - m_pMemory) << " Size: " << chunk.size << "\n";
		}
    }

private:
	// TODO: consider using a custom data structure to avoid the need for this function
	void updateHandleTableAddresses()
	{
		for (auto it = m_handleTable.begin(); it != m_handleTable.end();)
		{
			if (it->first != it->second->getAddress())
			{
				IMemoryHandleBase* handleBase = it->second;
				m_handleTable.erase(it++);
				m_handleTable[handleBase->getAddress()] = handleBase;
			}
			else
			{
				++it;
			}
		}
	}

	template <typename T>
	MemoryHandle<T>& registerHandle(uint8_t* data, size_t size)
	{
		if (m_handleCount >= m_handles.size())
		{
			throw std::runtime_error("Out of memory handles");
		}

		// placement new to construct the handle in the handle array
		new (m_handles[m_handleCount]) MemoryHandle<T>(*this, data, size);
		m_handleTable[data] = m_handles[m_handleCount];
		return *static_cast<MemoryHandle<T>*>(m_handles[m_handleCount++]);
	}

    uint8_t* m_pMemory;
    size_t m_size;
    std::vector<GeneralAllocatorChunk> m_chunks;
	std::map<uint8_t*, IMemoryHandleBase*> m_handleTable{};

	//list of handles to keep track of the allocated memory
	size_t m_handleCount = 0;
	std::array<IMemoryHandleBase*, 100> m_handles;
};