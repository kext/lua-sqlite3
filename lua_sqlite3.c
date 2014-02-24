#include "lua_sqlite3.h"
#include "sqlite3.h"
#include <stdio.h>

typedef struct {
  sqlite3 *connection;
  int status;
} database;

typedef struct {
  sqlite3_stmt *stmt;
  int status;
  int step;
} statement;

#define STATEMENT_ERROR 0
#define STATEMENT_STEP 1
#define STATEMENT_RESET 2

static const int database_metatable = 1337;
static const int statement_metatable = 666;

static int lua_isdatabase(lua_State *L, int index)
{
  int r;
  if (!lua_getmetatable(L, index))
    return 0;
  lua_rawgetp(L, LUA_REGISTRYINDEX, &database_metatable);
  r = lua_rawequal(L, -1, -2);
  lua_pop(L, 2);
  return r;
}

static int lua_database_open(lua_State *L)
{
  /* Parameter: filename */
  database *db;
  const char *filename;
  if (!lua_isstring(L, -1))
  {
    lua_pushliteral(L, "Invalid parameters.");
    lua_error(L);
    return 0;
  }
  else
  {
    filename = lua_tostring(L, -1);
    db = lua_newuserdata(L, sizeof(database));
    db->status = sqlite3_open(filename, &(db->connection));
    lua_rawgetp(L, LUA_REGISTRYINDEX, &database_metatable);
    lua_setmetatable(L, -2);
    if (db->status == SQLITE_OK)
    {
      return 1;
    }
    else
    {
      lua_pushnil(L);
      lua_pushstring(L, sqlite3_errmsg(db->connection));
      sqlite3_close(db->connection);
      db->status = -1;
      db->connection = 0;
      return 2;
    }
  }
}

static int lua_database_tostring(lua_State *L)
{
  database *db;
  if (!lua_isdatabase(L, 1))
    return 0;
  db = lua_touserdata(L, 1);
  lua_pushfstring(L, "database: %p", db->connection);
  return 1;
}

static int lua_database_gc(lua_State *L)
{
  database *db;
  if (!lua_isdatabase(L, 1))
    return 0;
  db = lua_touserdata(L, 1);
  /* Close database */
  sqlite3_close_v2(db->connection);
  db->status = -1;
  db->connection = 0;
  return 0;
}

static int lua_isstatement(lua_State *L, int index)
{
  int r;
  if (!lua_getmetatable(L, index))
    return 0;
  lua_rawgetp(L, LUA_REGISTRYINDEX, &statement_metatable);
  r = lua_rawequal(L, -1, -2);
  lua_pop(L, 2);
  return r;
}

static int lua_statement_prepare(lua_State *L)
{
  /* Parameter: connection, sql string */
  /* Return: statement */
  database *db;
  statement *stmt;
  const char *s;
  size_t l;
  if (!lua_isdatabase(L, 1) || lua_type(L, 2) != LUA_TSTRING)
  {
    lua_pushliteral(L, "Invalid parameters.");
    lua_error(L);
    return 0;
  }
  db = lua_touserdata(L, 1);
  if (db->status != SQLITE_OK)
  {
    lua_pushliteral(L, "Invalid database.");
    lua_error(L);
    return 0;
  }
  s = lua_tolstring(L, 2, &l);
  stmt = lua_newuserdata(L, sizeof(statement));
  stmt->status = sqlite3_prepare_v2(db->connection, s, l, &(stmt->stmt), 0);
  stmt->step = STATEMENT_STEP;
  lua_rawgetp(L, LUA_REGISTRYINDEX, &statement_metatable);
  lua_setmetatable(L, -2);
  if (stmt->status == SQLITE_OK)
  {
    return 1;
  }
  else
  {
    lua_pushnil(L);
    lua_pushstring(L, sqlite3_errmsg(db->connection));
    return 2;
  }
}

static int lua_statement_bind(lua_State *L)
{
  /* Parameter: statement, table */
  statement *stmt;
  int e, i;
  size_t l;
  const char *s;
  if (!lua_isstatement(L, 1) || !lua_istable(L, 2))
  {
    lua_pushliteral(L, "Invalid parameters.");
    lua_error(L);
    return 0;
  }
  stmt = lua_touserdata(L, 1);
  if (stmt->status != SQLITE_OK)
  {
    lua_pushliteral(L, "Invalid statement.");
    lua_error(L);
    return 0;
  }
  lua_pushnil(L); /* first key */
  while (lua_next(L, 2) != 0)
  {
    /* uses 'key' (at index -2) and 'value' (at index -1) */
    if (lua_type(L, -2) == LUA_TSTRING)
      i = sqlite3_bind_parameter_index(stmt->stmt, lua_tostring(L, -2));
    else if (lua_type(L, -2) == LUA_TNUMBER)
      i = lua_tonumber(L, -2);
    else
      i = 0;
    if (lua_type(L, -1) == LUA_TSTRING)
    {
      s = lua_tolstring(L, -1, &l);
      e = sqlite3_bind_text(stmt->stmt, i, s, l, SQLITE_TRANSIENT);
    }
    else if (lua_type(L, -1) == LUA_TNUMBER)
    {
      e = sqlite3_bind_double(stmt->stmt, i, lua_tonumber(L, -1));
    }
    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }
  lua_pushvalue(L, 1);
  return 1;
}

static int lua_statement_step(lua_State *L)
{
  /* Parameter: statement */
  /* Return: nil or table */
  statement *stmt;
  int i, n, t, s;
  if (!lua_isstatement(L, 1))
  {
    lua_pushliteral(L, "Invalid parameter.");
    lua_error(L);
    return 0;
  }
  stmt = lua_touserdata(L, 1);
  if (stmt->status != SQLITE_OK)
  {
    lua_pushliteral(L, "Invalid statement.");
    lua_error(L);
    return 0;
  }
  if (stmt->step == STATEMENT_STEP)
  {
    s = sqlite3_step(stmt->stmt);
    if (s == SQLITE_ROW)
    {
      n = sqlite3_column_count(stmt->stmt);
      lua_newtable(L);
      for (i = 0; i < n; i++)
      {
        t = sqlite3_column_type(stmt->stmt, i);
        if (t == SQLITE_FLOAT || t == SQLITE_INTEGER)
        {
          lua_pushstring(L, sqlite3_column_name(stmt->stmt, i));
          lua_pushnumber(L, sqlite3_column_double(stmt->stmt, i));
          lua_pushvalue(L, -1);
          lua_rawseti(L, -4, i + 1);
          lua_rawset(L, -3);
        }
        else if (t == SQLITE_BLOB || t == SQLITE_TEXT)
        {
          lua_pushstring(L, sqlite3_column_name(stmt->stmt, i));
          lua_pushlstring(L, sqlite3_column_blob(stmt->stmt, i), sqlite3_column_bytes(stmt->stmt, i));
          lua_pushvalue(L, -1);
          lua_rawseti(L, -4, i + 1);
          lua_rawset(L, -3);
        }
        else if (t == SQLITE_NULL)
        {
          lua_pushstring(L, sqlite3_column_name(stmt->stmt, i));
          lua_pushboolean(L, 0);
          lua_pushvalue(L, -1);
          lua_rawseti(L, -4, i + 1);
          lua_rawset(L, -3);
        }
      }
      return 1;
    }
    else if (s == SQLITE_DONE)
    {
      stmt->step = STATEMENT_RESET;
    }
    else
    {
      stmt->step = STATEMENT_ERROR;
    }
  }
  return 0;
}

static int lua_statement_reset(lua_State *L)
{
  /* Parameter: statement */
  statement *stmt;
  if (!lua_isstatement(L, 1))
  {
    lua_pushliteral(L, "Invalid parameter.");
    lua_error(L);
    return 0;
  }
  stmt = lua_touserdata(L, 1);
  if (stmt->status != SQLITE_OK)
  {
    lua_pushliteral(L, "Invalid statement.");
    lua_error(L);
    return 0;
  }
  if (sqlite3_reset(stmt->stmt) == SQLITE_OK)
    stmt->step = STATEMENT_STEP;
  else
    stmt->step = STATEMENT_ERROR;
  return 0;
}

static int lua_statement_rows(lua_State *L)
{
  /* Parameter: statement */
  /* Return: step, statement */
  statement *stmt;
  if (!lua_isstatement(L, 1))
  {
    lua_pushliteral(L, "Invalid parameter.");
    lua_error(L);
    return 0;
  }
  stmt = lua_touserdata(L, 1);
  if (stmt->status != SQLITE_OK)
  {
    lua_pushliteral(L, "Invalid statement.");
    lua_error(L);
    return 0;
  }
  lua_pushcfunction(L, lua_statement_step);
  lua_pushvalue(L, 1);
  return 2;
}

static int lua_statement_tostring(lua_State *L)
{
  statement *stmt;
  if (!lua_isstatement(L, 1))
    return 0;
  stmt = lua_touserdata(L, 1);
  lua_pushfstring(L, "statement: %p", stmt->stmt);
  return 1;
}

static int lua_statement_gc(lua_State *L)
{
  statement *stmt;
  if (!lua_isstatement(L, 1))
    return 0;
  stmt = lua_touserdata(L, 1);
  sqlite3_finalize(stmt->stmt);
  stmt->status = -1;
  stmt->stmt = 0;
  stmt->step = STATEMENT_ERROR;
  return 0;
}

static int lua_statement_exec(lua_State *L)
{
  int r;
  r = lua_statement_prepare(L);
  if (r != 1)
    return r;
  lua_replace(L, 1);
  if (lua_type(L, 3) == LUA_TTABLE)
  {
    lua_pushvalue(L, 3);
    lua_replace(L, 2);
    lua_settop(L, 2);
    lua_statement_bind(L);
  }
  lua_settop(L, 1);
  return lua_statement_step(L);
}

int luaopen_sqlite3(lua_State *L)
{
  /* Create database metatable */
  lua_newtable(L);
  lua_pushliteral(L, "__tostring");
  lua_pushcfunction(L, lua_database_tostring);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__gc");
  lua_pushcfunction(L, lua_database_gc);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__index");
    lua_newtable(L);
    lua_pushliteral(L, "prepare");
    lua_pushcfunction(L, lua_statement_prepare);
    lua_rawset(L, -3);
    lua_pushliteral(L, "exec");
    lua_pushcfunction(L, lua_statement_exec);
    lua_rawset(L, -3);
  lua_rawset(L, -3);
  lua_rawsetp(L, LUA_REGISTRYINDEX, &database_metatable);
  /* Create statement metatable */
  lua_newtable(L);
  lua_pushliteral(L, "__tostring");
  lua_pushcfunction(L, lua_statement_tostring);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__gc");
  lua_pushcfunction(L, lua_statement_gc);
  lua_rawset(L, -3);
  lua_pushliteral(L, "__index");
    lua_newtable(L);
    lua_pushliteral(L, "bind");
    lua_pushcfunction(L, lua_statement_bind);
    lua_rawset(L, -3);
    lua_pushliteral(L, "step");
    lua_pushcfunction(L, lua_statement_step);
    lua_rawset(L, -3);
    lua_pushliteral(L, "reset");
    lua_pushcfunction(L, lua_statement_reset);
    lua_rawset(L, -3);
    lua_pushliteral(L, "rows");
    lua_pushcfunction(L, lua_statement_rows);
    lua_rawset(L, -3);
  lua_rawset(L, -3);
  lua_rawsetp(L, LUA_REGISTRYINDEX, &statement_metatable);
  /* Create library */
  lua_newtable(L);
  lua_pushliteral(L, "open");
  lua_pushcfunction(L, lua_database_open);
  lua_rawset(L, -3);
  /*lua_pushvalue(L, -1);
  lua_setglobal(L, "sqlite3");*/
  return 1;
}
