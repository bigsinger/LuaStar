# 构建、部署与升级

## 构建基线

| 项目 | 要求 |
|---|---|
| IDE | Visual Studio 2026 |
| Platform Toolset | `v145` |
| 平台 | x64 |
| 字符集 | Unicode，源码按 UTF-8 编译 |
| C++ 标准 | `/std:c++latest` |
| C 标准 | `/std:clatest` |
| Debug CRT | `/MTd` |
| Release CRT | `/MT` |
| Lua | 官方 Lua 5.5.0 源码 |

在 Visual Studio Developer PowerShell 中执行：

```powershell
msbuild .\LuaStar.sln /m /t:Build /p:Configuration=Release /p:Platform=x64
msbuild .\LuaStar.sln /m /t:Build /p:Configuration=Debug /p:Platform=x64
```

产物分别位于 `bin\Release` 和 `bin\Debug`。

## 工程结构

```text
LuaStar.sln
Project.Common.props
lua/             Lua DLL 工程和兼容导出
runLua/          动态启动器和扁平配置解析
star/            轻量接口的单文件实现
third_party/     官方 Lua 源码
docs/            中文文档
examples/        通用示例
tests/           冒烟测试
```

只有一个解决方案。公共编译选项集中在 `Project.Common.props`。

## 最小部署

```text
runLua.exe
star.dll
lua.dll
main.lua
```

`main.lua` 可以放在工作目录，也可以通过配置或命令行指定。`runLua` 会把自身目录加入 `package.cpath`，脚本位于其他目录时仍可加载同目录的 `star.dll`。

## Lua 来源

| 项目 | 值 |
|---|---|
| 官方源码 | `https://www.lua.org/ftp/lua-5.5.0.tar.gz` |
| SHA-256 | `57ccc32bbbd005cab75bcc52444052535af691789dba2b9016d5c50640d68b3d` |
| 本地源码 | `third_party/lua-5.5.0/src` |

仓库只保留构建动态库需要的源码和头文件。Lua 许可证见 `third_party/LICENSE.txt`。

Lua 5.5 把 `luaL_openlibs` 改为宏，因此 `lua/lua_compat.c` 提供同名兼容导出，保持 `runLua` 的动态绑定名称稳定。

## 升级 Lua

1. 从 Lua 官方站点下载新的稳定源码并核验校验值。
2. 替换 `third_party` 中的源码，同时更新工程路径和来源说明。
3. 检查 `runLua` 使用的导出是否仍存在。
4. 若某个必需函数变成宏，只在 `lua_compat.c` 增加最小兼容导出。
5. 重新构建 `lua.dll` 和所有 Lua C 模块。
6. 运行冒烟测试。`runLua.exe` 本身无需因 Lua 升级重新构建。

## 验证

```powershell
Push-Location .\bin\Release
.\runLua.exe ..\..\tests\smoke.lua
Pop-Location
```

可使用 Visual Studio 自带的工具确认依赖和导出：

```powershell
dumpbin /dependents .\bin\Release\runLua.exe
dumpbin /exports .\bin\Release\star.dll
```

`runLua.exe` 的依赖中不应出现 `lua.dll` 或 `star.dll`；`star.dll` 应导出 `luaopen_star` 和供启动器使用的最小 Lua C API。

同一链接单元必须使用一致的 CRT 选项。不要把 `/MD` 静态库混入本工程，也不要跨 DLL 分配和释放同一块内存。
