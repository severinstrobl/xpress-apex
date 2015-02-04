//  Copyright (c) 2014 University of Oregon
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "thread_instance.hpp"
#include <iostream>

// TAU related
#ifdef APEX_HAVE_TAU
#define PROFILING_ON
#define TAU_DOT_H_LESS_HEADERS
#include <TAU.h>
#endif

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux) || defined(linux) || defined(__linux__)
#include <execinfo.h>
#elif __APPLE__
#  include <mach-o/dyld.h>
#elif defined(__FreeBSD__)
#  include <sys/types.h>
#  include <sys/sysctl.h>
#endif

using namespace std;

namespace apex {

// Global static pointer used to ensure a single instance of the class.
boost::thread_specific_ptr<thread_instance> thread_instance::_instance;
// Global static count of threads in system
boost::atomic_int thread_instance::_num_threads(0);
// Global static map of HPX thread names to TAU thread IDs
map<string, int> thread_instance::_name_map;
// Global static mutex to control access to the map
boost::mutex thread_instance::_name_map_mutex;
// Global static map of TAU thread IDs to HPX workers
map<int, bool> thread_instance::_worker_map;
// Global static mutex to control access to the map
boost::mutex thread_instance::_worker_map_mutex;

thread_instance& thread_instance::instance(void) {
  thread_instance* me = _instance.get();
  if( ! me ) {
    // first time called by this thread
    // construct test element to be used in all subsequent calls from this thread
    _instance.reset( new thread_instance());
    me = _instance.get();
    //me->_id = TAU_PROFILE_GET_THREAD();
    me->_id = _num_threads++;
  }
  return *me;
}

void thread_instance::set_worker(bool is_worker) {
  instance()._is_worker = is_worker;
  boost::unique_lock<boost::mutex> l(_worker_map_mutex);
  _worker_map[get_id()] = is_worker;
}

string thread_instance::get_name(void) {
  return instance()._top_level_timer_name;
}

void thread_instance::set_name(string name) {
  if (instance()._top_level_timer_name.empty())
  {
    instance()._top_level_timer_name = name;
    {
        boost::unique_lock<boost::mutex> l(_name_map_mutex);
        _name_map[name] = get_id();
    }
    if (name.find("worker-thread") != string::npos) {
      instance().set_worker(true);
    }
  }
}

int thread_instance::map_name_to_id(string name) {
  //cout << "Looking for " << name << endl;
  int tmp = -1;
  {
    boost::unique_lock<boost::mutex> l(_name_map_mutex);
    map<string, int>::const_iterator it = _name_map.find(name);
    if (it != _name_map.end()) {
      tmp = (*it).second;
    }
  }
  return tmp;
}

bool thread_instance::map_id_to_worker(int id) {
  //cout << "Looking for " << name << endl;
  bool worker = false;
  {
    boost::unique_lock<boost::mutex> l(_worker_map_mutex);
    map<int, bool>::const_iterator it = _worker_map.find(id);
    if (it != _worker_map.end()) {
      worker = (*it).second;
    }
  }
  return worker;
}

const char* program_path(void) {
    // FIXME: the initialization of path is not thread safe
    static string * the_path = NULL;

#if defined(_WIN32) || defined(_WIN64)

    if (the_path == NULL) {
        char path[MAX_PATH + 1] = { '\0' };
        if (!GetModuleFileName(NULL, path, sizeof(path)))
            return NULL;
        the_path = new string(path);
    }
    return the_path->c_str();

#elif defined(__linux) || defined(linux) || defined(__linux__)

    if (the_path == NULL) {
        char path[PATH_MAX];
        if (path != NULL) {
            if (readlink("/proc/self/exe", path, PATH_MAX) == -1)
                return NULL;
        }
        the_path = new string(path);
    }
    return the_path->c_str();

#elif defined(__APPLE__)

    if (the_path == NULL) {
        char path[PATH_MAX + 1];
        boost::uint32_t len = sizeof(path) / sizeof(path[0]);

        if (0 != _NSGetExecutablePath(path, &len))
            return NULL;

        path[len] = '\0';
        the_path = new string(path);
    }
    return the_path->c_str();

#elif defined(__FreeBSD__)

    if (the_path == NULL) {
        int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
        size_t cb = 0;
        sysctl(mib, 4, NULL, &cb, NULL, 0);
        if (!cb)
            return NULL;

        std::vector<char> buf(cb);
        sysctl(mib, 4, &buf[0], &cb, NULL, 0);
        the_path = new string(&buf[0]);
    }
    return the_path->c_str();

#else
#  error Unsupported platform
#endif
    return NULL;
}

string thread_instance::map_addr_to_name(void * function_address) {
  static std::string progname = string(program_path());
  auto it = _function_map.find(function_address);
  if (it != _function_map.end()) {
    return (*it).second;
  } // else...
#if 0
  char **strings = backtrace_symbols((void* const*)(&function_address),1);
  string name = string(strings[0]);
#else
  stringstream ss;
  ss << "UNRESOLVED " << progname << " ADDR " << hex << function_address;
  string name = string(ss.str());
#endif
  _function_map[function_address] = name;
  return name;
}


}