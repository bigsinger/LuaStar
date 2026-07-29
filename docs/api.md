# star.dll 完整接口

本文说明 `star.dll` 的全部公开 Lua 接口。示例只使用通用名称和相对路径，可直接改成自己的流程目录。

## 加载模块

以下两种写法都会加载同一个模块：

```lua
require "star"
star.debug(true)
```

```lua
local star = require "star"
star.debug(true)
```

第一种写法会得到全局变量 `star`；第二种写法同时得到局部变量，通常更适合较长脚本。重复 `require` 会使用 Lua 的模块缓存。

## 公共约定

### 接口清单

| 接口 | 作用 | 成功返回 |
|---|---|---|
| `version` | 获取 Star 和 Lua 版本 | `starVersion, luaVersion` |
| `debug` | 查询或切换调试模式 | `enabled` |
| `path` | 获取当前脚本位置 | `directory, filename` |
| `path.join` | 拼接路径 | `path` |
| `path.dir` | 获取父目录 | `directory` |
| `path.name` | 获取文件名 | `filename` |
| `path.ext` | 获取扩展名 | `extension` |
| `run` | 执行命令 | `code, text` |
| `zip` | 新建或更新 ZIP | `code, text` |
| `env` | 向当前进程的 `PATH` 追加目录 | `true, nil` |
| `copy` | 复制文件或目录 | `true, nil` |
| `move` | 移动文件或目录 | `true, nil` |
| `mkdir` | 创建一个或多个目录 | `true, nil` |
| `remove` | 删除文件或目录 | `true, nil` |
| `exists` | 判断路径是否存在及其类型 | `found, kind` |
| `pause` | 等待按键 | 无 |

`star.fs` 只包含 `copy/move/mkdir/remove/exists`，行为与同名顶层接口完全一致。

### 字符串和路径

- Lua 传给 DLL 的文本按 UTF-8 解释，DLL 内部使用 Windows UTF-16 文件 API。
- `star` 的路径接口支持中文；Lua 标准库 `io` 是否能直接打开中文文件名仍受当前 Windows CRT 和系统区域设置影响。
- 除 `star.path()` 返回脚本位置外，所有相对路径都以当前进程的工作目录为基准，不以脚本目录为基准。
- 需要稳定地从脚本目录定位文件时，先调用 `star.path()`，再使用 `star.path.join(...)`。
- 路径参数按字面值处理；`star.zip` 不展开 `*`、`?` 等通配符。

```lua
local star = require "star"
local scriptDir = star.path()
local input = star.path.join(scriptDir, "input")
```

### 两类失败返回

文件操作采用：

```lua
local ok, err = star.copy("input", "output")
assert(ok, err)
```

- 成功：`true, nil`
- 失败：`false, error`

外部进程操作采用：

```lua
local code, text = star.run("tool.exe", "--help")
assert(code == 0, text)
```

- 成功通常为 `0, text`
- 失败为非零退出码和合并后的标准输出/标准错误
- 无法启动命令或命令为空时退出码为 `-1`

参数数量、参数类型、无效 UTF-8，以及可在执行前确定的危险路径会抛出 Lua 错误，可用 `pcall` 捕获：

```lua
local called, message = pcall(star.zip, "only-one-argument")
assert(not called)
print(message)
```

## `star.version()`

一次返回 Star 和编译时 Lua 的版本。

```lua
local starVersion, luaVersion = star.version()
print(starVersion)
print(luaVersion)
```

| 项目 | 说明 |
|---|---|
| 参数 | 无 |
| 第一个返回值 | Star 版本字符串，例如 `"1.1.0"` |
| 第二个返回值 | Lua 完整版本字符串，例如 `"Lua 5.5.0"` |

只接收一个返回值时得到 Star 版本：

```lua
local version = star.version()
```

## `star.debug(enabled?)`

查询或切换当前进程内的调试模式。

```lua
local enabled = star.debug() -- 只查询
star.debug(true)             -- 开启
star.debug(false)            -- 关闭
```

| 项目 | 说明 |
|---|---|
| `enabled` | 可选布尔值；省略或传 `nil` 时只查询 |
| 返回值 | 设置后的当前状态 |
| 默认状态 | `false` |
| 作用范围 | 当前进程内所有 `star` 调用 |

开启后会输出 `[star]` 前缀的执行信息。`run` 和 `zip` 的退出码为 `0` 时绿色显示，非零退出码和错误输出红色显示。输出被重定向到文件或管道时不附加控制台颜色。

常见用法：

```lua
local debug = os.getenv("DEBUG") == "1"
star.debug(debug)
```

## `star.path()`

`star.path` 是一个可直接调用的小工具表。直接调用时返回当前 Lua 脚本的位置：

```lua
local directory, filename = star.path()
print(directory .. filename)
```

| 返回值 | 说明 |
|---|---|
| `directory` | 脚本所在目录，末尾始终带 `\` 或 `/` |
| `filename` | 不含目录、保留扩展名的脚本文件名 |

如果无法从 Lua 调用栈确定脚本文件，目录回退为当前工作目录并保留末尾斜杠，文件名返回空字符串。

只需要目录时：

```lua
local root = star.path()
```

### `star.path.join(...)`

按顺序拼接一个或多个路径片段，并进行词法规范化。

```lua
local file = star.path.join("output", "x64", "app.exe")
local other = star.path.join("output\\x64", "..", "symbols")
```

| 项目 | 说明 |
|---|---|
| 参数 | 至少一个路径字符串 |
| 返回值 | 拼接后的路径 |

注意：

- 该接口不检查路径是否存在。
- 后续片段如果是 Windows 绝对路径，遵循 `std::filesystem::path` 的拼接规则。
- `..` 会进行词法折叠，但不会解析符号链接。

### `star.path.dir(path)`

返回路径的父目录，非空结果末尾带斜杠。

```lua
assert(star.path.dir([[output\x64\app.exe]]) ==
       [[output\x64\]])
```

| 项目 | 说明 |
|---|---|
| `path` | 文件或目录形式的路径字符串；无需真实存在 |
| 返回值 | 父目录；没有父目录时为空字符串 |

### `star.path.name(path)`

返回最后一个路径组成部分。

```lua
assert(star.path.name([[output\x64\app.exe]]) == "app.exe")
```

目录路径末尾是否带斜杠会影响 `std::filesystem` 的结果，建议先去掉多余的末尾斜杠。

### `star.path.ext(path)`

返回最后一个路径组成部分的扩展名，包含前导点。

```lua
assert(star.path.ext("archive.zip") == ".zip")
assert(star.path.ext("README") == "")
```

该接口不检查文件是否存在。

## `star.run(...)`

通过 Windows 命令解释器执行命令，自动收集标准输出和标准错误。

```lua
local code, text = star.run(
    "tools\\pack.exe",
    "--input", "input files",
    "--output", "output\\package.bin")
assert(code == 0, text)
```

| 项目 | 说明 |
|---|---|
| 参数 | 任意个可由 Lua 转成字符串的值 |
| 拼接规则 | 参数依次转成字符串，必要时自动加引号，再用一个半角空格连接 |
| 执行方式 | `cmd.exe /D /S /C` |
| 第一个返回值 | 子进程退出码；启动失败或空命令为 `-1` |
| 第二个返回值 | 标准输出和标准错误合并后的原始文本 |

### 路径、数字和空参数

```lua
local code, text = star.run(
    [[C:\Program Files\Tool\tool.exe]],
    "--count", 3,
    "--label", "release package",
    "--empty", "")
```

脚本无需自己实现 `quote`。不要在普通路径参数外再包一层引号，否则引号可能成为参数内容的一部分。

### 执行命令表达式

第一个参数可以是完整的命令表达式：

```lua
local code, text = star.run(
    "echo first && echo second")
```

也可以把常见命令操作符作为独立参数：

```lua
local code, text = star.run(
    "tool.exe", "--check", "&", "exit", "/B", 7)
```

`run` 使用命令解释器，因此 `& | < >` 等符号具有命令语义。不要把未经信任的外部文本直接作为参数传入。

### 判断结果

`run` 不会因为非零退出码自动抛出 Lua 错误：

```lua
local code, text = star.run("tool.exe", "--build")
if code ~= 0 then
    error(("构建失败（%d）：\n%s"):format(code, text))
end
```

`text` 的具体编码由子进程决定；通用流程工具建议输出 UTF-8。

## `star.zip(input1, ..., outputZip)`

使用 Windows PowerShell 的 `Compress-Archive` 新建或更新 ZIP，不在 `star.dll` 内集成压缩库。

```lua
local code, text = star.zip(
    "app.exe",
    "README.md",
    "assets",
    "output\\package.zip")
assert(code == 0, text)
```

| 项目 | 说明 |
|---|---|
| 输入参数 | 一个或多个已存在的普通文件或目录 |
| 最后一个参数 | 输出 ZIP 文件路径，必须以 `.zip` 结尾 |
| 路径形式 | 输入和输出均可使用绝对路径或相对路径 |
| 第一个返回值 | PowerShell 退出码 |
| 第二个返回值 | PowerShell 的标准输出和标准错误合并文本 |

输出目录不存在时会自动递归创建。

### ZIP 内的组织结构

每个输入项都以自身名称作为 ZIP 根目录下的入口：

| 输入 | ZIP 内结果 |
|---|---|
| `app.exe` | `app.exe` |
| `docs\README.md` | `README.md` |
| `assets` | `assets\...` |
| `C:\work\locale` | `locale\...` |

目录名称和目录内部的相对层级都会保留。接口不会像 `assets\*` 那样丢弃 `assets` 这一层。

不同输入项的顶层名称必须唯一。例如，同时输入 `one\config.ini` 和 `two\config.ini` 会抛出 Lua 错误；需要保留两份时应输入 `one`、`two` 两个目录。

### 新建 ZIP

目标 ZIP 不存在时直接创建：

```lua
local code, text = star.zip(
    "bin",
    "config",
    "release\\package.zip")
assert(code == 0, text)
```

### 更新已有 ZIP

目标 ZIP 已存在时自动使用更新模式：

```lua
local code, text = star.zip(
    "bin\\app.exe",
    "config",
    "release\\package.zip")
assert(code == 0, text)
```

更新规则：

- ZIP 内同路径的文件会用当前输入文件覆盖，不依赖文件时间戳新旧。
- ZIP 内不存在的文件和目录会追加。
- 本次没有输入的旧条目继续保留。
- 输入目录中已经删除的文件不会自动从 ZIP 中删除。

需要完全重新生成压缩包时，先删除旧 ZIP：

```lua
local archive = "release\\package.zip"
local ok, err = star.remove(archive)
assert(ok, err)

local code, text = star.zip("bin", "config", archive)
assert(code == 0, text)
```

### 混合绝对路径和相对路径

```lua
local root = star.path()
local code, text = star.zip(
    star.path.join(root, "README.md"),
    "output",
    star.path.join(root, "release", "package.zip"))
assert(code == 0, text)
```

相对路径以启动 `runLua` 时的当前工作目录为基准。

### 校验和限制

以下情况会在启动 PowerShell 前抛出 Lua 错误：

- 少于一个输入项；
- 输入不存在、为空、重复，或不是普通文件/目录；
- 多个输入会产生相同的 ZIP 顶层名称；
- 输出不是 `.zip` 文件；
- 输出同时也是输入文件；
- 输出 ZIP 位于某个输入目录内部；
- 把文件系统根目录作为输入。

PowerShell 不存在、ZIP 已损坏、文件被占用或权限不足等运行期错误通过 `code, text` 返回。

`zip` 不提供密码、加密、分卷、排除规则或压缩等级参数；这些复杂场景应调用专用压缩工具。输入使用字面路径，不支持通配符。

## `star.env(directory)`

把目录追加到当前进程的 `PATH`，便于后续 `star.run` 直接调用其中的工具。

```lua
local root = star.path()
local ok, err = star.env(star.path.join(root, "tools"))
assert(ok, err)

local code, text = star.run("tool.exe", "--version")
assert(code == 0, text)
```

| 项目 | 说明 |
|---|---|
| `directory` | 已存在的目录；相对路径会转成绝对路径 |
| 成功返回 | `true, nil` |
| 失败返回 | `false, error` |

行为说明：

- 匹配时忽略大小写和末尾斜杠。
- 同一目录已在 `PATH` 中时不会重复追加。
- 修改只影响当前进程和之后启动的子进程。
- 不修改用户或系统的永久环境变量。
- 读取普通环境变量继续使用 `os.getenv("NAME")`。

## 文件接口

文件接口可直接调用，也可从 `star.fs` 调用：

```lua
star.copy("input", "output")
star.fs.copy("input", "output")
```

两种形式是同一实现。所有相对路径都以当前工作目录为基准。

## `star.copy(source, destination)`

复制普通文件或整个目录。

```lua
local ok, err = star.copy(
    "build\\app.exe",
    "release\\app.exe")
assert(ok, err)
```

文件复制规则：

- 自动创建目标文件的父目录。
- 目标同名文件存在时覆盖。

目录复制规则：

- 把源目录内容复制到明确指定的目标目录。
- 自动创建目标目录及子目录。
- 与已有目标目录合并，同名普通文件覆盖。
- 不删除目标目录中多出的旧文件。
- 目标目录不能等于源目录，也不能位于源目录内部。
- 不支持复制符号链接或其他特殊文件。

```lua
local ok, err = star.copy("assets", "release\\assets")
assert(ok, err)
```

成功返回 `true, nil`；失败返回 `false, error`。

## `star.move(source, destination)`

移动普通文件或目录。

```lua
local ok, err = star.move(
    "output\\package.tmp",
    "release\\package.zip")
assert(ok, err)
```

行为说明：

- 自动创建目标父目录。
- 目标路径已存在时失败，不覆盖。
- 同卷优先直接改名。
- 直接改名失败后尝试复制并删除源路径，可覆盖常见跨卷移动场景。
- 目录目标不能位于源目录内部。
- 不支持移动符号链接。

成功返回 `true, nil`；失败返回 `false, error`。跨卷回退中若复制成功但删除源路径失败，错误文本会明确说明，此时目标可能已经存在。

## `star.mkdir(path1, path2, ...)`

按参数顺序递归创建一个或多个目录。

```lua
local ok, err = star.mkdir(
    "output",
    "output\\cache",
    "release\\x64")
assert(ok, err)
```

行为说明：

- 至少需要一个参数。
- 每个目录创建前先检查是否存在。
- 已存在的目录直接跳过。
- 同名普通文件存在时失败。
- 某个参数失败后停止，之前已经创建的目录不会回滚。

成功返回 `true, nil`；失败返回 `false, error`。

## `star.remove(path)`

删除普通文件、符号链接或整个目录树。

```lua
local ok, err = star.remove("output")
assert(ok, err)
```

行为说明：

- 路径不存在也视为成功，适合发布流程开始前清理旧目录。
- 目录会连同全部子项递归删除。
- 拒绝空路径。
- 拒绝文件系统根目录，避免误删整个磁盘。

成功返回 `true, nil`；失败返回 `false, error`。该操作不可恢复，路径应由脚本明确构造。

## `star.exists(path)`

判断路径是否存在，并标识对象类型。

```lua
local found, kind = star.exists("release\\package.zip")
if found and kind == "file" then
    print("压缩包已生成")
end
```

| 返回值 | 说明 |
|---|---|
| `found` | 存在为 `true`，不存在为 `false` |
| `kind` | `"file"`、`"dir"` 或 `"other"`；不存在时为 `nil` |

符号链接、设备等非普通对象返回 `"other"`。

按类型分支：

```lua
local found, kind = star.exists("output")
if not found then
    assert(star.mkdir("output"))
elseif kind ~= "dir" then
    error("output 已存在，但不是目录")
end
```

## `star.pause(message?)`

直接从控制台读取一个按键。

```lua
star.pause()
star.pause("请检查输出，按任意键继续...")
```

| 项目 | 说明 |
|---|---|
| `message` | 可选 UTF-8 提示文本 |
| 默认提示 | `按任意键继续 . . .` |
| 返回值 | 无 |

该接口不调用外部 `pause` 命令。不要在无人值守构建或持续集成脚本的正常路径中调用，否则会等待人工输入。

`runLua` 在交互式控制台遇到 Lua 编译错误或运行时错误时会自动暂停；重定向和无人值守环境不会因此阻塞。

## 完整流程示例

```lua
local star = require "star"
local root, script = star.path()
local stage = star.path.join(root, "stage")
local archive = star.path.join(root, "release", "package.zip")

star.debug(true)
print("执行脚本：" .. script)

local ok, err = star.remove(stage)
assert(ok, err)
ok, err = star.mkdir(stage)
assert(ok, err)

ok, err = star.copy(
    star.path.join(root, "input"),
    stage)
assert(ok, err)

local code, text = star.run(
    "tool.exe",
    "--input", stage,
    "--mode", "release")
assert(code == 0, text)

code, text = star.zip(stage, archive)
assert(code == 0, text)

print("已生成：" .. archive)
```

## 设计边界

`star.dll` 只保留高频、轻量、易记忆的单词接口。文件内容处理使用 Lua `io`，环境变量读取使用 `os.getenv`，复杂压缩、网络、JSON、注册表和桌面自动化通过 `star.run` 或独立扩展模块完成。
