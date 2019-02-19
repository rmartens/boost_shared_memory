#include <boost/interprocess/managed_shared_memory.hpp>
#include <functional>
#include <iostream>
#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread/thread.hpp>
#include <boost/interprocess/sync/named_recursive_mutex.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/string.hpp>

bool bContinue = true;

using namespace boost::interprocess;

typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef basic_string<char, std::char_traits<char>, CharAllocator> shm_string;

void construct_objects(managed_shared_memory &managed_shm, int& i, float& f, std::string& s)
{
    managed_shm.destroy<int>("Integer");
    managed_shm.destroy<float>("Float");
    managed_shm.destroy<shm_string>("String");

    managed_shm.construct<int>("Integer")(i);
    managed_shm.construct<float>("Float")(f);
    managed_shm.construct<shm_string>("String")(s.c_str(), managed_shm.get_segment_manager());
}

/**
 * @brief Example usage of boost managed_shared_memory class.
 *        Safely passes data from writer process to reader process.
 * @param argc if one then runs as the writer, if more than one then runs as the reader
 * @return
 */
int main(int argc, char* /*argv[]*/)
{
    std::string shared_memory_name("boost_shared_memory");
    std::string mutex_name("boost_mutex");

    boost::interprocess::named_recursive_mutex m_interprocess_mutex(boost::interprocess::open_or_create, mutex_name.c_str());

    if (argc == 1) // writer
    {
        int i = 99;
        float f = 3.14;
        std::string s = boost::lexical_cast<std::string>(f);
        managed_shared_memory managed_shm;

        {
            boost::interprocess::scoped_lock<boost::interprocess::named_recursive_mutex> lock(m_interprocess_mutex);
            shared_memory_object::remove(shared_memory_name.c_str());
            managed_shm = managed_shared_memory{open_or_create, shared_memory_name.c_str(), 1024};
        }

        while (bContinue)
        {
            std::cout << "server" << "\n";
            std::cout << i << '\n';
            std::cout << f << '\n';
            std::cout << s << '\n';
            std::flush(std::cout);

            {
                boost::interprocess::scoped_lock<boost::interprocess::named_recursive_mutex> lock(m_interprocess_mutex);
                auto atomic_construct = std::bind(construct_objects, std::ref(managed_shm), std::ref(i), std::ref(f), std::ref(s));
                managed_shm.atomic_func(atomic_construct);
            }

            i = i + 1;
            f = f + 1.0;
            s = boost::lexical_cast<std::string>(f);


            boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
        }
    }
    else // reader
    {
        managed_shared_memory managed_shm;

        {
            bool bSharedMemFound = false;
            while (!bSharedMemFound)
            {
                try
                {
                    boost::interprocess::scoped_lock<boost::interprocess::named_recursive_mutex> lock(m_interprocess_mutex);
                    managed_shm = managed_shared_memory{open_read_only, shared_memory_name.c_str()};
                    bSharedMemFound = true;
                }
                catch (const std::exception &ex)
                {
                    std::cout << "managed_shared_memory ex: "  << ex.what();
                    std::flush(std::cout);
                }
                boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
            }
        }

        while (bContinue)
        {
            {
                boost::interprocess::scoped_lock<boost::interprocess::named_recursive_mutex> lock(m_interprocess_mutex);
                std::cout << "client" << "\n";
                std::cout << *managed_shm.find<int>("Integer").first << '\n';
                std::cout << *managed_shm.find<float>("Float").first << '\n';
                std::cout << *managed_shm.find<shm_string>("String").first << '\n';
                std::flush(std::cout);
            }
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
        }
    }

    return 0;
}
