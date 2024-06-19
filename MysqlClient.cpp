#include "MysqlClient.h"
#include <sstream>
#include "Logger.h"

int CMysqlClient::Connect(const KeyValue& args)
{
	if (m_bInit) return -1; // 不要重复初始化
	MYSQL* ret = mysql_init(&m_db); // 初始化 MYSQL 结构体
	if (ret == NULL) return -2; // 初始化失败 返回-2
	// 连接到mysql数据库
	ret = mysql_real_connect(&m_db, 
		args.at("host"),	 // 数据库主机名
		args.at("user"),	 // 用户名
		args.at("password"), // 密码
		args.at("db"), // 数据库名 
		atoi(args.at("port")), // 端口号
		NULL, 
		0);
	// 如果连接失败且有 MySQL 错误
	if ((ret == NULL) && (mysql_errno(&m_db) != 0)) {
		printf("%s %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));// 打印错误信息
		TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db)); // 记录日志
		mysql_close(&m_db); // 关闭 mysql 连接
		bzero(&m_db, sizeof(m_db)); // 清空m_db结构体
		return -3;
	}
	m_bInit = true; // 标记为已初始化
	return 0;
}

int CMysqlClient::Exec(const Buffer& sql) // 在已连接的mysql数据库上执行SQL语句 不关心结果
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, sql, sql.size());
	if (ret != 0) {
		printf("%s %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));// 打印错误信息
		TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db)); // 记录日志
		return -2;
	}
	return 0;
}

int CMysqlClient::Exec(const Buffer& sql, Result& result, const _Table_& table) // 在已连接的mysql数据库上执行带有结果集的SQL语句
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, sql, sql.size()); // 执行sql语句
	if (ret != 0) {
		printf("%s %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));// 打印错误信息
		TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db)); // 记录日志
		return -2;
	}
	MYSQL_RES* res = mysql_store_result(&m_db); // 获取查询结果
	MYSQL_ROW row;
	unsigned num_fields = mysql_num_fields(res); // 遍历结果集
	while ((row = mysql_fetch_row(res)) != NULL) { // 遍历结果集的每一行
		PTable pt = table.Copy(); // 复制表对象指针
		for (unsigned i = 0; i < num_fields; i++) { // 遍历每一行的结果
			if (row[i] != NULL) { // 如果结果行某列上有数据
				pt->FieldDefine[i]->LoadFromStr(row[i]); // 将结果行的列值填充到表对象对应的列对象中
			}
		}
		result.push_back(pt); // 将每一行结果处理后的表对象添加到结果集
	}
	return 0;
}

int CMysqlClient::StartTransaction()
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, "BEGIN", 6);
	if (ret != 0) {
		printf("%s %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));// 打印错误信息
		TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db)); // 记录日志
		return -2;
	}
	return 0;
}

int CMysqlClient::CommitTransaction()
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, "COMMIT", 7);
	if (ret != 0) {
		printf("%s %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));// 打印错误信息
		TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db)); // 记录日志
		return -2;
	}
	return 0;
}

int CMysqlClient::RollbackTransaction()
{
	if (!m_bInit) return -1;
	int ret = mysql_real_query(&m_db, "ROLLBACK", 9);
	if (ret != 0) {
		printf("%s %d %s\n", __FUNCTION__, mysql_errno(&m_db), mysql_error(&m_db));// 打印错误信息
		TRACEE("%d %s", mysql_errno(&m_db), mysql_error(&m_db)); // 记录日志
		return -2;
	}
	return 0;
}

int CMysqlClient::Close()
{
	if (m_bInit) {
		m_bInit = false;
		mysql_close(&m_db);
		bzero(&m_db, sizeof(m_db));
	}
	return 0;
}

bool CMysqlClient::IsConnected()
{
	return m_bInit;
}

_mysql_table_::_mysql_table_(const _mysql_table_& table)
{
	Database = table.Database;
	Name = table.Name;
	for (size_t i = 0; i < table.FieldDefine.size(); i++) // FieldDefine是一个列对象指针数组，存放了表的所有列对象
	{
		// 1..get()获取指向被shared_ptr管理的列对象的原始指针 2.*(_mysql_field_*)表示对获取的原始指针进行解引用，即获取被shared_ptr管理的列对象
		// 3.new _mysql_field_()是通过复制构造函数创建一个新的_mysql_field_对象，其内容与原始指针所指向的对象相同
		// 4.将新创建的_mysql_field_对象的指针封装在一个智能指针PField中，从而确保资源的安全管理
		PField field = PField(new _mysql_field_(*
			(_mysql_field_*)table.FieldDefine[i].get()));
		FieldDefine.push_back(field); // 列对象指针入数组
		Fields[field->Name] = field; // 记录列名和列对象指针的映射关系 Fields是一个列名和列对象指针的映射map
	}
}

_mysql_table_::~_mysql_table_()
{}

Buffer _mysql_table_::Create() // 创建表的语句
{
	// CREATE TABLE IF NOT EXISTS 表全名 (列定义, ..., PRIMARY KEY `主键列名`, UNIQUE INDEX `列名_UNIQUE` (`列名` ASC) VISIBLE );  --->mysql的创建表的语句
	// 表全名 = 数据库.表名
	Buffer sql = "CREATE TABLE IF NOT EXISTS " + (Buffer)*this + "(\r\n"; // 重载了Buffer() 故返回的是this指向表对象的全名
	for (unsigned i = 0; i < FieldDefine.size(); i++) { // 遍历当前表的列对象指针数组
		if (i > 0) sql += ",\r\n"; // 从第二个列定义开始，前面要加','
		sql += FieldDefine[i]->Create(); // 创建列的语句
		if (FieldDefine[i]->Attr & PRIMARY_KEY) {
			sql += ",\r\n PRIMARY KEY (`" + FieldDefine[i]->Name + "`)";
		}
		if (FieldDefine[i]->Attr & UNIQUE) {
			sql += ",\r\n UNIQUE INDEX `" + FieldDefine[i]->Name + "_UNIQUE` (";
			sql += (Buffer)*FieldDefine[i] + " ASC) VISIBLE ";
		}
	}
	sql += ");";
	return sql;
}

Buffer _mysql_table_::Drop() // 删除表的语句
{
	return "DROP TABLE" + (Buffer)*this;
}

Buffer _mysql_table_::Insert(const _Table_& values) // 插入表的语句
{
	// INSERT INTO 表全名 (列名,...)VALUES(值,...)；
	Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) { // 遍历表的每一列
		if (values.FieldDefine[i]->Condition & SQL_INSERT) {
			if (!isfirst) sql += ",";
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i]; // 列名
		}
	}
	sql += ") VALUES (";
	isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_INSERT) {
			if (!isfirst) sql += ",";
			else isfirst = false;
			sql += values.FieldDefine[i]->toSqlStr(); // 得到列值转sql语句的字符串
		}
	}
	sql += " );";
	printf("sql = %s\n", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Delete(const _Table_& values) // 输入要删除的内容values(也是个表对象) 基于values来删除数据库中的表
{
	// DELETE FROM table_name
	// WHERE condition;
	Buffer sql = "DELETE FROM " + (Buffer)*this + " ";
	Buffer Where = "";
	bool isfirst = true;
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		if (FieldDefine[i]->Condition & SQL_CONDITION) { // 如果该列是条件
			if (!isfirst) Where += " AND ";
			else isfirst = false;
			Where += (Buffer)*FieldDefine[i] + "=" + FieldDefine[i]->toSqlStr();
		}
	}
	if (Where.size() > 0)
		sql += " WHERE " + Where;
	sql += ";";
	printf("sql = %s\n", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Modify(const _Table_& values) // 输入更新表 基于更新表去更新数据库
{
	// UPDATE 表全名
	// SET column1 = value1, column2 = value2, ...  (指定要更新的列及其新值)
	// WHERE condition; （指定更新哪些行的条件。如果省略 WHERE 子句，将更新表中的所有行）
	Buffer sql = "UPDATE " + (Buffer)*this + " SET "; // 初始化 SQL 修改语句的前缀
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) { // 遍历表的所有列
		if (values.FieldDefine[i]->Condition & SQL_MODIFY) { // 如果列可修改
			if (!isfirst) sql += ",";
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr(); // 列名 = 新值
		}
	}
	Buffer Where = "";
	for (size_t i = 0; i < values.FieldDefine.size(); i++) { // 遍历表的所有列
		if (values.FieldDefine[i]->Condition& SQL_CONDITION) { // 如果该列是条件列
			if (!isfirst) Where += " AND ";
			else isfirst = false;
			Where += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr();
		}
	}
	if (Where.size() > 0)
		sql += " WHERE " + Where;
	sql += " ;";
	printf("sql = %s\n", (char*)sql);
	return sql;
}

Buffer _mysql_table_::Query(const Buffer& condition)
{
	// SELECT column1, column2, ...
	// FROM table_name
	// WHERE condition
	Buffer sql = "SELECT ";
	for (size_t i = 0; i < FieldDefine.size(); i++)
	{
		if (i > 0)sql += ',';
		sql += '`' + FieldDefine[i]->Name + "` ";
	}
	sql += " FROM " + (Buffer)*this + " ";
	if (condition.size() > 0) {
		sql += " WHERE " + condition;
	}
	sql += ";";
	printf("sql = %s\n", (char*)sql);
	return sql;
}

PTable _mysql_table_::Copy() const
{
	return PTable(new _mysql_table_(*this));
}

void _mysql_table_::ClearFieldUsed()
{
	for (size_t i = 0; i < FieldDefine.size(); i++){
		FieldDefine[i]->Condition = 0;
	}
}

_mysql_table_::operator const Buffer() const
{
	Buffer Head;
	if (Database.size())
		Head = '`' + Database + "`.";
	return Head + '`' + Name + '`';
}

_mysql_field_::_mysql_field_() :_Field_()
{
	nType = TYPE_NULL;
	Value.Double = 0.0;
}

_mysql_field_::_mysql_field_(int ntype, const Buffer& name, unsigned attr, const Buffer& type, const Buffer& size, const Buffer& default_, const Buffer& check)
{
	nType = ntype;
	switch (nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		Value.String = new Buffer();
		break;
	}
	Name = name;
	Attr = attr;
	Type = type;
	Size = size;
	Default = default_;
	Check = check;
}

_mysql_field_::_mysql_field_(const _mysql_field_& field)
{
	nType = field.nType;
	switch (field.nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB: 
		Value.String = new Buffer(); // // 深拷贝指针类型成员变量 避免指针指向同一块内存
		*Value.String = *field.Value.String;
		break;
	}
	Name = field.Name; // std::sting 类型默认的赋值操作符已经实现了深拷贝（即分配新的内存并复制内容）
	Attr = field.Attr;
	Type = field.Type;
	Size = field.Size;
	Default = field.Default;
	Check = field.Check;
}

_mysql_field_::~_mysql_field_()
{
	switch (nType)
	{
	case TYPE_VARCHAR:
	case TYPE_TEXT:
	case TYPE_BLOB:
		if (Value.String) {
			Buffer* p = Value.String;
			Value.String = NULL;
			delete p;
		}
		break;
	}
}

Buffer _mysql_field_::Create()
{
	Buffer sql = "`" + Name + "` " + Type + Size + " ";
	if (Attr & NOT_NULL) {
		sql += "NOT NULL";
	}
	else {
		sql += "NULL";
	}
	// BLOB TEXT GEOMETRY JSON 不能有默认值的
	if ((Attr & DEFAULT) && (Default.size() > 0) && (Type != "BLOB") && (Type != "TEXT") && (Type != "GEOMETRY") && (Type != "JSON"))
	{
		sql += " DEFAULT \"" + Default + "\" ";
	}
	// UNIQUE PRIMARY_KEY 外面处理
	// CHECK mysql 不支持
	if (Attr & AUTOINCREMENT) {
		sql += " AUTO_INCREMENT ";
	}
	return sql;
}

void _mysql_field_::LoadFromStr(const Buffer& str)
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
		printf("type=%d\n", nType);
		break;
	}
}

Buffer _mysql_field_::toEqualExp() const
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
		printf("type=%d\n", nType);
		break;
	}
	return sql;
}

Buffer _mysql_field_::toSqlStr() const
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
		printf("type=%d\n", nType);
		break;
	}
	return sql;
}

_mysql_field_::operator const Buffer() const
{
	return '`' + Name + '`';
}

Buffer _mysql_field_::Str2Hex(const Buffer& data) const
{
	const char* hex = "0123456789ABCDEF";
	std::stringstream ss;
	for (auto ch : data)
		ss << hex[(unsigned char)ch >> 4] << hex[(unsigned char)ch & 0xF];
	return ss.str();
}

