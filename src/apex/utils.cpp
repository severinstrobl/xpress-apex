/*
 * Copyright (c) 2014-2020 Kevin Huck
 * Copyright (c) 2014-2020 University of Oregon
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "utils.hpp"
#include "apex.hpp"
#include <sstream>
#include <cstring>
#if defined(__GNUC__)
#include <cxxabi.h>
#endif
// for setting thread affinity
#if !defined(__APPLE__) && !defined(_MSC_VER)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
// for removing directories
#if !defined(_MSC_VER)
#include <dirent.h>
#endif
#include <sys/stat.h>
#include "apex_assert.h"
#include <atomic>
#include <iostream>
#include <string>
#include <vector>
#include <mutex>

namespace apex {

/* Idea borrowed from:
 * http://stackoverflow.com/questions/931827/
    stdstring-comparison-check-whether-string-begins-with-another-string */
bool starts_with(const std::string& input, const std::string& match)
{
        return input.size() >= match.size()
                    && std::equal(match.begin(), match.end(), input.begin());
}

/* Idea borrowed from:
 * http://stackoverflow.com/questions/236129/split-a-string-in-c
 */
std::vector<std::string> &split(const std::string &s, char delim,
    std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        // ignore duplicate delimiters.
        if (item.size() > 0) {
            elems.push_back(item);
        }
    }
    return elems;
}

std::string demangle(const std::string& timer_name) {
    std::string demangled = std::string(timer_name);
#if defined(__GNUC__)
    int     status;
    char *realname = abi::__cxa_demangle(timer_name.c_str(), 0, 0, &status);
    if (status == 0 && realname != nullptr) {
    /*
        char* index = strstr(realname, "<");
        if (index != nullptr) {
            *index = 0; // terminate before templates for brevity
        }
    */
        demangled = std::string(realname);
        free(realname);
    } else {
#if defined(APEX_DEBUG)
        switch (status) {
            case 0:
                printf("The demangling operation succeeded, but realname is NULL\n");
                break;
            case -1:
                printf("The demangling operation failed:");
                printf(" A memory allocation failiure occurred.\n");
                break;
            case -2:
                printf("The demangling operation failed:");
                printf(" '%s' is not a valid", timer_name.c_str());
                printf(" name under the C++ ABI mangling rules.\n");
                break;
            case -3:
                printf("The demangling operation failed: One of the");
                printf(" arguments is invalid.\n");
                break;
            default:
                printf("The demangling operation failed: Unknown error.\n");
                break;
        }
#endif // defined(APEX_DEBUG)
    }
#endif
    return demangled;
}

#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

void set_thread_affinity(int core) {
#if !defined(__APPLE__) && !defined(_MSC_VER) && !defined(APEX_HAVE_HPX)
    cpu_set_t cpuset;
    cpu_set_t mask;

    if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_getaffinity");
        return;
    }
    if (CPU_ISSET(core, &mask)) {
        CPU_ZERO(&cpuset);
        CPU_SET(core, &cpuset);
        sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    }
#else
    APEX_UNUSED(core);
#endif
    return;
}

void set_thread_affinity(void) {
#if !defined(__APPLE__) && !defined(_MSC_VER) && !defined(APEX_HAVE_HPX)
    cpu_set_t cpuset;
    cpu_set_t mask;

    // only let one thread in here at a time
    std::mutex _mutex;
    std::unique_lock<std::mutex> l(_mutex);
    static unsigned int last_assigned = my_hardware_concurrency();

    if (sched_getaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
        perror("sched_getaffinity");
        return;
    }
    unsigned int j = my_hardware_concurrency() - 1;
    for (unsigned int i = j ; i > 0 ; i--) {
        if (CPU_ISSET(i, &mask) && i < last_assigned) {
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
            last_assigned = i;
            break;
        }
    }
#endif
    return;
}

void remove_path(const char *pathname) {
#if !defined(_MSC_VER)
    struct dirent *entry = nullptr;
    DIR *dir = nullptr;
    struct stat sb;
    if (stat(pathname, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        dir = opendir(pathname);
        while((entry = readdir(dir)) != nullptr) {
            DIR *sub_dir = nullptr;
            FILE *file = nullptr;
            std::stringstream abs_path;
            if(*(entry->d_name) != '.') {
                abs_path << pathname << "/" <<  entry->d_name;
                sub_dir = opendir(abs_path.str().c_str());
                if(sub_dir != nullptr) {
                    closedir(sub_dir);
                    remove_path(abs_path.str().c_str());
                } else {
                    file = fopen(abs_path.str().c_str(), "r");
                    if(file != nullptr) {
                        fclose(file);
                        //printf("Removing: %s\n", abs_path);
                        remove(abs_path.str().c_str());
                    }
                }
            }
        }
        //printf("Removing: %s\n", pathname);
        remove(pathname);
    }
#endif
}

std::atomic<uint64_t> reference_counter::task_wrappers(0L);
std::atomic<uint64_t> reference_counter::null_task_wrappers(0L);

std::atomic<uint64_t> reference_counter::starts(0L);
std::atomic<uint64_t> reference_counter::disabled_starts(0L);
std::atomic<uint64_t> reference_counter::apex_internal_starts(0L);
std::atomic<uint64_t> reference_counter::hpx_shutdown_starts(0L);
std::atomic<uint64_t> reference_counter::hpx_timer_starts(0L);
std::atomic<uint64_t> reference_counter::suspended_starts(0L);
std::atomic<uint64_t> reference_counter::failed_starts(0L);
std::atomic<uint64_t> reference_counter::starts_after_finalize(0L);

std::atomic<uint64_t> reference_counter::resumes(0L);
std::atomic<uint64_t> reference_counter::disabled_resumes(0L);
std::atomic<uint64_t> reference_counter::apex_internal_resumes(0L);
std::atomic<uint64_t> reference_counter::hpx_shutdown_resumes(0L);
std::atomic<uint64_t> reference_counter::hpx_timer_resumes(0L);
std::atomic<uint64_t> reference_counter::suspended_resumes(0L);
std::atomic<uint64_t> reference_counter::failed_resumes(0L);
std::atomic<uint64_t> reference_counter::resumes_after_finalize(0L);

std::atomic<uint64_t> reference_counter::yields(0L);
std::atomic<uint64_t> reference_counter::disabled_yields(0L);
std::atomic<uint64_t> reference_counter::null_yields(0L);
std::atomic<uint64_t> reference_counter::double_yields(0L);
std::atomic<uint64_t> reference_counter::yields_after_finalize(0L);
std::atomic<uint64_t> reference_counter::apex_internal_yields(0L);

std::atomic<uint64_t> reference_counter::stops(0L);
std::atomic<uint64_t> reference_counter::disabled_stops(0L);
std::atomic<uint64_t> reference_counter::null_stops(0L);
std::atomic<uint64_t> reference_counter::double_stops(0L);
std::atomic<uint64_t> reference_counter::exit_stops(0L);
std::atomic<uint64_t> reference_counter::apex_internal_stops(0L);
std::atomic<uint64_t> reference_counter::stops_after_finalize(0L);

void reference_counter::report_stats(void) {
    unsigned int ins = starts + resumes;
    unsigned int all_ins =
        starts +
        resumes +
        disabled_starts +
        disabled_resumes +
        apex_internal_starts +
        apex_internal_resumes +
        hpx_shutdown_starts +
        hpx_shutdown_resumes +
        hpx_timer_starts +
        hpx_timer_resumes +
        suspended_starts +
        suspended_resumes +
        failed_starts +
        failed_resumes +
        starts_after_finalize +
        resumes_after_finalize;
    unsigned int outs = yields + stops;
    unsigned int all_outs = yields +
        stops +
        disabled_yields +
        disabled_stops +
        null_yields +
        null_stops +
        double_yields +
        double_stops +
        exit_stops +
        yields_after_finalize +
        stops_after_finalize +
        apex_internal_yields +
        apex_internal_stops;;
    int nid = apex::apex::instance()->get_node_id();
    std::stringstream ss;
    if (task_wrappers > 0) {
        ss << nid << " Task Wrappers         : " << task_wrappers << std::endl;
    }
    if (task_wrappers > 0) {
        ss << nid << " NULL Task Wrappers    : " << null_task_wrappers << std::endl;
    }
    if (starts > 0) {
        ss << nid << " Starts                : " << starts << std::endl;
    }
    if (resumes > 0) {
        ss << nid << " Resumes               : " << resumes << std::endl;
    }
    if (apex_internal_starts > 0) {
        ss << nid << " APEX Starts           : " << apex_internal_starts << std::endl;
    }
    if (apex_internal_resumes > 0) {
        ss << nid << " APEX Resumes          : " << apex_internal_resumes << std::endl;
    }
    if (hpx_shutdown_starts > 0) {
        ss << nid << " HPX shutdown starts   : " << hpx_shutdown_starts << std::endl;
    }
    if (hpx_shutdown_resumes > 0) {
        ss << nid << " HPX shutdown resumes  : " << hpx_shutdown_resumes << std::endl;
    }
    if (hpx_timer_starts > 0) {
        ss << nid << " HPX timer starts      : " << hpx_timer_starts << std::endl;
    }
    if (hpx_timer_resumes > 0) {
        ss << nid << " HPX timer resumes     : " << hpx_timer_resumes << std::endl;
    }
    if (suspended_starts > 0) {
        ss << nid << " Suspended starts      : " << suspended_starts << std::endl;
    }
    if (suspended_resumes > 0) {
        ss << nid << " Suspended resumes     : " << suspended_resumes << std::endl;
    }
    if (failed_starts > 0) {
        ss << nid << " Failed starts         : " << failed_starts << std::endl;
    }
    if (failed_resumes > 0) {
        ss << nid << " Failed resumes        : " << failed_resumes << std::endl;
    }
    if (disabled_starts > 0) {
        ss << nid << " Disabled Starts       : " << disabled_starts << std::endl;
    }
    if (disabled_resumes > 0) {
        ss << nid << " Disabled Resumes      : " << disabled_resumes << std::endl;
    }
    if (starts_after_finalize > 0) {
        ss << nid << " Starts after finalize : " << starts_after_finalize << std::endl;
    }
    if (resumes_after_finalize > 0) {
        ss << nid << " Resumes after finalize: " << resumes_after_finalize << std::endl;
    }
    ss << nid << " -----------------------------" << std::endl;
    ss << nid << " Total in              : " << all_ins << std::endl << std::endl;

    if (yields > 0) {
        ss << nid << " Yields                : " << yields << std::endl;
    }
    if (stops > 0) {
        ss << nid << " Stops                 : " << stops << std::endl;
    }
    if (apex_internal_yields > 0) {
        ss << nid << " APEX Yields           : " << apex_internal_yields << std::endl;
    }
    if (apex_internal_stops > 0) {
        ss << nid << " APEX Stops            : " << apex_internal_stops << std::endl;
    }
    if (null_yields > 0) {
        ss << nid << " Null Yields           : " << null_yields << std::endl;
    }
    if (null_stops > 0) {
        ss << nid << " Null Stops            : " << null_stops << std::endl;
    }
    if (double_yields > 0) {
        ss << nid << " Double Yields         : " << double_yields << std::endl;
    }
    if (double_stops > 0) {
        ss << nid << " Double Stops          : " << double_stops << std::endl;
    }
    if (disabled_yields > 0) {
        ss << nid << " Disabled Yields       : " << disabled_yields << std::endl;
    }
    if (disabled_stops > 0) {
        ss << nid << " Disabled Stops        : " << disabled_stops << std::endl;
    }
    if (exit_stops > 0) {
        ss << nid << " Exit Stops            : " << exit_stops << std::endl;
    }
    if (yields_after_finalize > 0) {
        ss << nid << " Yields after finalize : " << yields_after_finalize << std::endl;
    }
    if (stops_after_finalize > 0) {
        ss << nid << " Stops after finalize  : " << stops_after_finalize << std::endl;
    }
    ss << nid << " -----------------------------" << std::endl;
    ss << nid << " Total out             : " << all_outs << std::endl << std::endl;
#ifdef APEX_HAVE_HPX
    if (ins != outs && ins != outs+1) {
#else
    if (ins != outs) {
#endif
        ss << std::endl;
        ss << " ------->>> ERROR! missing ";
        if (ins > outs) {
          ss << (ins - outs) << " stops. <<<-------" << std::endl;
        } else {
          ss << (outs - ins) << " starts. <<<-------" << std::endl;
        }
        ss << std::endl;
/*
        cout << "Profilers that were not stopped:" << endl;
        for (auto tmp : thread_instance::get_open_profilers()) {
            cout << tmp << endl;
        }
*/
    }
#ifdef APEX_HAVE_HPX
    if (all_ins != all_outs && all_ins != all_outs+1) {
#else
    if (all_ins != all_outs) {
#endif
        ss << std::endl;
        ss << " ------->>> Warning! missing ";
        if (all_ins > all_outs) {
          ss << (all_ins - all_outs) << " total stops. <<<-------" << std::endl;
        } else {
          ss << (all_outs - all_ins) << " total starts. <<<-------" << std::endl;
        }
        ss << std::endl;
/*
        cout << "Profilers that were not stopped:" << endl;
        for (auto tmp : thread_instance::get_open_profilers()) {
            cout << tmp << endl;
        }
*/
    }
    std::cout << ss.str(); // flush it!
    //APEX_ASSERT(ins == outs);
}

/* This function reverses the bits of a 32 bit unsigned integer. */
uint32_t simple_reverse(uint32_t x)
{
    x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
    x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
    x = ((x >> 4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) << 4);
    x = ((x >> 8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) << 8);
    x = ((x >> 16) & 0xffffu) | ((x & 0xffffu) << 16);
    return x;
}

uint64_t test_for_MPI_comm_rank(uint64_t commrank) {
    /* Some configurations might use MPI without telling APEX - they can
     * call apex::init() with a rank of 0 and size of 1 even though
     * they are running in an MPI application.  For that reason, we double
     * check to make sure that we aren't in an MPI execution by checking
     * for some common environment variables. */
    // PMI, MPICH, Cray, Intel, MVAPICH2...
    const char * tmpvar = getenv("PMI_RANK");
    if (tmpvar != NULL) {
        commrank = atol(tmpvar);
        // printf("Changing MPICH rank to %lu\n", commrank);
        return commrank;
    }
    // OpenMPI, Spectrum
    tmpvar = getenv("OMPI_COMM_WORLD_RANK");
    if (tmpvar != NULL) {
        commrank = atol(tmpvar);
        // printf("Changing openMPI rank to %lu\n", commrank);
        return commrank;
    }
    // PBS/Torque
    tmpvar = getenv("PBS_TASKNUM");
    if (tmpvar != NULL) {
        commrank = atol(tmpvar);
        return commrank;
    }
    // Slurm - last resort
    tmpvar = getenv("SLURM_PROCID");
    if (tmpvar != NULL) {
        commrank = atol(tmpvar);
        return commrank;
    }
    return commrank;
}

uint64_t test_for_MPI_comm_size(uint64_t commsize) {
    /* Some configurations might use MPI without telling APEX - they can
     * call apex::init() with a rank of 0 and size of 1 even though
     * they are running in an MPI application.  For that reason, we double
     * check to make sure that we aren't in an MPI execution by checking
     * for some common environment variables. */
    // PMI, MPICH, Cray, Intel, MVAPICH2...
    const char * tmpvar = getenv("PMI_SIZE");
    if (tmpvar != NULL) {
        commsize = atol(tmpvar);
        // printf("Changing MPICH size to %lu\n", commsize);
        return commsize;
    }
    // OpenMPI, Spectrum
    tmpvar = getenv("OMPI_COMM_WORLD_SIZE");
    if (tmpvar != NULL) {
        commsize = atol(tmpvar);
        // printf("Changing openMPI size to %lu\n", commsize);
        return commsize;
    }
    // PBS/Torque - no variable specifies number of nodes...
#if 0
    tmpvar = getenv("PBS_TASKNUM"); // number of tasks requested
    if (tmpvar != NULL) {
        commsize = atol(tmpvar);
        return commsize;
    }
#endif
    // Slurm - last resort
    tmpvar = getenv("SLURM_NNODES");
    if (tmpvar != NULL) {
        commsize = atol(tmpvar);
        return commsize;
    }
    return commsize;
}

std::string activity_to_string(apex_cuda_async_activity_t activity) {
    static std::string kernel{"Compute"};
    static std::string memory{"Memory"};
    static std::string sync{"Sync"};
    static std::string other{"Other"};
    static std::string empty{""};
    switch (activity) {
        case APEX_CUDA_KERNEL:
            return kernel;
        case APEX_CUDA_MEMORY:
            return memory;
        case APEX_CUDA_SYNCHRONIZE:
            return sync;
        case APEX_CUDA_OTHER:
            return other;
        default:
            return empty;
    }
}

/* The following code is from:
   http://stackoverflow.com/questions/7706339/
   grayscale-to-red-green-blue-matlab-jet-color-scale */
node_color * get_node_color_visible(double v, double vmin, double vmax) {
   node_color * c = new node_color();

   if (v < vmin)
      v = vmin;
   if (v > vmax)
      v = vmax;
   double dv = vmax - vmin;
   double fraction = 1.0 - ( (v - vmin) / dv );
   // red should be full on.
   c->red = 1.0;
   // blue should increase as the fraction increases.
   c->blue = (1.0 * fraction);
   // green should increase as the fraction increases.
   c->green = (1.0 * fraction);
   return c;
}

node_color * get_node_color(double v,double vmin,double vmax)
{
   node_color * c = new node_color();
   double dv;

   if (v < vmin)
      v = vmin;
   if (v > vmax)
      v = vmax;
   dv = vmax - vmin;

   if (v < (vmin + 0.25 * dv)) {
      c->red = 0;
      c->green = 4 * (v - vmin) / dv;
   } else if (v < (vmin + 0.5 * dv)) {
      c->red = 0;
      c->blue = 1 + 4 * (vmin + 0.25 * dv - v) / dv;
   } else if (v < (vmin + 0.75 * dv)) {
      c->red = 4 * (v - vmin - 0.5 * dv) / dv;
      c->blue = 0;
   } else {
      c->green = 1 + 4 * (vmin + 0.75 * dv - v) / dv;
      c->blue = 0;
   }

   return(c);
}

size_t& in_apex::get() {
    thread_local static size_t _in = 0;
    return _in;
}
in_apex::in_apex() {
    get()++;
    //printf("IN %lu, %lu\n", syscall(SYS_gettid), get());
}
in_apex::~in_apex() {
    get()--;
    //printf("OUT %lu, %lu\n", syscall(SYS_gettid), get());
}

} // namespace apex

extern "C" void __cyg_profile_func_enter(void *this_fn, void *call_site) __attribute__((no_instrument_function));
extern "C" void __cyg_profile_func_exit(void *this_fn, void *call_site) __attribute__((no_instrument_function));

extern "C" void __cyg_profile_func_enter(void *this_fn, void *call_site)
{
    APEX_UNUSED(call_site);
    apex::start((apex_function_address)this_fn);
} /* __cyg_profile_func_enter */

extern "C" void __cyg_profile_func_exit(void *this_fn, void *call_site)
{
    APEX_UNUSED(this_fn);
    APEX_UNUSED(call_site);
    apex::stop(apex::thread_instance::instance().get_current_profiler());
} /* __cyg_profile_func_enter */
