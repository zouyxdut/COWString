#pragma once
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <iterator>
#include <type_traits>

template <typename CharT> class BasicCOWString
{
    public:
    class CharProxy;
    typedef CharT value_type;
    typedef size_t size_type;
    BasicCOWString() : _shareable(true)
    {
        _str = alloc(0);
        _ref_count = new std::atomic<size_t>(1);
    }
    BasicCOWString(const CharT *str) : _shareable(true)
    {
        auto sz = strlen(str);
        fromChars(str, sz);
    }
    BasicCOWString(const CharT *str, size_t sz) : _shareable(true)
    {
        fromChars(str, sz);
    }
    BasicCOWString(const BasicCOWString &other) : _shareable(true)
    {
        if (other.Shareable()) {
            _str = other._str;
            _ref_count = other._ref_count;
            _size = other._size;
            ++(*_ref_count);
        } else {
            fromChars(other._str, other._size);
        }
    }
    BasicCOWString(BasicCOWString &&other)
    {
        new (this) BasicCOWString{};
        Swap(other);
    }
    ~BasicCOWString()
    {
        if (--(*_ref_count) == 0) {
            delete[] _str;
            delete _ref_count;
        }
    }
    BasicCOWString &operator=(const BasicCOWString &other)
    {
        if (&other != this) {
            BasicCOWString(other).Swap(*this);
        }
        return *this;
    }
    BasicCOWString &operator=(const CharT *str)
    {
        *this = BasicCOWString(str);
        return *this;
    }

    BasicCOWString &operator=(BasicCOWString &&other)
    {
        if (&other != this) {
            BasicCOWString(std::move(other)).Swap(*this);
        }
        return *this;
    }
    void Swap(BasicCOWString &other) noexcept
    {
        std::swap(_str, other._str);
        std::swap(_ref_count, other._ref_count);
        std::swap(_size, other._size);
        std::swap(_shareable, other._shareable);
    }
    BasicCOWString Clone() const
    {
        return _str;
    }
    bool Shareable() const noexcept
    {
        return _shareable;
    }
    bool IsShared() const noexcept
    {
        return *_ref_count > 1;
    }

    const CharT *c_str() const noexcept
    {
        return _str;
    }

    size_t Size() const noexcept
    {
        return _size;
    }
    size_t UseCount() const
    {
        return _ref_count->load(std::memory_order_relaxed);
    }
    CharProxy operator[](size_t index)
    {
        return CharProxy(this, index);
    }
    const CharProxy operator[](size_t index) const noexcept
    {
        return CharProxy(const_cast<BasicCOWString *>(this), index);
    }
    BasicCOWString &Append(const CharT *str)
    {
        return append(str, strlen(str));
    }
    BasicCOWString &Append(const CharT *str, size_t sz)
    {
        return append(str, sz);
    }
    BasicCOWString &Append(const BasicCOWString &other)
    {
        return append(other.c_str(), other.Size());
    }
    BasicCOWString &operator+=(const BasicCOWString &other)
    {
        return append(other.c_str(), other.Size());
    }

    int Compare(const BasicCOWString &other) const noexcept
    {
        return Compare(other._str);
    }
    int Compare(const CharT *str) const noexcept
    {
        return ::strcmp(_str, str);
    }
    BasicCOWString SubStr(size_t pos, size_t count = npos)
    {
        if (pos >= _size) {
            throw std::out_of_range("bad position: " + std::to_string(pos));
        }
        if (count == npos || pos + count > _size) {
            count = _size - pos;
        }
        return BasicCOWString(_str + pos, count);
    }
    friend std::ostream &operator<<(std::ostream &o, const BasicCOWString &str)
    {
        o << str._str;
        return o;
    }
    template <typename V> class IteratorBase
    {
public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef V value_type;
        typedef ptrdiff_t difference_type;
        typedef V *pointer;
        typedef std::conditional_t<std::is_const<V>::value, const CharProxy, CharProxy> reference;
        typedef std::conditional_t<std::is_const<V>::value,
                                   const BasicCOWString<std::remove_const_t<V>>,
                                   BasicCOWString<std::remove_const_t<V>>>
            str_type;
        IteratorBase() = default;
        explicit IteratorBase(str_type *pstr, typename str_type::size_type index) noexcept
                : _pstr(pstr), _index(index)
        {
        }
        reference operator*() const
        {
            return (*_pstr)[_index];
        }
        IteratorBase &operator++() noexcept
        {
            ++_index;
            return *this;
        }
        IteratorBase operator++(int) noexcept
        {
            return IteratorBase{ _pstr, _index++ };
        }
        IteratorBase &operator--() noexcept
        {
            --_index;
            return *this;
        }
        IteratorBase operator--(int) noexcept
        {
            return IteratorBase{ _pstr, _index-- };
        }
        friend IteratorBase operator+(const IteratorBase &it, difference_type pos)
        {
            return IteratorBase{ it._pstr, it._index + pos };
        }
        IteratorBase operator+=(difference_type pos) noexcept
        {
            _index += pos;
            return *this;
        }
        typedef std::conditional_t<std::is_const<V>::value, IteratorBase<std::remove_const_t<V>>,
                                   IteratorBase<const V>>
            opposite_type;
#define BINARY_OP(x, cond)                                                                       \
    friend bool operator x(const IteratorBase &lhs, const IteratorBase &rhs)                     \
    {                                                                                            \
        return cond;                                                                             \
    }                                                                                            \
    friend bool operator x(const IteratorBase &lhs, const opposite_type &rhs)                    \
    {                                                                                            \
        return cond;                                                                             \
    }
        BINARY_OP(==, lhs._pstr == rhs._pstr && lhs._index == rhs._index);
        BINARY_OP(!=, lhs._pstr != rhs._pstr || lhs._index != rhs._index);
        BINARY_OP(<, lhs._pstr->c_str() + lhs._index < rhs._pstr->c_str() + rhs._index);
        BINARY_OP(<=, lhs._pstr->c_str() + lhs._index <= rhs._pstr->c_str() + rhs._index);
        BINARY_OP(>, lhs._pstr->c_str() + lhs._index > rhs._pstr->c_str() + rhs._index);
        BINARY_OP(>=, lhs._pstr->c_str() + lhs._index >= rhs._pstr->c_str() + rhs._index);

protected:
        str_type *_pstr;
        typename str_type::size_type _index;
    };
    typedef IteratorBase<value_type> iterator;
    typedef IteratorBase<const value_type> const_iterator;
    iterator begin() noexcept
    {
        return iterator(this, 0);
    }
    const_iterator cbegin() const noexcept
    {
        return const_iterator(this, 0);
    }
    iterator end() noexcept
    {
        return iterator(this, _size);
    }
    const_iterator cend() const noexcept
    {
        return const_iterator(this, _size);
    }
    class CharProxy
    {
public:
        CharProxy() = default;
        CharProxy(BasicCOWString *pstr, size_t index) noexcept : _pstr(pstr), _index(index)
        {
        }
        operator CharT() const noexcept
        {
            return _pstr->_str[_index];
        }
        CharProxy(const CharProxy &) = default;
        CharProxy(CharProxy &&) = default;
        CharProxy &operator=(const CharProxy &other)
        {
            if (_pstr->IsShared()) {
                _pstr->Clone().Swap(*_pstr);
            }
            _pstr->_str[_index] = other._ref[other._index];
            return *this;
        }
        CharProxy &operator=(CharT ch)
        {
            if (_pstr->IsShared()) {
                _pstr->Clone().Swap(*_pstr);
            }
            _pstr->_str[_index] = ch;
            return *this;
        }
        CharT *operator&()
        {
            if (_pstr->IsShared()) {
                _pstr->Clone().Swap(*_pstr);
            }
            _pstr->_shareable = false;
            return &(_pstr->_str[_index]);
        }
        const CharT *operator&() const noexcept
        {
            return &(_pstr->_str[_index]);
        }

private:
        BasicCOWString *_pstr{ nullptr };
        size_t _index{ 0 };
    };

    private:
    CharT *alloc(size_t size)
    {
        auto str = new value_type[size + 1];
        _size = size;
        str[size] = 0; // null terminated
        return str;
    }

    void fromChars(const CharT *str, size_t sz)
    {
        _str = alloc(sz);
        memcpy(_str, str, sz);
        _ref_count = new std::atomic<size_t>(1);
    }
    BasicCOWString &append(const CharT *str, size_t sz)
    {
        auto new_sz = _size + sz;
        auto old_sz = _size;
        auto new_str = alloc(new_sz);
        memcpy(new_str, _str, old_sz);
        memcpy(new_str + old_sz, str, sz);
        this->~BasicCOWString();
        _ref_count = new std::atomic<size_t>(1);
        _str = new_str;
        _shareable = true;
        return *this;
    }
    value_type *_str;
    std::atomic<size_t> *_ref_count;
    size_t _size;
    bool _shareable;
    static const size_t npos = std::numeric_limits<size_t>::max();
};
template <typename CharT>
BasicCOWString<CharT> operator+(const BasicCOWString<CharT> &lhs,
                                const BasicCOWString<CharT> &rhs)
{
    return BasicCOWString<CharT>{ lhs }.Append(rhs);
}

template <typename CharT>
BasicCOWString<CharT> operator+(const CharT *lhs, const BasicCOWString<CharT> &rhs)
{
    return BasicCOWString<CharT>{ rhs }.Append(lhs);
}

template <typename CharT>
BasicCOWString<CharT> operator+(const BasicCOWString<CharT> &lhs, const CharT *rhs)
{
    return BasicCOWString<CharT>{ lhs }.Append(rhs);
}

template <typename CharT>
bool operator==(const BasicCOWString<CharT> &lhs, const BasicCOWString<CharT> &rhs)
{
    return lhs.Compare(rhs) == 0;
}

template <typename CharT> bool operator==(const BasicCOWString<CharT> &lhs, const CharT *rhs)
{
    return lhs.Compare(rhs) == 0;
}

template <typename CharT> bool operator==(const CharT *lhs, const BasicCOWString<CharT> &rhs)
{
    return rhs.Compare(lhs) == 0;
}

using COWString = BasicCOWString<char>;