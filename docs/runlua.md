# runLua 使用说明

## 职责

`runLua.exe` 是与 Lua 版本解耦的启动器。它不包含 Lua 头文件、不链接 `lua.lib`，也不注册任何 `star` 接口，只完成：

1. 解析命令行和扁平配置。
2. 动态加载 Lua DLL。
3. 通过 `GetProcAddress` 绑定最小 Lua C API。
4. 读取并编译 Lua 文件。
5. 传入命令行参数并执行脚本。

升级 Lua 时只需重新构建 `lua.dll` 和使用该 ABI 的模块，`runLua.exe` 无需重新构建。

## 默认行为

| 项目 | 默认值 | 相对路径基准 |
|---|---|---|
| Lua DLL | `star.dll` | `runLua.exe` 所在目录 |
| Lua 文件 | `main.lua` | 当前工作目录 |

自动配置文件按以下顺序查找：

1. 当前工作目录的 `runLua.ini`
2. `runLua.exe` 所在目录的 `runLua.ini`

找到配置后，配置中的相对路径以配置文件目录为基准。

## 配置文件

配置采用 UTF-8 扁平 `key=value` 格式，键不区分大小写。支持空行、以 `#` 或 `;` 开头的注释，也兼容 UTF-8 BOM、UTF-16 LE BOM和系统代码页文本。

```ini
# runLua.ini
LuaDll=star.dll
LuaFile=main.lua
```

| 名称 | 作用 | 参数 | 默认值 |
|---|---|---|---|
| `LuaDll` | 指定提供 Lua C API 的动态库 | DLL 文件路径 | `star.dll` |
| `LuaFile` | 指定要执行的 Lua 文件 | Lua 文件路径 | `main.lua` |

未加引号的值原样保留。双引号值支持 `\n`、`\r`、`\t`、`\\`、`\"` 转义。Windows 路径通常不需要加引号。

## 命令行

```text
runLua.exe [script.lua] [args...]
runLua.exe --config file.ini [args...]
runLua.exe --config file.ini --script file.lua [args...]
runLua.exe --dll runtime.dll --script file.lua [args...]
runLua.exe --help
```

| 选项 | 作用 |
|---|---|
| `--config path` | 显式指定配置文件；文件不存在时立即失败 |
| `--dll path` | 覆盖配置中的 `LuaDll` |
| `--script path` | 覆盖配置中的 `LuaFile` |
| `--` | 后续参数全部传给 Lua，适合传递以 `-` 开头的值 |
| `--help`、`-h`、`/?` | 显示中文帮助，不执行脚本 |

没有显式 `--config` 时，第一个普通位置参数视为脚本路径。显式使用 `--config` 后，普通位置参数直接传给配置指定的脚本；覆盖脚本应使用 `--script`。

Lua 脚本通过可变参数接收 UTF-8 字符串：

```lua
local args = {...}
for index, value in ipairs(args) do
    print(index, value)
end
```

## DLL 导出约定

配置指定的 DLL 必须导出：

| 名称 | 作用 |
|---|---|
| `luaL_newstate` | 创建 Lua 状态 |
| `luaL_openlibs` | 加载 Lua 标准库 |
| `luaL_loadbufferx` | 编译脚本缓冲区 |
| `lua_pcallk` | 保护执行 Lua 代码 |
| `lua_close` | 释放 Lua 状态 |
| `lua_tolstring` | 读取错误文本 |
| `lua_settop` | 调整 Lua 栈 |
| `lua_pushlstring` | 压入路径和脚本参数 |

以下导出可选；存在时，`runLua` 会把自身目录加入 `package.cpath`：

| 名称 | 作用 |
|---|---|
| `lua_getglobal` | 获取 `package` |
| `lua_getfield` | 读取 `package.cpath` |
| `lua_setfield` | 写回 `package.cpath` |

默认 `star.dll` 把这些符号转发到同目录的 `lua.dll`。因此默认只加载 `star.dll`，脚本仍可正常 `require "star"`。

## 编码与路径

- Lua 文件按二进制读取，推荐保存为 UTF-8。
- UTF-8 BOM 会被移除，首行 `#!...` 会被跳过。
- Windows 文件路径使用宽字符 API，支持中文目录。
- Lua 边界字符串统一使用 UTF-8。
- 脚本块名称使用绝对路径，错误堆栈能定位到实际文件。

脚本编译错误或运行时错误输出后，如果标准输入连接到交互式控制台，`runLua` 会自动提示“按任意键继续”并暂停，方便查看错误；在重定向或无人值守环境中不会阻塞。

## 退出码

| 退出码 | 含义 |
|---:|---|
| `0` | 执行成功 |
| `2` | 参数、配置、脚本读取或其他启动错误 |
| `3` | Lua DLL 加载失败、依赖缺失或位数不一致 |
| `4` | DLL 缺少必需的 Lua C API |
| `5` | Lua 编译错误或运行时错误 |

批处理和持续集成应检查退出码。

## 常见问题

| 现象 | 处理 |
|---|---|
| 无配置时找不到 `star.dll` | 将 `runLua.exe`、`star.dll`、`lua.dll` 放在同一目录 |
| `require "star"` 失败 | 使用默认 DLL，并避免覆盖 `package.cpath` |
| Windows 错误 193 | 确认 EXE 和所有 DLL 都是 x64 |
| 配置中的相对路径不正确 | 相对路径应从配置文件所在目录计算 |
| 第一个参数被当成脚本 | 使用 `--config`，或用 `--script` 明确脚本 |
