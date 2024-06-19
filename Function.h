#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <functional>
//һ���������麯�����Ժ�ģ�庯�����ԣ�����ͬʱ����
//һ��ģ����������麯��

class CSocketBase;
class Buffer;

class CFunctionBase // ��������
{
public:
    virtual ~CFunctionBase() {}
    virtual int operator()() { return -1; }
    virtual int operator()(CSocketBase*) { return -1; }
    virtual int operator()(CSocketBase*, const Buffer&) { return -1; }
};

////  ����ÿ�����̺�������һ������������ʹ�÷��ͱ��˼�룬ʹ��ģ����
//template <typename _FUNCTION_, typename... _ARGS_>
//class CFunction : public CFunctionBase // ������ ��ģ�������ģ����������廯һ�������࣬��ͬ�Ĳ�����Ӧ�ĺ����಻ͬ
//{
//public:
//    // ���캯�������պ������������Ϊ������Ĳ������������ʼ����Ա���� m_binder
//    // using BinderType = std::function<int()>;
//    CFunction(_FUNCTION_ func, _ARGS_... args) :m_binder(std::forward<_FUNCTION_>(func), std::forward<_ARGS_>(args)...)
//    {}
//    virtual ~CFunction() {}
//    virtual int operator()() { // ���غ������ò�����(��������)��ʹ�����������һ�������� ִ�пɵ��ö��󲢷��ؽ�� eg:CFunction a; int res = a();
//        return m_binder(); // m_binderҲ��һ���ɵ��ö��󣬷���ֵ��int �����棬������STL��æʵ�ֵ�
//    }
//
//    // std::_Bindres_helper ��һ�����ڰ������� C++11 �е� std::bind �����������͵ĸ�����ģ�塣
//    // std::bind �������ڽ�һ���ɵ��ö��󣨺���ָ�롢�������󡢳�Ա�����ȣ���һ��������������γ�һ���µĿɵ��ö���
//    // �� C++11 �У�std::bind �������صĶ���������ǲ�ȷ���ģ���Ϊ�������ڰ󶨵ľ��������Ϊ�˻�ȡ������ͣ�����ʹ�� std::_Bindres_helper ��ģ��
//    // std::_Bindres_helper ��ģ�����Ҫ�������ṩһ�� type ��Ա�����ڻ�ȡ�󶨺�Ŀɵ��ö��������
//
//    // ʹ�� std::_Bindres_helper ��ģ�壬ģ�����Ϊ int:m_binder�ķ������ͣ����������õķ���ֵ����, _FUNCTION_:�ɵ��ö��������, _ARGS_...���ɵ��ö���Ĳ�������  
//    // ������ģ��ʵ��������������Ϊ m_binder ������
//    // ��std::_Bindres_helper<int, _FUCTION_, _ARGS_...>�᷵��һ�����ͣ�ͨ��::type �����������
//    typename std::_Bindres_helper<int, _FUNCTION_, _ARGS_...>::type m_binder; // �ɵ��ö��� �����Ǹ��ݲ���(_FUNCTION_ _ARGS_)�ɱ��
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


