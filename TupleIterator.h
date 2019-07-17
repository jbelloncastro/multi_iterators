
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

    virtual std::unique_ptr<StageHandlerBase>
    next_stage( std::tuple<range<ContainerIt>...>& ) = 0;

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

    std::unique_ptr<BaseType> next_stage( std::tuple<range<ContainerIt>...>& ranges ) override;

    // Member variables
    Range _range;
};

// Creates a StageHandler instance for a particular stage
template < class ValueType, size_t I, bool HasMoreStages, class... ContainerIt >
struct StageFactory;

template < class ValueType, size_t I, class... ContainerIt >
struct StageFactory<ValueType,I,true,ContainerIt...> {
    std::unique_ptr<StageHandlerBase<ValueType, ContainerIt...>>
    operator()( std::tuple<range<ContainerIt>...>& ranges ) {
        return std::make_unique<StageHandler<ValueType, I, ContainerIt...>>( std::get<I>(ranges) );
    }
};

template < class ValueType, size_t I, class... ContainerIt >
struct StageFactory<ValueType,I,false,ContainerIt...> {
    std::unique_ptr<StageHandlerBase<ValueType, ContainerIt...>>
    operator()( std::tuple<range<ContainerIt>...>& ranges ) {
        // No stages left
        return nullptr;
    }
};

template < class ValueType, size_t I, class... ContainerIt >
std::unique_ptr<StageHandlerBase<ValueType, ContainerIt...>>
StageHandler<ValueType, I, ContainerIt...>::next_stage( std::tuple<range<ContainerIt>...>& ranges )
{
    constexpr bool hasMoreStages = I < sizeof...(ContainerIt)-1;
    return StageFactory<ValueType, I+1, hasMoreStages, ContainerIt...>()(ranges);
}

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
        _handler( std::make_unique<StageHandler<value_type, I, ContainerIt...>>(current) )
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
        if( _handler->done() ) {
            _handler = _handler->next_stage(_ranges);
        } else {
            _handler->next_element();
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
        if( _handler->done() )
            _handler = _handler->next_stage(_ranges);
        return _handler->get_element();
    }

    // De-reference
    pointer_type operator->() {
        return &_handler->get_element();
    }

    bool operator!=( const iterator& other ) const {
        return *_handler != *other._handler;
    }

private:
    using handler_base = StageHandlerBase<value_type, ContainerIt...>;

    ranges_tuple&                 _ranges;
    std::unique_ptr<handler_base> _handler;
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
