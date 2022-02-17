/*
 * @Author: xtt
 * @Date: 2021-07-30 13:57:29
 * @Description: ...
 * @LastEditTime: 2021-08-13 18:01:32
 */
#pragma once

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <utility>
namespace slpool
{
#define FIXED_MEM_POOL 1
#define DYNAMIC_MEM_POOL 0
#define MULTI_MEM_POOL 0

#if FIXED_MEM_POOL
    template <class T, size_t BlockSize = 4096>
    class MemPool
    {
    public:
        constexpr MemPool() noexcept = default;
        MemPool(const MemPool &pool) = delete;
        constexpr MemPool(MemPool &&pool) noexcept;
        ~MemPool();

        MemPool &operator=(const MemPool &pool) = delete;
        MemPool &operator=(MemPool &&pool) noexcept;

        void expandBlock() noexcept;

        inline T *allocate();
        inline void deallocate(T *p);

        template <class... Args>
        inline T *newElement(Args &&...args)
        {
            T *buffer = allocate();
            construct(buffer, std::forward<Args>(args)...);
            return buffer;
        }
        inline void delElement(T *p)
        {
            destory(p);
            deallocate(p);
        }

    private:
        template <class... Args>
        inline void construct(T *p, Args &&...args) { new (p) T(std::forward<Args>(args)...); }
        inline void destory(T *p) { p->~T(); }

    private:
        union Obj {
            T _element;
            Obj *_next;
        };
        Obj *_pcurrent_block = nullptr;
        Obj *_pcurrent_obj = nullptr;
        Obj *_plast_obj = nullptr;
        Obj *_pfree_obj = nullptr;

        static_assert(BlockSize >= 2 * sizeof(Obj), "mem size too small");
    };

    template <class T, size_t BlockSize>
    constexpr MemPool<T, BlockSize>::MemPool(MemPool &&pool) noexcept : _pcurrent_block(pool._pcurrent_block),
                                                                        _pcurrent_obj(pool._pcurrent_obj), _plast_obj(pool._plast_obj), _pfree_obj(pool._pfree_obj)
    {
        pool._pcurrent_block = nullptr;
        pool._pcurrent_obj = nullptr;
        pool._plast_obj = nullptr;
        pool._pfree_obj = nullptr;
    }

    template <class T, size_t BlockSize>
    MemPool<T, BlockSize>::~MemPool()
    {
        Obj *temp = _pcurrent_block;
        while (temp != nullptr) {
            Obj *next = temp->_next;
            operator delete(reinterpret_cast<void *>(temp));
            temp = next;
        }
    }

    template <class T, size_t BlockSize>
    MemPool<T, BlockSize> &MemPool<T, BlockSize>::operator=(MemPool &&pool) noexcept
    {
        _pcurrent_block = pool._pcurrent_block;
        _pcurrent_obj = pool._pcurrent_obj;
        _plast_obj = pool._plast_obj;
        _pfree_obj = pool._pfree_obj;

        pool._pcurrent_block = nullptr;
        pool._pcurrent_obj = nullptr;
        pool._plast_obj = nullptr;
        pool._pfree_obj = nullptr;

        return *this;
    }

    template <class T, size_t BlockSize>
    void MemPool<T, BlockSize>::expandBlock() noexcept
    {
        char *expand_buffer = new char[BlockSize];
        reinterpret_cast<Obj *>(expand_buffer)->_next = _pcurrent_block;
        _pcurrent_block = reinterpret_cast<Obj *>(expand_buffer);

        char *body = expand_buffer + sizeof(Obj *);
        uintptr_t temp = reinterpret_cast<uintptr_t>(body);
        size_t align_size = ((alignof(Obj) - temp) % alignof(Obj));
        _pcurrent_obj = reinterpret_cast<Obj *>(body + align_size);
        _plast_obj = reinterpret_cast<Obj *>(expand_buffer + BlockSize - sizeof(Obj) + 1);
    }

    template <class T, size_t BlockSize>
    inline T *MemPool<T, BlockSize>::allocate()
    {
        if (nullptr != _pfree_obj) {
            T *res = reinterpret_cast<T *>(_pfree_obj);
            _pfree_obj = _pfree_obj->_next;
            return res;
        } else {
            if (_pcurrent_obj >= _plast_obj) {
                expandBlock();
            }
            return reinterpret_cast<T *>(_pcurrent_obj++);
        }
    }

    template <class T, size_t BlockSize>
    inline void MemPool<T, BlockSize>::deallocate(T *p)
    {
        if (nullptr != p) {
            reinterpret_cast<Obj *>(p)->_next = _pfree_obj;
            _pfree_obj = reinterpret_cast<Obj *>(p);
        }
    }
#elif DYNAMIC_MEM_POOL
#elif MULTI_MEM_POOL
    // class MemPool
    // {
    // public:
    //     constexpr MemPool() noexcept = default;
    //     MemPool(const MemPool &pool) = delete;
    //     constexpr MemPool(MemPool &&pool) noexcept;
    //     ~MemPool();

    //     MemPool &operator=(const MemPool &pool) = delete;
    //     MemPool &operator=(MemPool &&pool) noexcept;

    // private:
    //     union Obj {
    //         char *_data;
    //         Obj *_next;
    //     };
    //     enum { __ALIGN = 16 };

    // public:
    //     inline char *allocate(size_t size) noexcept;
    //     inline void deallocate(void *p) noexcept;

    // private:
    //     inline size_t roundUp(size_t size) noexcept { return }
    // };
#endif
} // namespace slpool
