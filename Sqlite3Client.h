#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include "sqlite3/sqlite3.h"

// Sqlite3的数据库类
class CSqlite3Client 
	:public CDatabaseClient
{
public:
	CSqlite3Client(const CSqlite3Client&) = delete;
	CSqlite3Client& operator=(const CSqlite3Client&) = delete;
public:
	CSqlite3Client() {
		m_db = NULL;
		m_stmt = NULL;
	}
	virtual ~CSqlite3Client() {
		Close();
	}
public:
	// 连接
	virtual int Connect(const KeyValue& args);
	// 执行
	virtual int Exec(const Buffer& sql);
	// 带结果的执行
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table);
	// 开启事务
	virtual int StartTransaction();
	// 提交事务
	virtual int CommitTransaction();
	// 回滚事务
	virtual int RollbackTransaction();
	// 关闭连接
	virtual int Close();
	// 是否连接
	virtual bool IsConnected();
private:
	// 同HttpParser 回调函数只能是静态或系统调用，C++不能使用系统调用作为成员函数，故回调函数声明为静态
	// C++成员函数有一个隐含的this指针，因此它们的签名与C风格函数指针不兼容
	// 为了在回调函数中使用C++对象的方法，常用的技巧是通过静态回调函数传递对象指针(this)和其他必要的参数
	static int ExecCallback(void* arg, int count, char** names, char** values); // 用于处理执行带结果集的SQL语句时的回调
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values); // 用于处理执行带结果集的SQL语句时的回调
private:
	sqlite3_stmt* m_stmt; // SQLite3数据库的准备语句对象指针，sqlite3_stmt是一种可以直接执行的二进制形式的SQL语句实例
	sqlite3* m_db; // SQLite3数据库连接对象指针
private:
	class ExecParam { // 内部嵌套类，用于封装回调函数所需的参数(para1)
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{}
		CSqlite3Client* obj; // 指向封装好的Sqlite3客户端(CSqlite3Client)的指针(this 用来在静态回调函数中调用成员函数)
		Result& result; // 结果集(所有行的table的集合)
		const _Table_& table; // 表信息(每一行的结果对应一个table)
	};
};

// Sqlite3的表类
class _sqlite3_table_ :
	public _Table_
{
public:
	_sqlite3_table_() :_Table_() {}
	_sqlite3_table_(const _sqlite3_table_& table); // 表的复制构造函数
	virtual ~_sqlite3_table_() {}
	// 返回创建表的SQL语句 放到数据库类的exec()中去执行，才能实现创建表，这个函数本身并不是创建表
	virtual Buffer Create();
	// 删除表(返回对应的SQL语句)
	virtual Buffer Drop();
	// 增删改查(返回对应的SQL语句)
	// TODO：参数进行优化
	virtual Buffer Insert(const _Table_& values);
	virtual Buffer Delete(const _Table_& values);
	// TODO：参数进行优化
	virtual Buffer Modify(const _Table_& values);
	virtual Buffer Query(const Buffer& condition);
	// 创建一个基于表的对象
	virtual PTable Copy() const;
	virtual void ClearFieldUsed(); // 清空表的标志(插入、修改、查询条件)
public:
	// 重载类型转换，获取表的全名
	virtual operator const Buffer() const; 
};

// Sqlite3的列类
class _sqlite3_field_ :
	public _Field_
{
public:
	_sqlite3_field_();
	_sqlite3_field_(
		int ntype,
		const Buffer& name,
		unsigned attr,
		const Buffer& type,
		const Buffer& size,
		const Buffer& default_,
		const Buffer& check
	);
	_sqlite3_field_(const _sqlite3_field_& field);
	virtual ~_sqlite3_field_();
	// 创建列(返回对应的SQL语句)
	virtual Buffer Create();
	// 根据str设置列的值
	virtual void LoadFromStr(const Buffer& str);
	// where 语句使用的 1.生成等于表达式 2.值转字符串
	virtual Buffer toEqualExp() const;
	virtual Buffer toSqlStr() const;
	// 重载类型转换，获取列的全名
	virtual operator const Buffer() const;
private:
	Buffer Str2Hex(const Buffer& data) const;
	union {
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;
	}Value; // 列的值
	int nType; // 列的值的类型
};


// DECLARE_TABLE_CLASS宏 用于声明一个数据库表的类，其中参数 name 是表名，base 是基类。
#define DECLARE_TABLE_CLASS(name, base) class name:public base { \
public: \
virtual PTable Copy() const {return PTable(new name(*this));} \
name():base(){Name=#name;

// DECLARE_FIELD宏 用于声明一个表字段(列)，其中参数包括字段的类型、名称、属性、数据类型、大小、默认值和检查条件
#define DECLARE_FIELD(ntype,name,attr,type,size,default_,check) \
{PField field(new _sqlite3_field_(ntype, #name, attr, type, size, default_, check));FieldDefine.push_back(field);Fields[#name] = field; }

// DECLARE_TABLE_CLASS_END 宏用于结束表的声明
#define DECLARE_TABLE_CLASS_EDN() }};

// eg：
/*
DECLARE_TABLE_CLASS(Employee, BaseTable)
DECLARE_FIELD(SQL_INSERT, Id, PRIMARY_KEY, TYPE_INT, 0, "NOT NULL", "")
DECLARE_FIELD(SQL_INSERT | SQL_MODIFY, Name, NOT_NULL | DEFAULT, TYPE_VARCHAR, 50, "'Unknown'", "")
DECLARE_FIELD(SQL_INSERT | SQL_MODIFY, Age, NOT_NULL, TYPE_INT, 0, "", "Age > 0")
DECLARE_TABLE_CLASS_END()
*/
// 创建了一个名为 Employee 的数据库表类，继承自 BaseTable。
// 该表有三个字段：Id、Name 和 Age，分别表示员工的ID、姓名和年龄。字段 Id 是主键，字段 Name 有默认值 'Unknown'，字段 Age 必须大于 0。