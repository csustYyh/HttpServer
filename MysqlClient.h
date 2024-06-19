#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include <mysql/mysql.h>

// MySQL�����ݿ���
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
	// ���� KeyValue ��һ��string->string��map����¼��"host" "user" "password" "db" "port"�Ȳ��������������ݿ�
	virtual int Connect(const KeyValue& args); // call: mysql_real_connect() 
	// ִ�� sql ��Ҫִ�е� mysql ���
	virtual int Exec(const Buffer& sql); // call: mysql_real_query()
	// �������ִ�� sql ��Ҫִ�е� mysql ��� Result��һ����ָ���б���Ž���� table�����洢ÿһ�еĲ�ѯ���Ȼ�����Result��
	// call:mysql_real_query() mysql_store_result() mysql_num_fields() mysql_fetch_row()
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table); 
	// ��������
	virtual int StartTransaction(); // call��mysql_real_query(&m_db, "BEGIN", 6);
	// �ύ����
	virtual int CommitTransaction(); // call��mysql_real_query(&m_db, "COMMIT", 7);
	// �ع�����
	virtual int RollbackTransaction(); // call��mysql_real_query(&m_db, "ROLLBACK", 9);
	// �ر�����
	virtual int Close(); // call��mysql_close(&m_db);
	// �Ƿ�����
	virtual bool IsConnected();
private:
	MYSQL m_db; // MYSQL �ṹ�壬��ʾһ�� MySQL ���Ӷ���
	bool m_bInit;// Ĭ����false ��ʾû�г�ʼ�� ��ʼ��֮����Ϊtrue����ʾ�Ѿ�����
private:
	class ExecParam { // �ڲ�Ƕ���࣬���ڷ�װ�ص���������Ĳ���(para1) Sqlite3��Ҫ Mysql����Ҫ
	public:
		ExecParam(CMysqlClient* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{}
		CMysqlClient* obj; // ָ���װ�õ�Sqlite3�ͻ���(CSqlite3Client)��ָ��(this �����ھ�̬�ص������е��ó�Ա����)
		Result& result; // �����(�����е�table�ļ���)
		const _Table_& table; // ����Ϣ(ÿһ�еĽ����Ӧһ��table)
	};
};

// mysql�ı���
class _mysql_table_ :
	public _Table_
{
public:
	_mysql_table_() :_Table_() {}
	_mysql_table_(const _mysql_table_& table); // ��ĸ��ƹ��캯��
	virtual ~_mysql_table_();
	// ���ش������SQL��� �ŵ����ݿ����exec()��ȥִ�У�����ʵ�ִ�������������������Ǵ�����
	// CREATE TABLE table_name (column1 datatype constraints, column2 datatype constraints, ... primary key(column_name));
	virtual Buffer Create(); // eg:CREATE TABLE `Persons` ( `ID` INTEGER NOT NULL AUTO_INCREMENT, ..., PRIMARY KEY (`ID`));
	// ɾ����(���ض�Ӧ��SQL���)
	virtual Buffer Drop(); // eg:DROP TABLE `Persons`;
	// ��ɾ�Ĳ�(���ض�Ӧ��SQL���)
	virtual Buffer Insert(const _Table_& values); // eg: INSERT INTO `Persons` (`ID`) VALUES (123);
	virtual Buffer Delete(const _Table_& values); // eg: DELETE FROM `Persons` WHERE `ID` = 123;
	virtual Buffer Modify(const _Table_& values); // eg: UPDATE `Persons` SET `ID`=456 WHERE `ID` = 123;
	virtual Buffer Query(const Buffer& condition); // eg:SELECT `ID` FROM `Persons` WHERE 'ID' = 123;
	// ͨ���������촴����ǰ������һ������  
	virtual PTable Copy() const; //  ÿ�α����������һ������ʱ��������� Copy() ������һ���µı���󸱱���Ȼ����ݲ�ѯ�����������������ֵ
	virtual void ClearFieldUsed(); // ��ձ�������еı�־(���롢�޸ġ���ѯ����) ��Щ��־����main�����õ� eg:value.Fields["user_qq"]->Condition = SQL_MODIFY;
public:
	// ��������ת������ȡ���ȫ��
	virtual operator const Buffer() const;
};

// mysql������
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
	// �ж���(���ض�Ӧ��SQL���)
	virtual Buffer Create(); // eg: `id` INT NOT NULL AUTO_INCREMENT  `name` VARCHAR(255) NOT NULL DEFAULT "default_name"  ��Щ������ڴ�����
	// ����str�����е�ֵ
	virtual void LoadFromStr(const Buffer& str); // ���ڻ�ȡ�����ʱ����������е�ֵ����������Ӧ����
	// where ���ʹ�õ�:���ɵ��ڱ��ʽ
	virtual Buffer toEqualExp() const; // eg:SELECT * FROM table WHERE column_name = value DELETE FROM table WHERE column_name = value
	// ֵת�ַ���
	virtual Buffer toSqlStr() const; // ���ֶε�ֵת�ַ������ڲ�������� INSERT INTO ��ȫ�� (����,...)VALUES(ֵ,...)�� �������Ϊ�˵õ�ֵ
	// ��������ת������ȡ�е�ȫ��
	virtual operator const Buffer() const; // eg: `id`
private:
	Buffer Str2Hex(const Buffer& data) const; // �������ַ���תʮ�����Ʊ�ʾ
	
};

#define DECLARE_TABLE_CLASS(name, base) class name:public base { \
public: \
virtual PTable Copy() const {return PTable(new name(*this));} \
name():base(){Name=#name;

#define DECLARE_MYSQL_FIELD(ntype,name,attr,type,size,default_,check) \
{PField field(new _mysql_field_(ntype, #name, attr, type, size, default_, check));FieldDefine.push_back(field);Fields[#name] = field; }

#define DECLARE_TABLE_CLASS_EDN() }};

// eg��
/*
DECLARE_TABLE_CLASS(Employee, BaseTable)
DECLARE_FIELD(SQL_INSERT, Id, PRIMARY_KEY, TYPE_INT, 0, "NOT NULL", "")
DECLARE_FIELD(SQL_INSERT | SQL_MODIFY, Name, NOT_NULL | DEFAULT, TYPE_VARCHAR, 50, "'Unknown'", "")
DECLARE_FIELD(SQL_INSERT | SQL_MODIFY, Age, NOT_NULL, TYPE_INT, 0, "", "Age > 0")
DECLARE_TABLE_CLASS_END()
*/
// ������һ����Ϊ Employee �����ݿ���࣬�̳��� BaseTable��
// �ñ��������ֶΣ�Id��Name �� Age���ֱ��ʾԱ����ID�����������䡣�ֶ� Id ���������ֶ� Name ��Ĭ��ֵ 'Unknown'���ֶ� Age ������� 0��

