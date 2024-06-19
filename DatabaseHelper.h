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

// ���ݿ�ĳ���ӿ��࣬����ִ�о����ҵ��
class CDatabaseClient 
{
public:
	CDatabaseClient(const CDatabaseClient&) = delete; // ��ֹ���ƹ����븳ֵ
	CDatabaseClient& operator=(const CDatabaseClient&) = delete;
public:
	CDatabaseClient() {}
	virtual ~CDatabaseClient() {}
public:
	// ����
	virtual int Connect(const KeyValue& args) = 0;
	// ִ��
	virtual int Exec(const Buffer& sql) = 0;
	// �������ִ��
	virtual int Exec(const Buffer& sql, Result& result, const _Table_& table) = 0;
	// ��������
	virtual int StartTransaction() = 0;
	// �ύ����
	virtual int CommitTransaction() = 0;
	// �ع�����
	virtual int RollbackTransaction() = 0;
	// �ر�����
	virtual int Close() = 0;
	// �Ƿ�����
	virtual bool IsConnected() = 0;
};

// ����еĻ����ʵ��
class _Field_;
using PField = std::shared_ptr<_Field_>; // �ж���ָ��
using FieldArray = std::vector<PField>; // �ж���ָ������
using FieldMap = std::map < Buffer, PField >; // �������ж���ָ���ӳ���

// ��ĳ���ӿ��࣬����ִ�о����ҵ��
class _Table_ {
public:
	_Table_() {}
	virtual ~_Table_() {}
	// ���ش�����SQL��� �ŵ����ݿ����exec()��ȥִ�У�����ʵ�ִ�������������������Ǵ�����
	virtual Buffer Create() = 0;
	// ɾ����(���ض�Ӧ��SQL���)
	virtual Buffer Drop() = 0;
	// ��ɾ�Ĳ�(���ض�Ӧ��SQL���)
	// TODO�����������Ż�
	virtual Buffer Insert(const _Table_& values) = 0;
	virtual Buffer Delete(const _Table_& values) = 0;
	// TODO�����������Ż�
	virtual Buffer Modify(const _Table_& values) = 0;
	virtual Buffer Query(const Buffer& condition) = 0;
	// ����һ�����ڱ�Ķ���
	virtual PTable Copy() const= 0;
	virtual void ClearFieldUsed() = 0;
public:
	// ��������ת������ȡ���ȫ��
	virtual operator const Buffer() const = 0;
public: 
	Buffer Database; // ��������DB������
	Buffer Name; // ����
	FieldArray FieldDefine; // ��������ж���ָ��
	FieldMap Fields; // ���������ж���ָ���ӳ���
};

enum { // ��־λ
	SQL_INSERT = 1,//�������
	SQL_MODIFY = 2,//�޸ĵ���
	SQL_CONDITION = 4//��ѯ������
};

enum { // ��־λ(������) 
	NONE = 0,
	NOT_NULL = 1, // �ǿ�
	DEFAULT = 2, // Ĭ��
	UNIQUE = 4, // Ψһ
	PRIMARY_KEY = 8, // ����
	CHECK = 16, // Լ��
	AUTOINCREMENT = 32 // �Զ�����
};

using SqlType = enum { // �е���������
	TYPE_NULL = 0, // ������
	TYPE_BOOL = 1, 
	TYPE_INT = 2,
	TYPE_DATETIME = 4, // ����ʱ������ 
	TYPE_REAL = 8, // С������
	TYPE_VARCHAR = 16,
	TYPE_TEXT = 32,
	TYPE_BLOB = 64 // �ַ���
};

// �еĳ���ӿ��࣬����ִ�о����ҵ��
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
	// ������(���ض�Ӧ��SQL���)
	virtual Buffer Create() = 0;
	virtual void LoadFromStr(const Buffer& str) = 0;
	// where ���ʹ�õ� 1.���ɵ��ڱ��ʽ 2.����ת�ַ���
	virtual Buffer toEqualExp() const = 0;
	virtual Buffer toSqlStr() const = 0;
	// ��������ת������ȡ�е�ȫ��
	virtual operator const Buffer() const = 0;
public:
	Buffer Name; // ����
	Buffer Type; // ����
	Buffer Size; // ���ݿ�SQL������õĵ�size
	unsigned Attr; // ���ԣ�Ψһ�ԡ��������ǿ�...
	Buffer Default; // Ĭ��ֵ
	Buffer Check; // Լ������
public:
	unsigned Condition; // �������� SQL_INSERT->���� SQL_MODIFY->�޸� SQL_CONDITION->���� �����ֲ��������
	union {
		bool Bool;
		int Integer;
		double Double;
		Buffer* String;
	}Value; // �е�ֵ
	int nType; // �е�ֵ������
};
