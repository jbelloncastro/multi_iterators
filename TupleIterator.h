
#pragma once

#include <algorithm>
#include <iterator>
#include <memory>

#include <cassert>

namespace util {

template < class Iterator >
struct range {
    Iterator first;
    Iterator last;

    Iterator begin() { return first; }
    Iterator end()   { return last; }
};

template < class ValueType, class... ContainerIt >
struct StageHandlerBase {
    virtual ~StageHandlerBase() = default;

    virtual bool done() const = 0;

    virtual ValueType& get_element() = 0;

    virtual void next_element() = 0;

    virtual void next_stage( std::tuple<range<ContainerIt>...>&, void* ) = 0;

    virtual bool operator!=( const StageHandlerBase& other ) const = 0;
};

template < class ValueType, size_t I, class... ContainerIt >
struct StageHandler final : public StageHandlerBase<ValueType, ContainerIt...> {
    using Range = std::tuple_element_t<I,std::tuple<range<ContainerIt>...>>;
    using BaseType = StageHandlerBase<ValueType, ContainerIt...>;

    StageHandler( Range range ) :
        _range(range)
    {
    }

    bool done() const override {
        return _range.first == _range.last;
    }

    ValueType& get_element() override {
        return *_range.first;
    }
    
    void next_element() override {
        _range.first++;
    }

    bool operator!=( const BaseType& other ) const override {
        const StageHandler* o = dynamic_cast<const StageHandler*>(&other);
        if( !o ) {
            return true;
        } else {
            return _range.first != o->_range.first 
                || _range.last != o->_range.last;
        }
    }

    void next_stage( std::tuple<range<ContainerIt>...>& ranges, void* ptr ) override;

    // Member variables
    Range _range;
};

// Creates a StageHandler instance for a particular stage
template < class ValueType, size_t I, bool HasMoreStages, class... ContainerIt >
struct StageFactory;

// Specialization when stages still remain
template < class ValueType, size_t I, class... ContainerIt >
struct StageFactory<ValueType,I,true,ContainerIt...> {
    void operator()( std::tuple<range<ContainerIt>...>& ranges, void* ptr ) {
        new(ptr) StageHandler<ValueType, I, ContainerIt...>( std::get<I>(ranges) );
    }
};

// Specialization when no more stages are left
template < class ValueType, size_t I, class... ContainerIt >
struct StageFactory<ValueType,I,false,ContainerIt...> {
    void operator()( std::tuple<range<ContainerIt>...>& ranges, void* ptr ) {
        // No stages left. Do nothing.
    }
};

// This just calls the factory
template < class ValueType, size_t I, class... ContainerIt >
void StageHandler<ValueType, I, ContainerIt...>::next_stage( std::tuple<range<ContainerIt>...>& ranges, void* ptr )
{
    constexpr bool hasMoreStages = I < sizeof...(ContainerIt)-1;
    StageFactory<ValueType, I+1, hasMoreStages, ContainerIt...>()(ranges, ptr);
}

// Type trait to define a union
template < class ValueType, class... ContainerIt >
struct StageStorage {
    template < class T >
    struct Helper;

    template < size_t... Is >
    struct Helper<std::index_sequence<Is...>> {
        static_assert( sizeof...(Is) == sizeof...(ContainerIt), "Sequences do not match" );
        using type = std::aligned_union_t<0, StageHandler<ValueType, Is, ContainerIt...>...>;
    };

    using type = typename Helper<std::index_sequence_for<ContainerIt...>>::type;
};

// Wraps multiple ranges into a single instance
template < class... ContainerIt >
struct MultiRange {
public:
    class iterator;

    MultiRange( range<ContainerIt>... ranges ) :
        _ranges{ranges...}
    {
    }

    // Copyable
    MultiRange( const MultiRange& ) = default;
    // Moveable
    MultiRange( MultiRange&& ) = default;

    iterator begin();
    iterator end();

private:
    std::tuple<range<ContainerIt>...> _ranges;
};

// Iterates over multiple ranges
template < class... ContainerIt >
class MultiRange<ContainerIt...>::iterator {
private:
    using ranges_tuple = std::tuple<range<ContainerIt>...>;
public:
    using value_type      = std::common_type_t<
                                typename std::iterator_traits<ContainerIt>::value_type...
                            >;
    using reference_type  = value_type&;
    using pointer_type    = value_type*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::forward_iterator_tag;

    iterator() = default;

    // Integral constant serves as a 'tag' to select which iterator in the
    // tuple is started from (for begin() this is 0)
    template < size_t I = 0 >
    iterator( ranges_tuple& ranges, std::integral_constant<size_t,I> stage = {} ) :
        iterator( ranges, std::get<I>(ranges), stage )
    {
    }

    template < size_t I >
    iterator( ranges_tuple& ranges, std::tuple_element_t<I, ranges_tuple> current,
              std::integral_constant<size_t,I> stage ) :
        _ranges( ranges ),
        _handler()
    {
        new(&_handler) StageHandler<value_type, I, ContainerIt...>(current);
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
        handler_base& handler = get_handler();
        if( handler.done() ) {
            // Replace current handler with next stage
            handler.~handler_base();
            handler.next_stage(_ranges, &_handler);
        } else {
            handler.next_element();
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
        handler_base& handler = get_handler();
        if( handler.done() ) {
            // Replace current handler with next stage
            handler.~handler_base();
            handler.next_stage(_ranges, &_handler);
        }
        return handler.get_element();
    }

    // De-reference
    pointer_type operator->() {
        return get_handler().get_element();
    }

    bool operator!=( const iterator& other ) const {
        return get_handler() != other.get_handler();
    }

private:
    using handler_base = StageHandlerBase<value_type, ContainerIt...>;
    using handler_storage = typename StageStorage<value_type, ContainerIt...>::type;

    handler_base& get_handler() {
        return reinterpret_cast<handler_base&>(_handler);
    }

    const handler_base& get_handler() const {
        return reinterpret_cast<const handler_base&>(_handler);
    }

    ranges_tuple&   _ranges;
    handler_storage _handler;
};

template < class... ContainerIt >
inline
typename MultiRange<ContainerIt...>::iterator MultiRange<ContainerIt...>::begin() {
    return iterator(_ranges, std::integral_constant<size_t,0>());
}

template < class... ContainerIt >
inline
typename MultiRange<ContainerIt...>::iterator MultiRange<ContainerIt...>::end() {
    constexpr size_t last_stage = sizeof...(ContainerIt)-1;
    auto last_range = std::get<last_stage>(_ranges);
    last_range.first = last_range.last;

    return iterator(_ranges, last_range, std::integral_constant<size_t,last_stage>());
}


template < class... T >
auto iterate_over( T&... containers ) {
    return MultiRange<decltype(std::begin(containers))...>(
            {range<decltype(std::begin(containers))>{std::begin(containers), std::end(containers)}...}
        );
}

} // namespace util
