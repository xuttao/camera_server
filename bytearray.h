/*
 * @Author: xtt
 * @Date: 2020-08-25 20:12:31
 * @Description: ...
 * @LastEditTime: 2022-02-16 14:19:20
 */

#ifndef TCPSERVER_BYTEARRAY_H
#define TCPSERVER_BYTEARRAY_H

#include <cassert>
#include <cstddef>
#include <cstring>
#include <ostream>

class ByteArray
{
public:
    ByteArray() noexcept = default;
    ByteArray(const char *p, int32_t len = -1) noexcept;
    ByteArray(const char c, const size_t len) noexcept;
    ByteArray(const ByteArray &) noexcept;
    ByteArray(ByteArray &&) noexcept;
    ~ByteArray();

private:
    size_t _size = 0;
    size_t _capacity = 0;
    char *_data = NULL;

private:
    inline void construct(const char *p, const size_t size) noexcept;
    void expand(size_t size) noexcept;
    void shrink() noexcept = delete;

public:
    ByteArray &append(char c) noexcept;
    ByteArray &append(const char *s, const int32_t len = -1) noexcept;
    ByteArray &append(const ByteArray &a) noexcept;
    ByteArray &append(ByteArray &&a) noexcept;

    ByteArray &remove(size_t index, size_t len) noexcept;
    ByteArray mid(size_t index, size_t len) const noexcept;
    void swap(ByteArray &) noexcept;
    inline char &operator[](size_t index) noexcept
    {
        assert(index < _size);
        return _data[index];
    }
    ByteArray &operator=(const ByteArray &) noexcept;
    ByteArray &operator=(const char *) noexcept;
    ByteArray &operator=(ByteArray &&) noexcept;
    inline size_t size() const noexcept { return _size; }
    inline bool empty() const noexcept { return 0 == _size; }
    inline char *data() const noexcept { return _data; }
    inline const char at(size_t index) const noexcept
    {
        assert(index < _size);
        return _data[index];
    }
    inline void clear(bool delmem = false) noexcept
    {
        if (delmem) this->~ByteArray();
        _size = 0;
    }

    friend std::ostream &operator<<(std::ostream &, const ByteArray &);

    // static_assert(__cplusplus >= 201703L, "cxx standard < 17");
};

inline void ByteArray::construct(const char *p, const size_t size) noexcept
{
    if (size >= _capacity) expand(size);
    memcpy(_data, p, size);
    _data[size] = '\0';
}

#endif //TCPSERVER_BYTEARRAY_H
