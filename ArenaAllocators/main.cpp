#include <iostream>
#include <string>

#include "PoolAllocator.h"
#include "StackAllocator.h"
#include "GeneralAllocator.h"

struct Person
{
    std::string m_name;
    std::string m_address;
    int m_age;

    //Person(Person&&) = delete;

    Person(Person&& other) noexcept : m_name{ other.m_name }, m_address{ other.m_address }, m_age{ other.m_age }
    {
		std::cout << "Person " << m_name << " moved\n";
	}

    Person() : m_name{"Gabriel"}, m_address{"Sandcastle"}, m_age{27}
    {
		std::cout << "Person constructor\n";
	}

    Person(const std::string& name, const std::string& address, int age) : m_name{ name }, m_address{ address }, m_age{ age }
    {
        std::cout << "Person " << name << " constructed\n";
    }

    ~Person()
    {
        std::cout << "Person " << m_name << " destructed\n";
    }

    friend std::ostream& operator<<(std::ostream& os, const Person& person)
    {
        os << "Name: " << person.m_name << "\nAddress: " << person.m_address << "\nAge: " << person.m_age << "\n";
        return os;
    }
};

void poolAllocatorTest();
void stackAllocatorTest();
void generalAllocatorTest();

int main()
{
    //poolAllocatorTest();
    //stackAllocatorTest();
    generalAllocatorTest();

    std::cout << "Tests Finished!\n";

    return 0;
}

void poolAllocatorTest()
{
    while (true)
    {
        PoolAllocator<Person> allocator;

        Person* person1 = allocator.allocate("John", "New York", 25);
        Person* person2 = allocator.allocate("Jane", "London", 30);
        Person* person3 = allocator.allocate("Jack", "Paris", 35);

        std::cout << *person1 << *person2 << *person3;

        allocator.deallocate(person1);
        allocator.deallocate(person2);
        allocator.deallocate(person3);
    }
}

void stackAllocatorTest()
{
    while (true)
    {
        StackAllocator allocator;
        const Person* persons1 = allocator.allocate<Person>(10, "John", "New York", 25);
        const auto marker = allocator.getMarker();

        for (size_t i = 0; i < 10; ++i)
        {
            std::cout << *(persons1 + i) << "\n";
        }

        allocator.deallocate(marker);
        allocator.deallocateAll();
    }
}

void generalAllocatorTest()
{
    GeneralAllocator allocator;

    allocator.debugPrintChunks();
    const auto& person1 = allocator.allocate<Person>();
    allocator.debugPrintChunks();
    const auto& person2 = allocator.allocate<Person>();
    allocator.debugPrintChunks();
    const auto& person3 = allocator.allocate<Person>();
    allocator.debugPrintChunks();
	const auto& person4 = allocator.allocate<Person>();
    allocator.debugPrintChunks();

    std::cout << *person1 << "\n";

    person1->m_name = "Benjamin";
    person1->m_address = "Vienna";
    person1->m_age = 24;

    person2->m_name = "Daniel";
    person2->m_address = "Graz";
    person2->m_age = 30;

    person3->m_name = "Phil";
    person3->m_address = "Berlin";
    person3->m_age = 28;

    person4->m_name = "Tom";
    person4->m_address = "Munich";
    person4->m_age = 26;

    std::cout << *person1 << "\n";
    std::cout << *person2 << "\n";
    std::cout << *person3 << "\n";
    std::cout << *person4 << "\n";

    allocator.deallocate(person3);
    allocator.debugPrintChunks();
    allocator.deallocate(person2);
    allocator.debugPrintChunks();

    allocator.defragment();
    allocator.debugPrintChunks();

    std::cout << *person1 << "\n";
    std::cout << *person4 << "\n";

    //std::cout << *person2 << "\n";
    //std::cout << *person3 << "\n";
    //std::cout << *person3 << "\n";

    /*while (true)
    {
        GeneralAllocator allocator;
        Person* person1 = allocator.allocate<Person>();

        std::cout << *person1 << "\n";

        person1->m_name = "Benjamin";
        person1->m_address = "Vienna";
        person1->m_age = 24;

        std::cout << *person1 << "\n";

        allocator.deallocate(person1, 1);
    }*/
}