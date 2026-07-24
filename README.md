# LuaStar

LuaStar 是面向 Windows 自动化与发布流程的轻量 Lua 工具集。它只保留三个组件：

- `runLua.exe`：不包含 Lua 头文件，也不链接 Lua 库；运行时动态绑定配置指定 DLL 的 Lua C API。
- `lua.dll`：由官方 Lua 5.5.0 源码构建。
- `star.dll`：提供路径、文件、进程和常见发布工具封装，并转发 `runLua` 所需的最小 Lua C API。

没有配置文件时，`runLua.exe` 默认加载同目录的 `star.dll`，执行当前目录的 `main.lua`：

```powershell
.\runLua.exe
```

最小部署只需要：

```text
runLua.exe
star.dll
lua.dll
main.lua
```

## 快速示例

```lua
local star = require "star"
local p, fs = star.path, star.fs
local root = star.script_dir()
local output = p.join(root, "output")

fs.reset_dir(output)
fs.copy_file(p.join(root, "input", "app.exe"),
             p.join(output, "app.exe"))
print("发布目录：" .. output)
star.pause()
```

如需配置：

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

产物位于 `bin\Release`。详细构建和 Lua 升级方法见 [构建说明](docs/build.md)。

## 文档

- [runLua 使用说明](docs/runlua.md)
- [star.dll 完整接口](docs/api.md)
- [发布场景与后续候选接口](docs/scenarios.md)
- [构建、部署与升级](docs/build.md)

工程不收录业务项目名称、业务目录或真实发布清单。示例只使用通用占位名称，项目私有配置应留在各自的私有脚本中。
