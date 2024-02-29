#pragma once
#include <cassert>
#include <vector>

#include "Typedefs.h"

template <typename T>
concept TriviallyDestructible = std::is_trivially_destructible_v<T>;

template <typename T>
concept NonTriviallyDestructible = !TriviallyDestructible<T>;

class StackAllocatorDestructor
{
public:
	StackAllocatorDestructor()
		: m_data{ nullptr }, m_destructor{ nullptr }
	{

	}

	template <typename T>
	explicit StackAllocatorDestructor(const T& data) : m_data{ &data }
													 , m_destructor{ [](const void* data) { static_cast<const T*>(data)->~T(); } }
	{

	}

	void operator()() const
	{
		if (m_destructor)
		{
			m_destructor(m_data);
		}
	}

private:
	const void* m_data;
	void(*m_destructor)(const void*);
};

class StackAllocatorMarker
{
public:
	StackAllocatorMarker(u8* head, size_t destructorsSize) : m_head{head}
		, m_destructorsSize{destructorsSize}
	{

	}

	u8 *m_head;
	size_t m_destructorsSize;
};

class StackAllocator
{
	template <TriviallyDestructible T>
	inline void addDestructor(T* object)
	{
		// do nothing
	}

	template <NonTriviallyDestructible T>
	inline void addDestructor(T* object)
	{
		m_destructors.emplace_back(StackAllocatorDestructor{*object});
	}

public:
	explicit StackAllocator(size_t size = DEFAULT_STACK_SIZE) : m_size{ size }
	{
		m_data = new u8[size];
		m_head = m_data;
	}

	~StackAllocator()
	{
		assert(m_data == m_head);
		delete[] m_data;
		m_data = nullptr;
		m_head = nullptr;
	}

	StackAllocator(const StackAllocator& other) = delete;
	StackAllocator(StackAllocator&& other) = delete;

	StackAllocator& operator=(const StackAllocator& other) = delete;
	StackAllocator& operator=(StackAllocator&& other) = delete;

	template <typename T, typename ...Args> T* allocate(size_t amount = 1, Args&&... args)
	{
		T* alignedHead = static_cast<T*>(allocate(amount * sizeof(T), alignof(T)));
		if (!alignedHead)
		{
			return nullptr;
		}

		for (size_t i = 0; i < amount; ++i)
		{
			T* object = new (alignedHead + i) T(std::forward<Args>(args)...);
			addDestructor(object);
		}
		return alignedHead;
	}

	void deallocateAll()
	{
		while (m_destructors.size() > 0)
		{
			m_destructors.back()();
			m_destructors.pop_back();
		}
		m_head = m_data;
	}

	void deallocate(StackAllocatorMarker marker)
	{
		assert(marker.m_head >= m_data);
		assert(marker.m_head <= m_head);
		assert(marker.m_destructorsSize <= m_destructors.size());

		while (m_destructors.size() > marker.m_destructorsSize)
		{
			m_destructors.back()();
			m_destructors.pop_back();
		}
		m_head = marker.m_head;
	}

	StackAllocatorMarker getMarker()
	{
		return StackAllocatorMarker{m_head, m_destructors.size()};
	}

private:
	void* allocate(size_t size, size_t alignment = 1)
	{
		assert(size > 0);
		assert(alignment > 0);
		assert(alignment % 2 == 0);
		u8* alignedHead = reinterpret_cast<u8*>(reinterpret_cast<size_t>(m_head + alignment - 1) & ~(alignment - 1));
		u8* newHead = alignedHead + size;
		if (newHead > m_data + m_size)
		{
			return nullptr;
		}
		m_head = newHead;
		return alignedHead;
	}

	static constexpr size_t DEFAULT_STACK_SIZE = 1024; // 1MB
	u8* m_data = nullptr;
	u8* m_head = nullptr;
	size_t m_size = 0;
	std::vector<StackAllocatorDestructor> m_destructors;
};