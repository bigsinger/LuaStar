# LuaStar

LuaStar 是面向 Windows 自动化和发布流程的轻量 Lua 工具集，只包含三个组件：

- `runLua.exe`：不包含 Lua 头文件、不链接 Lua 库，运行时动态绑定配置指定 DLL 的 Lua C API。
- `lua.dll`：由官方 Lua 5.5.0 源码构建。
- `star.dll`：提供少量直接接口和两个小工具表，并转发 `runLua` 所需的最小 Lua C API。

常用接口全部使用单词命名：

```text
version  debug  path  run  env
copy  move  mkdir  remove  exists  pause
fs
```

`star.path` 既可以直接调用，也只保留 `join/dir/name/ext` 四个方法。`star.fs` 只包含 `copy/move/mkdir/remove/exists`，这些文件接口同时可以直接从 `star` 调用。

没有配置文件时，`runLua.exe` 默认加载同目录的 `star.dll`，执行当前目录的 `main.lua`：

```powershell
.\runLua.exe
```

最小部署：

```text
runLua.exe
star.dll
lua.dll
main.lua
```

## 快速示例

```lua
local star = require "star"

local root, file = star.path()
local starVersion, luaVersion = star.version()

star.debug(true)
print(("Star %s / %s"):format(starVersion, luaVersion))
print("脚本：" .. root .. file)

assert(star.remove(root .. "output"))
assert(star.mkdir(root .. "output"))

local ok, err = star.copy(root .. "input", root .. "output")
assert(ok, err)

local code, output = star.run("echo", "发布完成")
print(output)
assert(code == 0)
```

扁平配置：

```ini
LuaDll=star.dll
LuaFile=main.lua
```

## 构建要求

- Visual Studio 2026，Platform Toolset `v145`
- x64、Unicode
- C/C++ 最新标准
- Debug `/MTd`，Release `/MT`

在 Visual Studio Developer PowerShell 中执行：

```powershell
msbuild .\LuaStar.sln /m /t:Build /p:Configuration=Release /p:Platform=x64
```

## 文档

- [runLua 使用说明](docs/runlua.md)
- [star.dll 完整接口](docs/api.md)
- [发布场景与设计边界](docs/scenarios.md)
- [构建、部署与升级](docs/build.md)

公开仓库不记录业务名称、私有目录、真实文件清单或工具安装路径。
