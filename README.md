# SQLite3 for Lua

## Installing

Just type `make` for building.

Typing `make install` will copy `sqlite3.so` to `/usr/local/lib/lua/5.2/` by default.

## Usage

To use SQLite3 in your program, you have to include it with `sqlite3 = require("sqlite3")`.

### `sqlite3.open(filename)`

Opens the database in `filename` and returns it.

Returns `nil` and an errormessage in case of errors.

### `database:prepare(sql_string)`

Prepares a statement with `sql_string` and returns it. `sql_string` may contain placeholders like `?`.

### `database:exec(sql_string [, table])`

Executes `sql_string` and returns the first result row. If `table` is given, the values of `table` are bound to the placeholders in `sql_string`.

### `statement:bind(table)`

Binds the values of `table` to the placeholders of the statement.

### `statement:step()`

Returns the next result row of the statement or `nil` if there are no results.

### `statement:reset()`

Resets the statement. Bound variables are not reset.

### `statement:rows()`

Returns an iterator for usage in `for` loops, which returns one result row at a time until there are no more rows left.
