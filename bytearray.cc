#include "bytearray.h"

ByteArray::ByteArray(const char *p, int32_t len) noexcept
{
    if (len == -1) {
        len = std::strlen(p);
    }
    _size = (size_t)len;
    construct(p, _size);
}

ByteArray::ByteArray(const char c, const size_t len) noexcept
{
    if (len) {
        _size = len;
        if (len >= _capacity) expand(len);
        memset(_data, c, len);
        _data[len] = '\0';
    }
}

ByteArray::ByteArray(const ByteArray &data) noexcept
{
    if (data.size()) {
        _size = data.size();
        construct(data.data(), _size);
    }
}

ByteArray::ByteArray(ByteArray &&data) noexcept
{
    swap(data);
}

ByteArray::~ByteArray()
{
    if (_data) free(_data);
    _data = NULL;
    _capacity = 0;
    _size = 0;
}

ByteArray &ByteArray::operator=(const ByteArray &a) noexcept
{
    _size = a._size;
    construct(a._data, a._size);
    return *this;
}

ByteArray &ByteArray::operator=(ByteArray &&a) noexcept
{
    this->~ByteArray();
    swap(a);
    return *this;
}

ByteArray &ByteArray::operator=(const char *p) noexcept
{
    _size = strlen(p);
    construct(p, _size);
    return *this;
}

std::ostream &operator<<(std::ostream &os, const ByteArray &a)
{
    os << a._data;
    return os;
}

void ByteArray::expand(size_t len) noexcept
{
    if (len < (_capacity << 1)) {
        _capacity <<= 1;
    } else {
        _capacity = len + 1;
    }
    _data = (char *)realloc(_data, _capacity);
}

void ByteArray::swap(ByteArray &b) noexcept
{
    std::swap(_data, b._data);
    std::swap(_capacity, b._capacity);
    std::swap(_size, b._size);
}

ByteArray &ByteArray::append(char c) noexcept
{
    if (_size + 1 >= _capacity) expand(_size + 1);
    _data[_size] = c;
    _size++;
    _data[_size] = '\0';
    return *this;
}

ByteArray &ByteArray::append(const char *s, int32_t len) noexcept
{
    if (len == -1) len = std::strlen(s);
    if (_size + len >= _capacity) expand(_size + len);
    memcpy(_data + _size, s, len);
    _size += len;
    _data[_size] = '\0';
    return *this;
}

ByteArray &ByteArray::append(const ByteArray &a) noexcept
{
    if (_size + a._size >= _capacity) expand(_size + a._size);
    memcpy(_data + _size, a._data, a._size);
    _size += a._size;
    _data[_size] = '\0';
    return *this;
}

ByteArray &ByteArray::append(ByteArray &&a) noexcept
{
    return append(a._data, a._size);
}

ByteArray &ByteArray::remove(size_t index, size_t len) noexcept
{
    assert(_size >= (len + index));
    if (!index && !len) return *this;
    memcpy(_data + index, _data + index + len, _size - index - len);
    _size -= len;
    _data[_size] = '\0';
    return *this;
}

ByteArray ByteArray::mid(size_t index, size_t len) const noexcept
{
    assert(_size >= (len + index));
    return ByteArray(_data + index, len);
}
