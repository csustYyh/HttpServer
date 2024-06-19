#pragma once
#include "Public.h"
#include "DatabaseHelper.h"
#include "sqlite3/sqlite3.h"

// Sqlite3�����ݿ���
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
	// ����
	virtual int Connect(const KeyValue& args);
	// ִ��
	virtual int Exec(const Buffer& sql);
	// �������ִ��
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table);
	// ��������
	virtual int StartTransaction();
	// �ύ����
	virtual int CommitTransaction();
	// �ع�����
	virtual int RollbackTransaction();
	// �ر�����
	virtual int Close();
	// �Ƿ�����
	virtual bool IsConnected();
private:
	// ͬHttpParser �ص�����ֻ���Ǿ�̬��ϵͳ���ã�C++����ʹ��ϵͳ������Ϊ��Ա�������ʻص���������Ϊ��̬
	// C++��Ա������һ��������thisָ�룬������ǵ�ǩ����C�����ָ�벻����
	// Ϊ���ڻص�������ʹ��C++����ķ��������õļ�����ͨ����̬�ص��������ݶ���ָ��(this)��������Ҫ�Ĳ���
	static int ExecCallback(void* arg, int count, char** names, char** values); // ���ڴ���ִ�д��������SQL���ʱ�Ļص�
	int ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values); // ���ڴ���ִ�д��������SQL���ʱ�Ļص�
private:
	sqlite3_stmt* m_stmt; // SQLite3���ݿ��׼��������ָ�룬sqlite3_stmt��һ�ֿ���ֱ��ִ�еĶ�������ʽ��SQL���ʵ��
	sqlite3* m_db; // SQLite3���ݿ����Ӷ���ָ��
private:
	class ExecParam { // �ڲ�Ƕ���࣬���ڷ�װ�ص���������Ĳ���(para1)
	public:
		ExecParam(CSqlite3Client* obj, Result& result, const _Table_& table)
			:obj(obj), result(result), table(table)
		{}
		CSqlite3Client* obj; // ָ���װ�õ�Sqlite3�ͻ���(CSqlite3Client)��ָ��(this �����ھ�̬�ص������е��ó�Ա����)
		Result& result; // �����(�����е�table�ļ���)
		const _Table_& table; // ����Ϣ(ÿһ�еĽ����Ӧһ��table)
	};
};

// Sqlite3�ı���
class _sqlite3_table_ :
	public _Table_
{
public:
	_sqlite3_table_() :_Table_() {}
	_sqlite3_table_(const _sqlite3_table_& table); // ��ĸ��ƹ��캯��
	virtual ~_sqlite3_table_() {}
	// ���ش������SQL��� �ŵ����ݿ����exec()��ȥִ�У�����ʵ�ִ�������������������Ǵ�����
	virtual Buffer Create();
	// ɾ����(���ض�Ӧ��SQL���)
	virtual Buffer Drop();
	// ��ɾ�Ĳ�(���ض�Ӧ��SQL���)
	// TODO�����������Ż�
	virtual Buffer Insert(const _Table_& values);
	virtual Buffer Delete(const _Table_& values);
	// TODO�����������Ż�
	virtual Buffer Modify(const _Table_& values);
	virtual Buffer Query(const Buffer& condition);
	// ����һ�����ڱ�Ķ���
	virtual PTable Copy() const;
	virtual void ClearFieldUsed(); // ��ձ�ı�־(���롢�޸ġ���ѯ����)
public:
	// ��������ת������ȡ���ȫ��
	virtual operator const Buffer() const; 
};

// Sqlite3������
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
	// ������(���ض�Ӧ��SQL���)
	virtual Buffer Create();
	// ����str�����е�ֵ
	virtual void LoadFromStr(const Buffer& str);
	// where ���ʹ�õ� 1.���ɵ��ڱ��ʽ 2.ֵת�ַ���
	virtual Buffer toEqualExp() const;
	virtual Buffer toSqlStr() const;
	// ��������ת������ȡ�е�ȫ��
	virtual operator const Buffer() const;
private:
	Buffer Str2Hex(const Buffer& data) const;
	union {
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;
	}Value; // �е�ֵ
	int nType; // �е�ֵ������
};


// DECLARE_TABLE_CLASS�� ��������һ�����ݿ����࣬���в��� name �Ǳ�����base �ǻ��ࡣ
#define DECLARE_TABLE_CLASS(name, base) class name:public base { \
public: \
virtual PTable Copy() const {return PTable(new name(*this));} \
name():base(){Name=#name;

// DECLARE_FIELD�� ��������һ�����ֶ�(��)�����в��������ֶε����͡����ơ����ԡ��������͡���С��Ĭ��ֵ�ͼ������
#define DECLARE_FIELD(ntype,name,attr,type,size,default_,check) \
{PField field(new _sqlite3_field_(ntype, #name, attr, type, size, default_, check));FieldDefine.push_back(field);Fields[#name] = field; }

// DECLARE_TABLE_CLASS_END �����ڽ����������
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