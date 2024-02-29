#pragma once

template <typename T>
union PoolChunk
{
	T data;
	PoolChunk<T>* next;

	PoolChunk() : next{nullptr} {}
	~PoolChunk() {}
};

template <typename T>
class PoolAllocator
{
public:
	explicit PoolAllocator(size_t size = DEFAULT_SIZE) : m_head{nullptr}, m_size{size}
	{
		m_data = new PoolChunk<T>[size];
		for (size_t i = 0; i < size - 1; ++i)
		{
			m_data[i].next = &m_data[i + 1];
		}
		m_data[size - 1].next = nullptr;
		m_head = m_data;
	}

	~PoolAllocator()
	{
		delete[] m_data;
		m_data = nullptr;
		m_head = nullptr;
	}

	PoolAllocator(const PoolAllocator& other) = delete;
	PoolAllocator(PoolAllocator&& other) = delete;

	PoolAllocator& operator=(const PoolAllocator& other) = delete;
	PoolAllocator& operator=(PoolAllocator&& other) = delete;

	template <typename ...Args>
	T* allocate(Args&&... args)
	{
		if (m_head == nullptr)
		{
			return nullptr;
		}
		PoolChunk<T>* temp = m_head;
		m_head = m_head->next;
		return new (temp) T(std::forward<Args>(args)...);
	}

	void deallocate(T* ptr)
	{
		if (ptr == nullptr)
		{
			return;
		}
		ptr->~T();
		PoolChunk<T>* temp = reinterpret_cast<PoolChunk<T>*>(ptr);
		temp->next = m_head;
		m_head = temp;
	}

private:
	static constexpr size_t DEFAULT_SIZE = 1024;
	PoolChunk<T>* m_data;
	PoolChunk<T>* m_head;
	size_t m_size;
};