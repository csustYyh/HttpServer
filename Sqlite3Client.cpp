#include "Sqlite3Client.h"
#include "Logger.h"

int CSqlite3Client::Connect(const KeyValue& args) // ����Sqlite3���ݿ�
{
	auto it = args.find("host");
	if (it == args.end()) return -1;
	if (m_db != NULL) return -2; // �ر�֮ǰ������������
	int ret = sqlite3_open(it->second, &m_db); // para1:��"host"����Ӧ��ֵ����Ҫ���ӵ����ݿ�·�� para2:ָ��sqlite3�ṹ���˫��ָ��(sqlite3**)�����ڴ洢�򿪵����ݿ�����ָ��(����)
	if (ret != 0) {
		TRACEE("connect failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -3;
	}
	return 0;
}

int CSqlite3Client::Exec(const Buffer& sql) // �������ӵ�SQLite3���ݿ���ִ��SQL���
{
	printf("sql={%s}\n", (char*)sql);
	if (m_db == NULL) return -1; // ������ݿ�����
	// para1:�Ѵ򿪵����ݿ����ӵ�ָ�� para2:Ҫִ�е�SQL���ָ�룬���԰�����������Ҫ��';'�ָ�
	// para3:�ص�����ָ�룬���ڴ����ѯ��� �ص�������ǩ�����£�int callback(void* data, int argc, char** argv, char** azColName);
	//       data�����ݸ��ص��������û����ݣ���arg����[para4]��ͬ�� argc����ǰ������е����� argv������е�ÿһ�е�ֵ azColName������е�ÿһ�е�����
	// para4:���ݸ��ص��������û�����ָ��,�������������͵�ָ�룬�����ڻص������з�����������Ϣ���ص������ĵ�һ��������
	// para5:ָ��C����ַ�����ָ�룬���ڴ洢������Ϣ
	int ret = sqlite3_exec(m_db, sql, NULL, this, NULL); // ִ��SQL��� ���ﲻ����ִ�к�Ľ����������ص�
	if (ret != SQLITE_OK) { // ִ��SQL���ʧ��
		printf("sql={%s}\n", (char*)sql); // ִ��ʧ�ܵ���䷢����־������
		printf("Exec failed:%d [%s]", ret, sqlite3_errmsg(m_db)); // ������Ϣ������־������
		return -2;
	}
	return 0;
}

// sqlite3_exec() para3:�ص�����
// ��ν �ص���������˼�ǣ�����ִ��*sql��Ӧ�Ĺ������Ȼ�󽫽�����ݸ��ص��������ص��������ݽ���ٽ�һ��ִ�С�
// ������ţ���� ���ص�������������������ģ�����Ҫ��������Ҫ�Ĺ��ܣ�ͨ���ص�������ʵ�֣������ǻ�ȡ���ݿ������Ч��Ϣ����������������
// ������Ҫ�ر�ע����ǣ��ص���������ʱ����ִ��1�Σ����ǻ�ѭ��ִ��n�Σ�������ʹ��select����sql����ʱ����������Ľ������ ���У���ô ��n�У��ͻ�ִ��n�ε� �ص�������
// �ص������Ĳ���һ���� sql��������ִ�н���Ľ�һ������(��SQL����Ѵ����꣬���ǹ��ķ��ؽ����Ҫ�Է��ؽ������һ����������Ҫ�ص�����)
int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table) // �������ӵ�SQLite3���ݿ���ִ�д��н������SQL���
{
	char* errmsg = NULL; // �洢������Ϣ��ָ��
	if (m_db == NULL) return -1; // ������ݿ�����
	printf("sql={%s}\n", (char*)sql);
	ExecParam param(this, result, table); // �ص�������һ������ CSqilite3Clientʵ��ָ�� + ����� + ��
	// sqlite3_exec��ִ��SQL�������У�����ÿһ�еĲ�ѯ����������CSqlite3Client::ExecCallback
	int ret = sqlite3_exec(m_db, sql, // �ص������Ĳ���һ���� sql��������ִ�н���Ľ�һ������
		&CSqlite3Client::ExecCallback, (void*)&param, &errmsg); // sqlite3_exec()�������ģ���ȵ��ص�����ִ������ٷ��أ�����para4��������Ϊ�ֲ�������������Ҫnew
	if (ret != SQLITE_OK) {
		printf("sql={%s}", sql);
		printf("connect failed:%d [%s]", ret, errmsg);
		if (errmsg)sqlite3_free(errmsg);
		return -2;
	}
	if (errmsg)sqlite3_free(errmsg);
	return 0;
}

int CSqlite3Client::StartTransaction() // һ��ʹ��BEGIN TRANSACTION��ʼ��һ�����񣬺�����SQL���ͻᱻ��Ϊ�����������
{									   // ֱ����ʽ���ύ��COMMIT����ع���ROLLBACK���������
	if (m_db == NULL) return -1;
	int ret = sqlite3_exec(m_db, "BEGIN TRANSACTION", 0, 0, NULL); // ��SQLite�У�BEGIN TRANSACTION������ʽ�ؿ�ʼһ������
	if (ret != SQLITE_OK) {
		TRACEE("sql={BEGIN TRANSACTINO}"); 
		TRACEE("BEGIN failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::CommitTransaction() // ִ��COMMIT�Ὣ�����е����в������ñ��浽���ݿ���
{
	if (m_db == NULL) return -1;
	int ret = sqlite3_exec(m_db, "COMMIT TRANSACTION", 0, 0, NULL);
	if (ret != SQLITE_OK) {
		TRACEE("sql={COMMIT TRANSACTINO}");
		TRACEE("BEGIN failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::RollbackTransaction() // ִ��ROLLBACK�᳷�������е����в�����ʹ�����ݿ�ص�����ʼ֮ǰ��״̬
{
	if (m_db == NULL) return -1;
	int ret = sqlite3_exec(m_db, "ROLLBACK TRANSACTION", 0, 0, NULL);
	if (ret != SQLITE_OK) {
		TRACEE("sql={ROLLBACK TRANSACTINO}");
		TRACEE("BEGIN failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::Close()
{
	if (m_db == NULL) return -1;
	int ret = sqlite3_close(m_db);
	if (ret != SQLITE_OK) {
		TRACEE("Close failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	m_db = NULL;
	return 0;
}

bool CSqlite3Client::IsConnected()
{
	return m_db != NULL;
}

// para1�����ݸ��ص��������û����ݣ���sqlite3_exec()��[para4]��ͬ�� count����ǰ������е����� values������е�ÿһ�е�ֵ names������е�ÿһ�е�����
int CSqlite3Client::ExecCallback(void* arg, int count, char** values, char** names) 
{  // �����̬�ص���������Ҫ�����ǽ��������ݸ�����ĳ�Ա�����ص����Ӷ����ó�Ա������������
	ExecParam* param = (ExecParam*)arg; // CSqilite3Clientʵ��ָ��(this) + ����� + ��
	return param->obj->ExecCallback(param->result, param->table, count, names, values); // ͨ��thisָ����ó�Ա����ExecCallback()
}

int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values)
{
	PTable pTable = table.Copy(); // ���Ʊ����ָ��
	if (pTable == nullptr) {
		printf("table %s error!", (const char*)(Buffer)table); // ����������Buffer() ������õ����Ǳ������
		return -1;
	}
	// ÿһ�н����Ӧһ��table
	for (int i = 0; i < count; i++) { // ��������е�ÿһ��
		Buffer name = names[i]; // ��ǰ���������е�����
		auto it = pTable->Fields.find(name); // ͨ���������Ҷ�Ӧ����
		if (it == pTable->Fields.end()) {  // pTable->Fields �������ж���ָ���ӳ��� std::map < Buffer, PField >;
			printf("table %s error!", (const char*)(Buffer)table);
			return -2;
		}
		if (values[i] != NULL) // ����ж�Ӧ������ֵ
			it->second->LoadFromStr(values[i]); // ������е���ֵ���ص�������Ӧ���ж�����
	}
	result.push_back(pTable); // ��ÿһ�н�������ı������ӵ������
	return 0;
}

_sqlite3_table_::_sqlite3_table_(const _sqlite3_table_& table) // ��ĸ��ƹ��캯��
{
	Database = table.Database;
	Name = table.Name;
	for (size_t i = 0; i < table.FieldDefine.size(); i++) // FieldDefine��һ���ж���ָ�����飬����˱�������ж���
	{
		// 1..get()��ȡָ��shared_ptr������ж����ԭʼָ�� 2.*(_sqlite3_field_*)��ʾ�Ի�ȡ��ԭʼָ����н����ã�����ȡ��shared_ptr������ж���
		// 3.new _sqlite3_field_()��ͨ�����ƹ��캯������һ���µ�_sqlite3_field_������������ԭʼָ����ָ��Ķ�����ͬ
		// 4.���´�����_sqlite3_field_�����ָ���װ��һ������ָ��PField�У��Ӷ�ȷ����Դ�İ�ȫ����
		PField field = PField(new _sqlite3_field_(*
			(_sqlite3_field_*)table.FieldDefine[i].get())); 
		FieldDefine.push_back(field); // �ж���ָ��������
		Fields[field->Name] = field; // ��¼�������ж���ָ���ӳ���ϵ Fields��һ���������ж���ָ���ӳ��map
	}
}

Buffer _sqlite3_table_::Create() // ����������
{
	// CREATE TABLE IF NOT EXISTS ��ȫ�� (�ж���, ...);  --->Sqlite3�Ĵ���������
	// ��ȫ�� = ���ݿ�.����
	Buffer sql = "CREATE TABLE IF NOT EXISTS " + (Buffer)*this + "(\r\n"; // ������Buffer() �ʷ��ص���thisָ�������ȫ��
	for (size_t i = 0; i < FieldDefine.size(); i++) { // ������ǰ����ж���ָ������
		if (i > 0) sql += ","; // �ӵڶ����ж��忪ʼ��ǰ��Ҫ��','
		sql += FieldDefine[i]->Create(); // �����е����
	}
	sql += ");";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Drop() // ɾ��������
{
	Buffer sql = "DROP TABLE " + (Buffer)*this + ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Insert(const _Table_& values)
{	//INSERT INTO ��ȫ�� (��1,...,��n)
	//VALUES(ֵ1,...,ֵn);
	Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_INSERT) { // ����֧�ֲ������
			if (!isfirst) sql += ',';
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i];
		}
	}
	sql += ") VALUES(";
	isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_INSERT) { // ����֧�ֲ������
			if (!isfirst) sql += ',';
			else isfirst = false;
			sql += values.FieldDefine[i]->toSqlStr();
		}
	}
	sql += " );";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Delete(const _Table_& values)
{// DELETE FROM ��ȫ�� WHERE ����
	Buffer sql = "DELETE FROM " + (Buffer)*this + " ";
	Buffer Where = "";
	bool isfirst = true;
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		if (FieldDefine[i]->Condition & SQL_CONDITION) { // ����֧����Ϊ����
			if (!isfirst) Where += " AND ";
			else isfirst = false;
			Where += (Buffer)*FieldDefine[i] + "=" + FieldDefine[i]->toSqlStr();
		}
	}
	if (Where.size() > 0)
		sql += " WHERE " + Where;
	sql += ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}


Buffer _sqlite3_table_::Modify(const _Table_& values)
{	//UPDATE ��ȫ�� SET ��1=ֵ1 , ... , ��n=ֵn [WHERE ����];
	Buffer sql = "UPDATE " + (Buffer)*this + " SET ";
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i ++ ) {
		if (values.FieldDefine[i]->Condition & SQL_MODIFY) {
			if (!isfirst) sql += ",";
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr(); // **�� == **ֵ
		}
	}
	Buffer Where = "";
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_CONDITION) {
			if (!isfirst)Where += " AND ";
			else isfirst = false;
			Where += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr();
		}
	}
	if (Where.size() > 0)
		sql += " WHERE " + Where;
	sql += " ;";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Query(const Buffer& condition)
{//SELECT ����1 ,����2 ,... ,����n FROM ��ȫ��;  // �����ǲ����Ǵ���ѯ�����������������һ���������������
	Buffer sql = "SELECT";
	for (size_t i = 0; i < FieldDefine.size(); i++)
	{
		if (i > 0) sql += ','; // �ӵڶ���������ʼ��ǰ��Ҫ��','
		sql += '"' + FieldDefine[i]->Name + "\" ";
	}
	sql += " FROM " + (Buffer)*this + ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

PTable _sqlite3_table_::Copy() const
{
	return PTable(new _sqlite3_table_(*this)); // ���ñ�ĸ��ƹ��캯��
}

void _sqlite3_table_::ClearFieldUsed()
{
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		FieldDefine[i]->Condition = 0; // �����е�״̬��0
	}
}

_sqlite3_table_::operator const Buffer() const
{
	Buffer Head;
	if (Database.size()) // ָ�������ݿ� ȫ����Ҫ�������ݿ������
		Head = '"' + Database + "\".";
	return Head + '"' + Name + '"';
}

_sqlite3_field_::_sqlite3_field_()
	:_Field_() {
	nType = TYPE_NULL;
	Value.Double = 0.0;
}

_sqlite3_field_::_sqlite3_field_(int ntype, const Buffer& name, unsigned attr, const Buffer& type, const Buffer& size, const Buffer& default_, const Buffer& check)
{
	nType = ntype;
	switch (ntype)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.String = new Buffer(); // ��Ҫ���Buffer*
		break;
	}

	Name = name;
	Attr = attr;
	Type = type;
	Size = size;
	Default = default_;
	Check = check;
}

_sqlite3_field_::_sqlite3_field_(const _sqlite3_field_& field)
{
	nType = field.nType;
	switch (field.nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.String = new Buffer();
		*Value.String = *field.Value.String;
		break;
	}

	Name = field.Name;
	Attr = field.Attr;
	Type = field.Type;
	Size = field.Size;
	Default = field.Default;
	Check = field.Check;
}


_sqlite3_field_::~_sqlite3_field_()
{
	switch (nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		if (Value.String) { // ����ʱnew ����ʱ��Ҫ�ͷ�
			Buffer* p = Value.String; 
			Value.String = NULL;
			delete p;
		}
		break;
	}
}

Buffer _sqlite3_field_::Create()
{	// "����" ���� ����
	Buffer sql = '"' + Name + "\" " + Type + " ";
	if (Attr & NOT_NULL) {
		sql += " NOT NULL ";
	}
	if (Attr & DEFAULT) {
		sql += " DEFAULT " + Default + " ";
	}
	if (Attr & UNIQUE) {
		sql += " UNIQUE ";
	}
	if (Attr & PRIMARY_KEY) {
		sql += " PRIMARY KEY ";
	}
	if (Attr & CHECK) {
		sql += " CHECK( " + Check + ") ";
	}
	if (Attr & AUTOINCREMENT) {
		sql += " AUTOINCREMENT ";
	}
	return sql;
}

void _sqlite3_field_::LoadFromStr(const Buffer& str) // ����str����Value
{
	switch (nType)
	{
	case TYPE_NULL:
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		Value.Integer = atoi(str);
		break;
	case TYPE_REAL:
		Value.Double = atof(str);
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
		*Value.String = str;
		break;
	case TYPE_BLOB:
		*Value.String = Str2Hex(str);
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
}

Buffer _sqlite3_field_::toEqualExp() const // ת���ɵ��ںű��ʽ �磺 "Age = 30"
{
	Buffer sql = (Buffer)*this + " = ";
	std::stringstream ss;
	switch (nType)
	{
	case TYPE_NULL:
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		ss << Value.Integer;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL:
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		sql += '"' + *Value.String + "\" ";
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
	return sql;
}

Buffer _sqlite3_field_::toSqlStr() const // ���ֶε�ֵ����������ת��Ϊ��Ӧ�� SQL �ַ�����ʾ��ʽ
{
	Buffer sql = "";
	std::stringstream ss;
	switch (nType)
	{
	case TYPE_NULL:
		sql += " NULL ";
		break;
	case TYPE_BOOL:
	case TYPE_INT:
	case TYPE_DATETIME:
		ss << Value.Integer;
		sql += ss.str() + " ";
		break;
	case TYPE_REAL:
		ss << Value.Double;
		sql += ss.str() + " ";
		break;
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		sql += '"' + *Value.String + "\" ";
		break;
	default:
		TRACEW("type=%d", nType);
		break;
	}
	return sql;
}

_sqlite3_field_::operator const Buffer() const
{
	return '"' + Name + '"';
}

Buffer _sqlite3_field_::Str2Hex(const Buffer& data) const // �������ַ���תʮ�����Ʊ�ʾ
{
	const char* hex = "0123456789ABCDEF";
	std::stringstream ss;
	for (auto ch : data)
		ss << hex[(unsigned char)ch >> 4] << hex[(unsigned char)ch & 0xF];
	return ss.str();
}