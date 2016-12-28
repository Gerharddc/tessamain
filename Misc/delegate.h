#ifndef DELEGATE_H
#define DELEGATE_H

/*
*	This code is originally from Oliver Mueller (oliver.mueller@gmail.com)
*	and is origanlly hosted on https://github.com/marcmo/delegates .
*
*	It has been been modified for use in this software by Gehard de Clercq (gerharddeclercq@outlook.com)
*/

#include <memory>

/**
* Main template for delgates
*
* \tparam return_type  return type of the function that gets captured
* \tparam params       variadic template list for possible arguments
*                      of the captured function
*/
template<typename return_type, typename... argument_types>
class DelegateBase
{
public:
    virtual return_type operator()(argument_types... args) = 0;

    virtual bool IsObjectDestroyed() const
    {
        // TODO: implement this
        return false;
    }

    virtual bool operator==(const DelegateBase<return_type, argument_types...> &other) const = 0;
};

// Contains the unneccessary details that should not be exposed in the public api
namespace details
{
    template <typename return_type, typename callee_type, typename... argument_types>
    class delegate_member : public DelegateBase<return_type, argument_types...>
    {
    public:
        typedef return_type(callee_type::*func_type)(argument_types...);

        delegate_member(func_type func, callee_type *callee)
            : callee_(callee)
            , func_(func)
        {
        }

        return_type operator()(argument_types... args) override
        {
            // Calling the function is probably dangerous if the callee does not exist so we should
            // check if it has not maybe told us that it has been destroyed
            if (calleeDestroyed_ && *calleeDestroyed_)
                return return_type();
            // TODO: implement this shit

            return (callee_->*func_)(args...);
        }

        bool operator==(const DelegateBase<return_type, argument_types...> &other) const
        {
            if (const delegate_member* derived = dynamic_cast<const delegate_member*>(&other))
                return (derived->callee_ == this->callee_) && (derived->func_ == this->func_);

            return false;
        }

        bool IsObjectDestroyed() const override
        {
            if (calleeDestroyed_)
                return (*calleeDestroyed_);

            return false;
        }

    private:
        callee_type *callee_;
        func_type func_;
        std::shared_ptr<bool> calleeDestroyed_;
    };


    template <typename return_type, typename calle_type, typename... argument_types>
    class delegate_const : public DelegateBase<return_type, argument_types...>
    {
    public:
        typedef return_type(calle_type::*func_type)(argument_types...) const;

        delegate_const(func_type func, const calle_type *callee)
            : callee_(callee)
            , func_(func)
        {
        }

        return_type operator()(argument_types... args) override
        {
            return (callee_->*func_)(args...);
        }

        bool operator==(const DelegateBase<return_type, argument_types...> &other) const
        {
            if (const delegate_const* derived = dynamic_cast<const delegate_const*>(&other))
                return (derived->callee_ == this->callee_) && (derived->func_ == this->func_);

            return false;
        }

    private:
        const calle_type *callee_;
        func_type func_;
    };


    template <typename return_type, typename... argument_types>
    class delegate_free : public DelegateBase<return_type, argument_types...>
    {
    public:
        typedef return_type(*func_type)(argument_types...);

        delegate_free(func_type func)
            : func_(func)
        {

        }

        return_type operator()(argument_types... args) override
        {
            return func_(args...);
        }

        bool operator==(const DelegateBase<return_type, argument_types...> &other) const
        {
            if (const delegate_free* derived = dynamic_cast<const delegate_free*>(&other))
                return (derived->func_ == this->func_);

            return false;
        }

    private:
        func_type func_;
    };
}

/**
* Main template for delgates
*
* \tparam return_type  return type of the function that gets captured
* \tparam params       variadic template list for possible arguments
*                      of the captured function
*/
template <typename return_type, typename... argument_types>
class Delegate
{
public:
    Delegate(DelegateBase<return_type, argument_types...>* _delegate)
    {
        shared_pointer = std::shared_ptr<DelegateBase<return_type, argument_types...>>(_delegate);
    }

    return_type operator()(argument_types... args) const
    {
        return (*shared_pointer.get())(args...);
    }

    bool IsObjectDestroyed() const
    {
        return (shared_pointer.get())->IsObjectDestroyed();
    }

    bool operator==(const Delegate<return_type, argument_types...> &other) const
    {
        return *shared_pointer == *other.shared_pointer;
    }

    Delegate<return_type, argument_types...>&  operator=(const Delegate<return_type, argument_types...> &o)
    {
        shared_pointer = o.shared_pointer;
    }

private:
    // Contains a shared pointer to the deleagte
    std::shared_ptr<DelegateBase<return_type, argument_types...>> shared_pointer;
};


// Base pointer factory methods:

template<typename class_type, typename return_type, typename... argument_types>
DelegateBase<return_type, argument_types...> *make_delegate_base(return_type(class_type::*func)(argument_types...), class_type *obj)
{
    return new details::delegate_member<return_type, class_type, argument_types...>(func, obj);
}

template<typename class_type, typename return_type, typename... argument_types>
DelegateBase<return_type, argument_types...> *make_delegate_base(return_type(class_type::*func)(argument_types...), class_type *obj, std::shared_ptr<bool> calleDestroyed)
{
    return new details::delegate_member<return_type, class_type, argument_types...>(func, obj, calleDestroyed);
}

template<typename class_type, typename return_type, typename... argument_types>
DelegateBase<return_type, argument_types... > *make_delegate_base(return_type(class_type::*func)(argument_types...) const, class_type *obj)
{
    return new details::delegate_const<return_type, class_type, argument_types... >(func, obj);
}

template <typename return_type, typename... argument_types>
DelegateBase<return_type, argument_types...> *make_delegate_base(return_type(*func)(argument_types...))
{
    return new details::delegate_free<return_type, argument_types...>(func);
}


// Shared pointer (normal) delegate factory methods:

template<typename class_type, typename return_type, typename... argument_types>
Delegate<return_type, argument_types...> make_delegate(return_type(class_type::*func)(argument_types...), class_type *obj)
{
    return Delegate<return_type, argument_types...>(new details::delegate_member<return_type, class_type, argument_types...>(func, obj));
}

template<typename class_type, typename return_type, typename... argument_types>
Delegate<return_type, argument_types... > make_delegate(return_type(class_type::*func)(argument_types...) const, class_type *obj)
{
    return Delegate<return_type, argument_types...>(new details::delegate_const<return_type, class_type, argument_types... >(func, obj));
}

template <typename return_type, typename... argument_types>
Delegate<return_type, argument_types...> make_delegate(return_type(*func)(argument_types...))
{
    return Delegate<return_type, argument_types...>(new details::delegate_free<return_type, argument_types...>(func));
}

#endif // DELEGATE_H
