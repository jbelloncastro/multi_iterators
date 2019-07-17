
#pragma once

#include <algorithm>
#include <iterator>
#include <memory>
#include <vector>

#include <cassert>

namespace util {

template < class Iterator >
struct range {
    Iterator first;
    Iterator last;

    Iterator begin() { return first; }
    Iterator end()   { return last; }
};

template < class ContainerIt >
struct MultiRange {
public:
    class iterator;

    MultiRange( std::initializer_list<range<ContainerIt>> ranges ) :
        MultiRange(ranges.begin(), ranges.end())
    {
    }

    template < class InputIt >
    MultiRange( InputIt begin, InputIt end ) :
        _num_ranges(std::distance(begin,end)),
        _ranges(new range<ContainerIt>[_num_ranges], deleter)
    {
        std::copy_n( begin, size(), data() );
    }

    // Copyable
    MultiRange( const MultiRange& ) = default;
    // Moveable
    MultiRange( MultiRange&& ) = default;

    iterator begin();
    iterator end();

    range<ContainerIt>* data() { return _ranges.get(); }

    size_t size() { return _num_ranges; }

private:
    static void deleter( range<ContainerIt>* ptr ) {
        delete[] ptr;
    }

    size_t                                _num_ranges;
    std::shared_ptr<range<ContainerIt>> _ranges;
};

template < class ContainerIt >
class MultiRange<ContainerIt>::iterator {
private:
    using ElementIt = decltype(std::declval<range<ContainerIt>>().begin());

public:
    using value_type      = typename std::iterator_traits<ElementIt>::value_type;
    using reference_type  = value_type&;
    using pointer_type    = value_type*;
    using difference_type = std::ptrdiff_t;

    iterator() = default;

    iterator( range<ContainerIt>* range, ElementIt element ) :
        _range_it( range ),
        _element_it( element )
    {
    }

    // Copyable
    iterator( const iterator& other ) = default;
    // Moveable
    iterator( iterator&& other ) = default;

    // Copy assignable
    iterator& operator=( const iterator& ) = default;
    // Move assignable
    iterator& operator=( iterator&& other ) = default;

    // Pre increment
    iterator& operator++() {
        if( _element_it == _range_it->end() ) {
            _range_it++;
            _element_it = _range_it->begin();
        } else {
            _element_it++;
        }
        return *this;
    }

    // Post increment
    iterator operator++(int) {
        iterator tmp(*this);
        ++(*this);
        return tmp;
    }

    // De-reference
    reference_type operator*() {
        if( _element_it == _range_it->end() )
            _element_it = (++_range_it)->begin();
        return *_element_it;
    }

    // De-reference
    ElementIt operator->() {
        return _element_it;
    }

    bool operator!=( const iterator& other ) const {
        return _range_it != other._range_it
            || _element_it != other._element_it;
    }

private:
    range<ContainerIt>* _range_it;
    ElementIt           _element_it;
};

template < class ContainerIt >
inline
typename MultiRange<ContainerIt>::iterator MultiRange<ContainerIt>::begin()
{
    using ElementIt = decltype( data()->begin() );
    range<ContainerIt>* first = data();
    ElementIt element = first? first->begin() : ElementIt();
    return {first, element};
}

template < class ContainerIt >
inline
typename MultiRange<ContainerIt>::iterator MultiRange<ContainerIt>::end()
{
    if( size() > 0 ) {
        range<ContainerIt>* last = data() + (size() -1);
        return iterator( last, last->end() );
    } else {
        return iterator();
    }
}

template < class Container >
auto iterate_over2( std::initializer_list<Container> containers ) {
    using ContainerIt = decltype(std::begin(containers));
    return MultiRange<ContainerIt>(std::begin(containers), std::end(containers));
}

template < class... T >
auto iterate_over( T&... containers ) {
    using ContainerIt = std::common_type_t<decltype(std::begin(containers))...>;
    return MultiRange<ContainerIt>(
            {range<ContainerIt>{std::begin(containers), std::end(containers)}...}
        );
}

} // namespace util
