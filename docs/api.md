# star.dll 完整接口

## 约定

```lua
require "star" -- 也会将模块放入全局 star
local star = require "star" -- 推荐在脚本中使用局部变量
```

接口名全部是简单单词。Lua 字符串按 UTF-8 解释，Windows 路径在 DLL 内转换为 UTF-16。不提供工具专用包装或旧名字兼容层。

## `star.version()`

```lua
local starVersion, luaVersion = star.version()
```

| 项目 | 说明 |
|---|---|
| 参数 | 无 |
| 第一个返回值 | Star 版本字符串 |
| 第二个返回值 | 编译时 Lua 完整版本字符串 |

只接收一个返回值时，得到的是 Star 版本。

## `star.debug(enabled?)`

```lua
star.debug(true)
local enabled = star.debug()
star.debug(false)
```

| 项目 | 说明 |
|---|---|
| `enabled` | 可选布尔值；`true` 开启，`false` 关闭 |
| 返回值 | 当前调试状态 |
| 默认值 | `false` |

开启后，`run` 会打印完整拼接命令、退出码和可读取的输出；环境和文件操作会打印目标及结果。日志以 `[star]` 开头，状态对当前进程全局生效。

## `star.path`

`star.path` 是可调用的小工具表。

直接调用返回当前脚本位置：

```lua
local root, file = star.path()
print(root .. file)
```

| 返回值 | 说明 |
|---|---|
| 第一个 | 脚本目录，末尾始终带 `\` |
| 第二个 | 文件名，不含目录，保留扩展名 |

无法确定脚本文件时，目录返回当前工作目录并带斜杠，文件名返回空字符串。

只保留四个路径方法：

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `star.path.join(...)` | 拼接并规范化路径 | 一个或多个路径片段 | 路径字符串 |
| `star.path.dir(path)` | 获取父目录 | 路径 | 末尾带斜杠的目录 |
| `star.path.name(path)` | 获取文件名 | 路径 | 含扩展名的文件名 |
| `star.path.ext(path)` | 获取扩展名 | 路径 | 含前导点的扩展名；没有则为空 |

## `star.run(...)`

```lua
local code, output = star.run(
    '"tools\\pack.exe"', "--mode", "release")
```

| 项目 | 说明 |
|---|---|
| 参数 | 任意个可转为字符串的 Lua 值 |
| 拼接规则 | 每项转为字符串，再用一个半角空格依次连接 |
| 执行方式 | 通过系统命令解释器执行 |
| 第一个返回值 | 退出码；启动失败或空命令为 `-1` |
| 第二个返回值 | 标准输出和标准错误合并后的文本 |

接口不自动给参数加引号。含空格的路径由脚本明确加双引号：

```lua
local exe = [["C:\Program Files\Tool\tool.exe"]]
local code, output = star.run(exe, "--input", [["input files"]])
assert(code == 0, output)
```

非零退出码不会抛错，调用者应自行判断。

开启 `star.debug(true)` 后，非零退出码及其命令输出会在交互式控制台以红色高亮显示，便于快速发现执行失败。

## `star.env(directory)`

把目录追加到当前进程的 `PATH`：

```lua
local ok, err = star.env(root .. "tools")
assert(ok, err)
```

| 项目 | 说明 |
|---|---|
| `directory` | 已存在的目录；相对路径会转成绝对路径 |
| 第一个返回值 | 成功为 `true`，失败为 `false` |
| 第二个返回值 | 成功为 `nil`，失败为错误文本 |

匹配时忽略大小写和末尾斜杠，已存在的目录不会重复追加。修改只影响当前进程以及之后由它启动的子进程，不修改系统或用户的永久环境变量。读取普通环境变量继续使用 Lua 自带的 `os.getenv`。

## 文件接口

文件接口既能直接调用，也能从 `star.fs` 调用：

```lua
star.copy(source, destination)
star.fs.copy(source, destination)
```

两种写法行为一致。

### `copy`

`star.copy(source, destination)` 或 `star.fs.copy(...)`

复制普通文件或目录。文件复制会创建父目录并覆盖同名文件；目录复制会合并到目标并覆盖同名普通文件，但不删除目标中的多余内容。目标目录不能等于或位于源目录内部。

返回 `true, nil`；失败返回 `false, error`。

### `move`

`star.move(source, destination)` 或 `star.fs.move(...)`

移动普通文件或目录并创建目标父目录。目标已存在时失败。同卷优先直接改名；失败后尝试复制并删除源路径，以支持常见跨卷场景。

返回 `true, nil`；失败返回 `false, error`。

### `mkdir`

`star.mkdir(path1, path2, ...)` 或 `star.fs.mkdir(path1, path2, ...)`

按参数顺序递归创建多个目录。每个目录创建前都会检查是否已经存在；已存在的目录直接跳过，同名文件会导致失败。

返回 `true, nil`；失败返回 `false, error`。

### `remove`

`star.remove(path)` 或 `star.fs.remove(path)`

删除普通文件、符号链接或整个目录树。路径不存在也视为成功。接口拒绝空路径和文件系统根目录。

返回 `true, nil`；失败返回 `false, error`。

### `exists`

```lua
local found, kind = star.exists(path)
local found, kind = star.fs.exists(path)
```

| 返回值 | 说明 |
|---|---|
| `found` | 存在为 `true`，不存在为 `false` |
| `kind` | `"file"`、`"dir"` 或 `"other"`；不存在时为 `nil` |

符号链接等非普通对象标识为 `"other"`。

## `star.pause(message?)`

```lua
star.pause()
star.pause("检查完成，按任意键退出...")
```

| 项目 | 说明 |
|---|---|
| `message` | 可选 UTF-8 文本 |
| 默认提示 | `按任意键继续 . . .` |
| 返回值 | 无 |

该接口直接读写控制台，不调用外部 `pause` 命令。

## 错误模型

- 参数类型或 UTF-8 编码错误会抛出 Lua 错误。
- `env/copy/move/mkdir/remove` 通过 `bool, error` 报告操作结果。
- `exists` 返回存在状态和对象类型。
- `run` 返回退出码和合并输出。

接口保持少量、直观，不把复杂发布框架放进 DLL。
