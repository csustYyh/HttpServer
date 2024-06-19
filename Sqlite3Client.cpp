#include "Sqlite3Client.h"
#include "Logger.h"

int CSqlite3Client::Connect(const KeyValue& args) // 连接Sqlite3数据库
{
	auto it = args.find("host");
	if (it == args.end()) return -1;
	if (m_db != NULL) return -2; // 关闭之前不允许反复连接
	int ret = sqlite3_open(it->second, &m_db); // para1:与"host"键对应的值，即要连接的数据库路径 para2:指向sqlite3结构体的双重指针(sqlite3**)，用于存储打开的数据库连接指针(出参)
	if (ret != 0) {
		TRACEE("connect failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -3;
	}
	return 0;
}

int CSqlite3Client::Exec(const Buffer& sql) // 在已连接的SQLite3数据库上执行SQL语句
{
	printf("sql={%s}\n", (char*)sql);
	if (m_db == NULL) return -1; // 检查数据库连接
	// para1:已打开的数据库连接的指针 para2:要执行的SQL语句指针，可以包含多条，需要用';'分隔
	// para3:回调函数指针，用于处理查询结果 回调函数的签名如下：int callback(void* data, int argc, char** argv, char** azColName);
	//       data：传递给回调函数的用户数据（与arg参数[para4]相同） argc：当前结果行中的列数 argv：结果行的每一列的值 azColName：结果行的每一列的列名
	// para4:传递给回调函数的用户数据指针,可以是任意类型的指针，用于在回调函数中访问上下文信息（回调函数的第一个参数）
	// para5:指向C风格字符串的指针，用于存储错误消息
	int ret = sqlite3_exec(m_db, sql, NULL, this, NULL); // 执行SQL语句 这里不关心执行后的结果，故无需回调
	if (ret != SQLITE_OK) { // 执行SQL语句失败
		printf("sql={%s}\n", (char*)sql); // 执行失败的语句发给日志服务器
		printf("Exec failed:%d [%s]", ret, sqlite3_errmsg(m_db)); // 错误信息发给日志服务器
		return -2;
	}
	return 0;
}

// sqlite3_exec() para3:回调函数
// 所谓 回调函数的意思是，会先执行*sql对应的功能命令，然后将结果传递给回调函数，回调函数根据结果再进一步执行。
// 这代表着，这个 “回调函数”才是最有意义的，我们要讲我们需要的功能，通过回调函数来实现，不管是获取数据库表中有效信息，还是其他动作。
// 另外需要特别注意的是：回调函数多数时候不是执行1次，而是会循环执行n次，当我们使用select进行sql功能时，往往输出的结果会是 多行，那么 有n行，就会执行n次的 回调函数。
// 回调函数的参数一定是 sql功能命令执行结果的进一步处理(即SQL语句已处理完，我们关心返回结果或要对返回结果做进一步处理，才需要回调函数)
int CSqlite3Client::Exec(const Buffer& sql, Result& result, const _Table_& table) // 在已连接的SQLite3数据库上执行带有结果集的SQL语句
{
	char* errmsg = NULL; // 存储错误消息的指针
	if (m_db == NULL) return -1; // 检查数据库连接
	printf("sql={%s}\n", (char*)sql);
	ExecParam param(this, result, table); // 回调函数第一个参数 CSqilite3Client实例指针 + 结果集 + 表
	// sqlite3_exec在执行SQL语句过程中，对于每一行的查询结果，会调用CSqlite3Client::ExecCallback
	int ret = sqlite3_exec(m_db, sql, // 回调函数的参数一定是 sql功能命令执行结果的进一步处理
		&CSqlite3Client::ExecCallback, (void*)&param, &errmsg); // sqlite3_exec()是阻塞的，会等到回调函数执行完后再返回，所以para4可以声明为局部变量，否则需要new
	if (ret != SQLITE_OK) {
		printf("sql={%s}", sql);
		printf("connect failed:%d [%s]", ret, errmsg);
		if (errmsg)sqlite3_free(errmsg);
		return -2;
	}
	if (errmsg)sqlite3_free(errmsg);
	return 0;
}

int CSqlite3Client::StartTransaction() // 一旦使用BEGIN TRANSACTION开始了一个事务，后续的SQL语句就会被视为属于这个事务
{									   // 直到显式地提交（COMMIT）或回滚（ROLLBACK）这个事务
	if (m_db == NULL) return -1;
	int ret = sqlite3_exec(m_db, "BEGIN TRANSACTION", 0, 0, NULL); // 在SQLite中，BEGIN TRANSACTION用于显式地开始一个事务
	if (ret != SQLITE_OK) {
		TRACEE("sql={BEGIN TRANSACTINO}"); 
		TRACEE("BEGIN failed:%d [%s]", ret, sqlite3_errmsg(m_db));
		return -2;
	}
	return 0;
}

int CSqlite3Client::CommitTransaction() // 执行COMMIT会将事务中的所有操作永久保存到数据库中
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

int CSqlite3Client::RollbackTransaction() // 执行ROLLBACK会撤销事务中的所有操作，使得数据库回到事务开始之前的状态
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

// para1：传递给回调函数的用户数据（与sqlite3_exec()的[para4]相同） count：当前结果行中的列数 values：结果行的每一列的值 names：结果行的每一列的列名
int CSqlite3Client::ExecCallback(void* arg, int count, char** values, char** names) 
{  // 这个静态回调函数的主要作用是将参数传递给具体的成员函数回调，从而利用成员函数处理结果集
	ExecParam* param = (ExecParam*)arg; // CSqilite3Client实例指针(this) + 结果集 + 表
	return param->obj->ExecCallback(param->result, param->table, count, names, values); // 通过this指针调用成员函数ExecCallback()
}

int CSqlite3Client::ExecCallback(Result& result, const _Table_& table, int count, char** names, char** values)
{
	PTable pTable = table.Copy(); // 复制表对象指针
	if (pTable == nullptr) {
		printf("table %s error!", (const char*)(Buffer)table); // 由于重载了Buffer() 故这里得到的是表的名字
		return -1;
	}
	// 每一行结果对应一个table
	for (int i = 0; i < count; i++) { // 遍历结果行的每一列
		Buffer name = names[i]; // 当前遍历到的列的列名
		auto it = pTable->Fields.find(name); // 通过列名查找对应的列
		if (it == pTable->Fields.end()) {  // pTable->Fields 列名与列对象指针的映射表 std::map < Buffer, PField >;
			printf("table %s error!", (const char*)(Buffer)table);
			return -2;
		}
		if (values[i] != NULL) // 结果行对应的列有值
			it->second->LoadFromStr(values[i]); // 将结果行的列值加载到表对象对应的列对象中
	}
	result.push_back(pTable); // 将每一行结果处理后的表对象添加到结果集
	return 0;
}

_sqlite3_table_::_sqlite3_table_(const _sqlite3_table_& table) // 表的复制构造函数
{
	Database = table.Database;
	Name = table.Name;
	for (size_t i = 0; i < table.FieldDefine.size(); i++) // FieldDefine是一个列对象指针数组，存放了表的所有列对象
	{
		// 1..get()获取指向被shared_ptr管理的列对象的原始指针 2.*(_sqlite3_field_*)表示对获取的原始指针进行解引用，即获取被shared_ptr管理的列对象
		// 3.new _sqlite3_field_()是通过复制构造函数创建一个新的_sqlite3_field_对象，其内容与原始指针所指向的对象相同
		// 4.将新创建的_sqlite3_field_对象的指针封装在一个智能指针PField中，从而确保资源的安全管理
		PField field = PField(new _sqlite3_field_(*
			(_sqlite3_field_*)table.FieldDefine[i].get())); 
		FieldDefine.push_back(field); // 列对象指针入数组
		Fields[field->Name] = field; // 记录列名和列对象指针的映射关系 Fields是一个列名和列对象指针的映射map
	}
}

Buffer _sqlite3_table_::Create() // 创建表的语句
{
	// CREATE TABLE IF NOT EXISTS 表全名 (列定义, ...);  --->Sqlite3的创建表的语句
	// 表全名 = 数据库.表名
	Buffer sql = "CREATE TABLE IF NOT EXISTS " + (Buffer)*this + "(\r\n"; // 重载了Buffer() 故返回的是this指向表对象的全名
	for (size_t i = 0; i < FieldDefine.size(); i++) { // 遍历当前表的列对象指针数组
		if (i > 0) sql += ","; // 从第二个列定义开始，前面要加','
		sql += FieldDefine[i]->Create(); // 创建列的语句
	}
	sql += ");";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Drop() // 删除表的语句
{
	Buffer sql = "DROP TABLE " + (Buffer)*this + ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

Buffer _sqlite3_table_::Insert(const _Table_& values)
{	//INSERT INTO 表全名 (列1,...,列n)
	//VALUES(值1,...,值n);
	Buffer sql = "INSERT INTO " + (Buffer)*this + " (";
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_INSERT) { // 该列支持插入操作
			if (!isfirst) sql += ',';
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i];
		}
	}
	sql += ") VALUES(";
	isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i++) {
		if (values.FieldDefine[i]->Condition & SQL_INSERT) { // 该列支持插入操作
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
{// DELETE FROM 表全名 WHERE 条件
	Buffer sql = "DELETE FROM " + (Buffer)*this + " ";
	Buffer Where = "";
	bool isfirst = true;
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		if (FieldDefine[i]->Condition & SQL_CONDITION) { // 该列支持作为条件
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
{	//UPDATE 表全名 SET 列1=值1 , ... , 列n=值n [WHERE 条件];
	Buffer sql = "UPDATE " + (Buffer)*this + " SET ";
	bool isfirst = true;
	for (size_t i = 0; i < values.FieldDefine.size(); i ++ ) {
		if (values.FieldDefine[i]->Condition & SQL_MODIFY) {
			if (!isfirst) sql += ",";
			else isfirst = false;
			sql += (Buffer)*values.FieldDefine[i] + "=" + values.FieldDefine[i]->toSqlStr(); // **列 == **值
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
{//SELECT 列名1 ,列名2 ,... ,列名n FROM 表全名;  // 这里是不考虑带查询条件的情况，即返回一个结果行有所有列
	Buffer sql = "SELECT";
	for (size_t i = 0; i < FieldDefine.size(); i++)
	{
		if (i > 0) sql += ','; // 从第二个列名开始，前面要加','
		sql += '"' + FieldDefine[i]->Name + "\" ";
	}
	sql += " FROM " + (Buffer)*this + ";";
	TRACEI("sql = %s", (char*)sql);
	return sql;
}

PTable _sqlite3_table_::Copy() const
{
	return PTable(new _sqlite3_table_(*this)); // 调用表的复制构造函数
}

void _sqlite3_table_::ClearFieldUsed()
{
	for (size_t i = 0; i < FieldDefine.size(); i++) {
		FieldDefine[i]->Condition = 0; // 所有列的状态清0
	}
}

_sqlite3_table_::operator const Buffer() const
{
	Buffer Head;
	if (Database.size()) // 指定了数据库 全名需要加上数据库的名字
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
		Value.String = new Buffer(); // 需要深拷贝Buffer*
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
		if (Value.String) { // 构造时new 析构时需要释放
			Buffer* p = Value.String; 
			Value.String = NULL;
			delete p;
		}
		break;
	}
}

Buffer _sqlite3_field_::Create()
{	// "名称" 类型 属性
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

void _sqlite3_field_::LoadFromStr(const Buffer& str) // 根据str设置Value
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

Buffer _sqlite3_field_::toEqualExp() const // 转换成等于号表达式 如： "Age = 30"
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

Buffer _sqlite3_field_::toSqlStr() const // 将字段的值根据其类型转换为相应的 SQL 字符串表示形式
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

Buffer _sqlite3_field_::Str2Hex(const Buffer& data) const // 二进制字符串转十六进制表示
{
	const char* hex = "0123456789ABCDEF";
	std::stringstream ss;
	for (auto ch : data)
		ss << hex[(unsigned char)ch >> 4] << hex[(unsigned char)ch & 0xF];
	return ss.str();
}