#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include <mysql/mysql.h>

// MySQL的数据库类
class CMysqlClient
	:public CDatabaseClient
{
public:
	CMysqlClient(const CMysqlClient&) = delete;
	CMysqlClient& operator=(const CMysqlClient&) = delete;
public:
	CMysqlClient() {
		bzero(&m_db, sizeof(m_db)); 
		m_bInit = false;
	}
	virtual ~CMysqlClient() {
		Close();
	}
public:
	// 连接 KeyValue 是一个string->string的map，记录了"host" "user" "password" "db" "port"等参数用于连接数据库
	virtual int Connect(const KeyValue& args); // call: mysql_real_connect() 
	// 执行 sql 即要执行的 mysql 语句
	virtual int Exec(const Buffer& sql); // call: mysql_real_query()
	// 带结果的执行 sql 即要执行的 mysql 语句 Result是一个表指针列表，存放结果集 table用来存储每一行的查询结果然后放入Result中
	// call:mysql_real_query() mysql_store_result() mysql_num_fields() mysql_fetch_row()
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table); 
	// 开启事务
	virtual int StartTransaction(); // call：mysql_real_query(&m_db, "BEGIN", 6);
	// 提交事务
	virtual int CommitTransaction(); // call：mysql_real_query(&m_db, "COMMIT", 7);
	// 回滚事务
	virtual int RollbackTransaction(); // call：mysql_real_query(&m_db, "ROLLBACK", 9);
	// 关闭连接
	virtual int Close(); // call：mysql_close(&m_db);
	// 是否连接
	virtual bool IsConnected();
private:
	MYSQL m_db; // MYSQL 结构体，表示一个 MySQL 连接对象
	bool m_bInit;// 默认是false 表示没有初始化 初始化之后，则为true，表示已经连接
private:
	class ExecParam { // 内部嵌套类，用于封装回调函数所需的参数(para1) Sqlite3需要 Mysql不需要
	public:
		ExecParam(CMysqlClient* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{}
		CMysqlClient* obj; // 指向封装好的Sqlite3客户端(CSqlite3Client)的指针(this 用来在静态回调函数中调用成员函数)
		Result& result; // 结果集(所有行的table的集合)
		const _Table_& table; // 表信息(每一行的结果对应一个table)
	};
};

// mysql的表类
class _mysql_table_ :
	public _Table_
{
public:
	_mysql_table_() :_Table_() {}
	_mysql_table_(const _mysql_table_& table); // 表的复制构造函数
	virtual ~_mysql_table_();
	// 返回创建表的SQL语句 放到数据库类的exec()中去执行，才能实现创建表，这个函数本身并不是创建表
	// CREATE TABLE table_name (column1 datatype constraints, column2 datatype constraints, ... primary key(column_name));
	virtual Buffer Create(); // eg:CREATE TABLE `Persons` ( `ID` INTEGER NOT NULL AUTO_INCREMENT, ..., PRIMARY KEY (`ID`));
	// 删除表(返回对应的SQL语句)
	virtual Buffer Drop(); // eg:DROP TABLE `Persons`;
	// 增删改查(返回对应的SQL语句)
	virtual Buffer Insert(const _Table_& values); // eg: INSERT INTO `Persons` (`ID`) VALUES (123);
	virtual Buffer Delete(const _Table_& values); // eg: DELETE FROM `Persons` WHERE `ID` = 123;
	virtual Buffer Modify(const _Table_& values); // eg: UPDATE `Persons` SET `ID`=456 WHERE `ID` = 123;
	virtual Buffer Query(const Buffer& condition); // eg:SELECT `ID` FROM `Persons` WHERE 'ID' = 123;
	// 通过拷贝构造创建当前表对象的一个副本  
	virtual PTable Copy() const; //  每次遍历结果集的一行数据时，都会调用 Copy() 来创建一个新的表对象副本，然后根据查询结果填充这个副本的列值
	virtual void ClearFieldUsed(); // 清空表的所有列的标志(插入、修改、查询条件) 这些标志是在main中设置的 eg:value.Fields["user_qq"]->Condition = SQL_MODIFY;
public:
	// 重载类型转换，获取表的全名
	virtual operator const Buffer() const;
};

// mysql的列类
class _mysql_field_ :
	public _Field_
{
public:
	_mysql_field_();
	_mysql_field_(
		int ntype,
		const Buffer& name,
		unsigned attr,
		const Buffer& type,
		const Buffer& size,
		const Buffer& default_,
		const Buffer& check
	);
	_mysql_field_(const _mysql_field_& field);
	virtual ~_mysql_field_();
	// 列定义(返回对应的SQL语句)
	virtual Buffer Create(); // eg: `id` INT NOT NULL AUTO_INCREMENT  `name` VARCHAR(255) NOT NULL DEFAULT "default_name"  这些语句用于创建表
	// 根据str设置列的值
	virtual void LoadFromStr(const Buffer& str); // 用于获取结果集时，将结果集中的值赋给结果表对应的列
	// where 语句使用的:生成等于表达式
	virtual Buffer toEqualExp() const; // eg:SELECT * FROM table WHERE column_name = value DELETE FROM table WHERE column_name = value
	// 值转字符串
	virtual Buffer toSqlStr() const; // 将字段的值转字符串用于插入表的语句 INSERT INTO 表全名 (列名,...)VALUES(值,...)； 这里就是为了得到值
	// 重载类型转换，获取列的全名
	virtual operator const Buffer() const; // eg: `id`
private:
	Buffer Str2Hex(const Buffer& data) const; // 二进制字符串转十六进制表示
	
};

#define DECLARE_TABLE_CLASS(name, base) class name:public base { \
public: \
virtual PTable Copy() const {return PTable(new name(*this));} \
name():base(){Name=#name;

#define DECLARE_MYSQL_FIELD(ntype,name,attr,type,size,default_,check) \
{PField field(new _mysql_field_(ntype, #name, attr, type, size, default_, check));FieldDefine.push_back(field);Fields[#name] = field; }

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

