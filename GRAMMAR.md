<!--
 Copyright (C) 2025 TG11
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.
 
 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
-->

statement     := create_table | drop_table | create_index | drop_index
               | insert | update | delete | select | tx | pragma

create_table  := "CREATE" "TABLE" ident "(" col_def {"," col_def} ["," pk_def] ")"
col_def       := ident type [ "NOT" "NULL" ]
pk_def        := "PRIMARY" "KEY" "(" ident ")"
type          := "INT" | "TEXT" | "BOOL" | "REAL" | "BYTEA"

drop_table    := "DROP" "TABLE" ident

create_index  := "CREATE" "INDEX" ident "ON" ident "(" ident ")"
drop_index    := "DROP" "INDEX" ident

insert        := "INSERT" "INTO" ident ["(" ident {"," ident} ")"]
                 "VALUES" row_values {"," row_values}
row_values    := "(" expr {"," expr} ")"

update        := "UPDATE" ident "SET" set_item {"," set_item} [where]
set_item      := ident "=" expr

delete        := "DELETE" "FROM" ident [where]

select        := "SELECT" sel_list "FROM" ident [where] [order] [limit]
sel_list      := "*" | expr [ "AS" ident ] {"," expr [ "AS" ident ]}
where         := "WHERE" bool_expr
order         := "ORDER" "BY" ident [ "ASC" | "DESC" ]
limit         := "LIMIT" integer

tx            := "BEGIN" | "COMMIT" | "ROLLBACK"
pragma        := "PRAGMA" ("tables" | "schema" ident)
