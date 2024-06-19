#pragma once
#include "Public.h"
#include <map>
#include <list>
#include <memory>
#include <vector>

class _Table_;
using PTable = std::shared_ptr<_Table_>;

using KeyValue = std::map<Buffer, Buffer>;
using Result = std::list<PTable>;

// 数据库的抽象接口类，本身不执行具体的业务
class CDatabaseClient 
{
public:
	CDatabaseClient(const CDatabaseClient&) = delete; // 禁止复制构造与赋值
	CDatabaseClient& operator=(const CDatabaseClient&) = delete;
public:
	CDatabaseClient() {}
	virtual ~CDatabaseClient() {}
public:
	// 连接
	virtual int Connect(const KeyValue& args) = 0;
	// 执行
	virtual int Exec(const Buffer& sql) = 0;
	// 带结果的执行
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table) = 0;
	// 开启事务
	virtual int StartTransaction() = 0;
	// 提交事务
	virtual int CommitTransaction() = 0;
	// 回滚事务
	virtual int RollbackTransaction() = 0;
	// 关闭连接
	virtual int Close() = 0;
	// 是否连接
	virtual bool IsConnected() = 0;
};

// 表和列的基类的实现
class _Field_;
using PField = std::shared_ptr<_Field_>; // 列对象指针
using FieldArray = std::vector<PField>; // 列对象指针数组
using FieldMap = std::map < Buffer, PField >; // 列名与列对象指针的映射表

// 表的抽象接口类，本身不执行具体的业务
class _Table_ {
public:
	_Table_() {}
	virtual ~_Table_() {}
	// 返回创建的SQL语句 放到数据库类的exec()中去执行，才能实现创建表，这个函数本身并不是创建表
	virtual Buffer Create() = 0;
	// 删除表(返回对应的SQL语句)
	virtual Buffer Drop() = 0;
	// 增删改查(返回对应的SQL语句)
	// TODO：参数进行优化
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete(const _Table_& values) = 0;
	// TODO：参数进行优化
	virtual Buffer Modify(const _Table_& values) = 0;
	virtual Buffer Query(const Buffer& condition) = 0;
	// 创建一个基于表的对象
	virtual PTable Copy() const= 0;
	virtual void ClearFieldUsed() = 0;
public:
	// 重载类型转换，获取表的全名
	virtual operator const Buffer() const = 0;
public: 
	Buffer Database; // 表所属的DB的名称
	Buffer Name; // 表名
	FieldArray FieldDefine; // 表的所有列对象指针
	FieldMap Fields; // 列名及其列对象指针的映射表
};

enum { // 标志位
	SQL_INSERT = 1,//插入的列
	SQL_MODIFY = 2,//修改的列
	SQL_CONDITION = 4//查询条件列
};

enum { // 标志位(列属性) 
	NONE = 0,
	NOT_NULL = 1, // 非空
	DEFAULT = 2, // 默认
	UNIQUE = 4, // 唯一
	PRIMARY_KEY = 8, // 主键
	CHECK = 16, // 约束
	AUTOINCREMENT = 32 // 自动增长
};

using SqlType = enum { // 列的数据类型
	TYPE_NULL = 0, // 空类型
	TYPE_BOOL = 1, 
	TYPE_INT = 2,
	TYPE_DATETIME = 4, // 日期时间类型 
	TYPE_REAL = 8, // 小数类型
	TYPE_VARCHAR = 16,
	TYPE_TEXT = 32,
	TYPE_BLOB = 64 // 字符串
};

// 列的抽象接口类，本身不执行具体的业务
class _Field_
{
public:
	_Field_() {}
	_Field_(const _Field_& field) {
		Name = field.Name;
		Type = field.Type;
		Attr = field.Attr;
		Default = field.Default;
		Check = field.Check;
	}
	virtual _Field_& operator=(const _Field_& field) {
		if (this != &field) {
			Name = field.Name;
			Type = field.Type;
			Attr = field.Attr;
			Default = field.Default;
			Check = field.Check;
		}
		return *this;
	}
	virtual ~_Field_() {}
public:
	// 创建列(返回对应的SQL语句)
	virtual Buffer Create() = 0;
	virtual void LoadFromStr(const Buffer& str) = 0;
	// where 语句使用的 1.生成等于表达式 2.列名转字符串
	virtual Buffer toEqualExp() const = 0;
	virtual Buffer toSqlStr() const = 0;
	// 重载类型转换，获取列的全名
	virtual operator const Buffer() const = 0;
public:
	Buffer Name; // 名称
	Buffer Type; // 类型
	Buffer Size; // 数据库SQL语句中用的到size
	unsigned Attr; // 属性：唯一性、主键、非空...
	Buffer Default; // 默认值
	Buffer Check; // 约束条件
public:
	unsigned Condition; // 操作条件 SQL_INSERT->插入 SQL_MODIFY->修改 SQL_CONDITION->条件 这三种操作的组合
	union {
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;
	}Value; // 列的值
	int nType; // 列的值的类型
};
