package = "sqlite3"
version = "3.8.6-1"

source = {
  url = "https://github.com/kext/lua-sqlite3/"
}

dependencies = {
  "lua >= 5.2"
}

build = {
  type = "builtin",
  modules = {
    sqlite3 = {
      "src/lua_sqlite3.c",
      "src/sqlite3.c"
    }
  }
}
