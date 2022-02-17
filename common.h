#pragma once
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#ifdef _NEON
#include <arm_neon.h>
#endif

struct my_compare {
    bool operator()(const std::string &key1, const std::string &key2) const
    {
        return true;
    }
};

template <class T>
T str_to_num(const std::string &str)
{
    std::istringstream iss(str);
    T num;
    iss >> num;
    return num;
}

inline int split_string_as_uint32(const std::string &src, std::vector<uint32_t> &dst, const char flag)
{
    std::string tmp;
    std::istringstream is(src);
    char *endptr;
    while (getline(is, tmp, flag)) {
        // check if decimal
        uint32_t result = std::strtoul(tmp.c_str(), &endptr, 10);
        if (*endptr == '\0') {
            dst.push_back(result);
            continue;
        }
        // check if octal
        result = std::strtoul(tmp.c_str(), &endptr, 8);
        if (*endptr == '\0') {
            dst.push_back(result);
            continue;
        }
        // check if hex
        result = std::strtoul(tmp.c_str(), &endptr, 16);
        if (*endptr == '\0') {
            dst.push_back(result);
            continue;
        }
    }
    return 0;
}

inline std::vector<std::string> split_string(const std::string &str, const char splitStr)
{
    std::vector<std::string> vecStr;
    std::string::size_type beginPos = str.find_first_not_of(splitStr, 0);
    std::string::size_type endPos = str.find_first_of(splitStr, beginPos);
    while (std::string::npos != endPos || std::string::npos != beginPos) {
        vecStr.push_back(str.substr(beginPos, endPos - beginPos));
        beginPos = str.find_first_not_of(splitStr, endPos);
        endPos = str.find_first_of(splitStr, beginPos);
    }

    return vecStr;
}

inline std::vector<std::string> split_string(const std::string &str, const char *splitStr)
{
    std::vector<std::string> vecStr;
    std::string::size_type beginPos = str.find_first_not_of(splitStr, 0);  //查找起始位置，即第一个不是分隔符的字符的位置
    std::string::size_type endPos = str.find_first_of(splitStr, beginPos); //根据起始位置，查找第一个分隔符位置　
    while (std::string::npos != endPos || std::string::npos != beginPos) { //开始查找，未找到返回npos,此处条件考虑到尾部结束为分割符的情况以及头部开始就是分隔符并且只有这一个分割符的情况
        vecStr.push_back(str.substr(beginPos, endPos - beginPos));         //从指定位置截断指定长度的字符串，str本身不变
        beginPos = str.find_first_not_of(splitStr, endPos);                //再从上次截断地方开始，查找下一个不是分隔符的起始位置
        endPos = str.find_first_of(splitStr, beginPos);                    //再次开始从指定位置查找下一个分隔符位置
    }

    return vecStr;
}

inline uint64_t get_current_mtime()
{
    timeval tv;
    gettimeofday(&tv, nullptr);

    //uint64_t time=tv.tv_sec;// 秒
    uint64_t time = tv.tv_sec * 1000 + tv.tv_usec / 1000; //毫秒
    //tv.tv_sec*1000000+tv.tv_usec 微秒
    //uint64_t time=tv.tv_sec*1000+tv.tv_usec/1000;

    return time;
}


#ifdef _NEON
inline void neon_memcpy(volatile void *dst, volatile void *src, int sz)
{
    if (sz & 63)
        sz = (sz & -64) + 64;
    asm volatile(
        "NEONCopyPLD: \n"
        " VLDM %[src]!,{d0-d7} \n"
        " VSTM %[dst]!,{d0-d7} \n"
        " SUBS %[sz],%[sz],#0x40 \n"
        " BGT NEONCopyPLD \n"
        : [dst] "+r"(dst), [src] "+r"(src), [sz] "+r"(sz)
        :
        : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "cc", "memory");
}
#endif

int create_dir(const char *_path);

inline std::string get_current_dir()
{
    char buf[1024] = {0};
    char *res = getcwd(buf, 1024);
    assert(res);
    return std::string(buf);
    // char exePath[1024];
    // GetModuleFileName(NULL, exePath, 1023);
    // (strrchr(exePath, '\\'))[1] = 0;
    // return std::string(exePath);
}

inline bool is_file_exist(const char *_file)
{
    FILE *fp = fopen(_file, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

inline char *read_binary_file(const char *_file, int &length)
{ //需要手动释放内存
    char *pRes = nullptr;

    std::ifstream i_f_stream(_file, std::ifstream::binary);
    assert(i_f_stream.is_open());

    i_f_stream.seekg(0, i_f_stream.end);
    length = i_f_stream.tellg();
    i_f_stream.seekg(0, i_f_stream.beg);

    pRes = new char[length];
    i_f_stream.read(pRes, length);

    i_f_stream.close();

    return pRes;
}

inline void write_binary_file(const void *_pData, int _len, const char *_file)
{
    std::ofstream out(_file, std::ios::out | std::ios::binary);
    out.write((char *)_pData, _len);
    out.close();
}

std::vector<std::string> get_dir_file(const char *path, const char *suffix);

inline uint64_t rdtsc()
{
#ifdef _ARM64
    int64_t virtual_timer_value;
    asm volatile("mrs %0, cntvct_el0"
                 : "=r"(virtual_timer_value));
    return virtual_timer_value;
#elif defined(__ARM_ARCH)
#if (__ARM_ARCH >= 6) // V6 is the earliest arch that has a standard cyclecount
    uint32_t pmccntr;
    uint32_t pmuseren;
    uint32_t pmcntenset;
    // Read the user mode perf monitor counter access permissions.
    asm volatile("mrc p15, 0, %0, c9, c14, 0"
                 : "=r"(pmuseren));
    if (pmuseren & 1) { // Allows reading perfmon counters for user mode code.
        asm volatile("mrc p15, 0, %0, c9, c12, 1"
                     : "=r"(pmcntenset));
        if (pmcntenset & 0x80000000ul) { // Is it counting?
            asm volatile("mrc p15, 0, %0, c9, c13, 0"
                         : "=r"(pmccntr));
            // The counter is set up to count every 64th cycle
            return static_cast<int64_t>(pmccntr) * 64; // Should optimize to << 6
        }
    }
#endif
#else
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc"
                         : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#endif
}

inline uint64_t clock_msec()
{
    timespec time1;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
    return time1.tv_sec * 1000 + time1.tv_nsec / 1000000;
}
