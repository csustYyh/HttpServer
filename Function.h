#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <functional>
//一个函数：虚函数特性和模板函数特性，不能同时存在
//一个模板类可以有虚函数

class CSocketBase;
class Buffer;

class CFunctionBase // 函数基类
{
public:
    virtual ~CFunctionBase() {}
    virtual int operator()() { return -1; }
    virtual int operator()(CSocketBase*) { return -1; }
    virtual int operator()(CSocketBase*, const Buffer&) { return -1; }
};

////  由于每个进程函数各不一样，我们这里使用泛型编程思想，使用模版类
//template <typename _FUNCTION_, typename... _ARGS_>
//class CFunction : public CFunctionBase // 函数类 该模版类根据模版参数来具体化一个函数类，不同的参数对应的函数类不同
//{
//public:
//    // 构造函数，接收函数及其参数作为自身构造的参数，并用其初始化成员变量 m_binder
//    // using BinderType = std::function<int()>;
//    CFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)
//    {}
//    virtual ~CFunction() {}
//    virtual int operator()() { // 重载函数调用操作符(不带参数)，使对象可以像函数一样被调用 执行可调用对象并返回结果 eg:CFunction a; int res = a();
//        return m_binder(); // m_binder也是一个可调用对象，返回值是int 见下面，具体是STL帮忙实现的
//    }
//
//    // std::_Bindres_helper 是一个用于帮助管理 C++11 中的 std::bind 函数返回类型的辅助类模板。
//    // std::bind 函数用于将一个可调用对象（函数指针、函数对象、成员函数等）与一组参数绑定起来，形成一个新的可调用对象
//    // 在 C++11 中，std::bind 函数返回的对象的类型是不确定的，因为它依赖于绑定的具体情况。为了获取这个类型，可以使用 std::_Bindres_helper 类模板
//    // std::_Bindres_helper 类模板的主要作用是提供一个 type 成员，用于获取绑定后的可调用对象的类型
//
//    // 使用 std::_Bindres_helper 类模板，模板参数为 int:m_binder的返回类型，即函数调用的返回值类型, _FUNCTION_:可调用对象的类型, _ARGS_...：可调用对象的参数类型  
//    // 根据类模版实例化出来的类作为 m_binder 的类型
//    // 即std::_Bindres_helper<int, _FUCTION_, _ARGS_...>会返回一个类型，通过::type 访问这个类型
//    typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder; // 可调用对象 类型是根据参数(_FUNCTION_ _ARGS_)可变的
//    // BinderType m_binder;
//};

template <typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase 
{
public:
    CFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::bind(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...))
    {}
    virtual ~CFunction() {}
    virtual int operator()() { 
        return m_binder(); 
    }

    std::function<int()> m_binder;
};


