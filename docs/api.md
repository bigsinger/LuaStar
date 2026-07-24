# star.dll 完整接口

## 通用约定

```lua
local star = require "star"
local path = star.path
local fs = star.fs
local process = star.process
local release = star.release
```

Lua 字符串统一按 UTF-8 解释，Windows 路径在 DLL 内转换为 UTF-16。除明确说明外，修改文件或执行进程失败时会抛出 Lua 错误并终止当前脚本。

## 顶层接口

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `star.version()` | 获取 Star 版本 | 无 | 版本字符串 |
| `star.lua_version()` | 获取编译时 Lua 版本 | 无 | Lua 版本字符串 |
| `star.exe_path()` | 获取当前启动程序路径 | 无 | 绝对路径 |
| `star.exe_dir()` | 获取启动程序目录 | 无 | 绝对路径 |
| `star.script_path()` | 获取当前 Lua 文件路径 | 无 | 绝对路径；无法确定时为 `nil` |
| `star.script_dir()` | 获取当前 Lua 文件目录 | 无 | 绝对路径；无法确定时为当前目录 |
| `star.cwd()` | 获取当前工作目录 | 无 | 绝对路径 |
| `star.cwd(path)` | 修改当前工作目录 | `path`：现有目录 | 修改后的绝对路径 |
| `star.pause(message?)` | 输出提示并等待任意键 | `message`：可选 UTF-8 文本，默认“按任意键继续 . . .” | 无 |
| `star.require_file(path, label?)` | 检查必需文件 | 文件路径；可选中文说明 | 文件绝对路径；缺失时抛错 |
| `star.require_dir(path, label?)` | 检查必需目录 | 目录路径；可选中文说明 | 目录绝对路径；缺失时抛错 |
| `star.help()` | 获取简短帮助 | 无 | 字符串 |

发布脚本通常以 `star.script_dir()` 作为根目录，避免依赖调用者的当前目录。

## `star.path`

路径接口只处理路径文本，不要求目标存在。

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `path.join(part1, part2, ...)` | 拼接并规范化路径 | 一个或多个路径片段 | 路径字符串 |
| `path.normalize(value)` | 消除可处理的 `.`、`..` 和重复分隔符 | 路径 | 路径字符串 |
| `path.absolute(value, base?)` | 生成绝对路径 | 路径；可选基准目录，默认当前目录 | 绝对路径 |
| `path.relative(value, base?)` | 生成相对路径 | 目标；可选基准目录 | 相对路径 |
| `path.parent(value)` | 获取父目录 | 路径 | 路径字符串 |
| `path.filename(value)` | 获取文件名和扩展名 | 路径 | 字符串 |
| `path.stem(value)` | 获取不含最后一个扩展名的文件名 | 路径 | 字符串 |
| `path.extension(value)` | 获取扩展名 | 路径 | 含前导点的字符串；没有则为空 |
| `path.is_absolute(value)` | 判断是否绝对路径 | 路径 | 布尔值 |
| `path.separator` | Windows 首选分隔符 | 无；常量 | `"\\"` |

## `star.fs`

### 查询和文件内容

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `fs.exists(path)` | 判断路径是否存在 | 路径 | 布尔值 |
| `fs.is_file(path)` | 判断是否普通文件 | 路径 | 布尔值 |
| `fs.is_dir(path)` | 判断是否目录 | 路径 | 布尔值 |
| `fs.size(path)` | 获取文件大小 | 文件路径 | 字节数 |
| `fs.read(path)` | 二进制读取全部内容 | 文件路径 | Lua 字符串，可含 `\0` |
| `fs.write(path, data)` | 二进制覆盖写入，并创建父目录 | 文件路径；数据 | `true` |
| `fs.replace_text(path, search, replacement, required?)` | 普通字节串全量替换 | 文件；查找；替换；`required` 默认 `true` | 替换次数 |
| `fs.sha256(path)` | 计算 SHA-256 | 文件路径 | 64 位小写十六进制字符串 |

`read`、`write`、`replace_text` 不自动转码，建议文本文件统一使用 UTF-8。

### 目录和复制

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `fs.mkdir(path)` | 递归创建目录 | 路径 | `true` |
| `fs.reset_dir(path)` | 删除已有目录并重建空目录 | 路径 | `true` |
| `fs.remove(path, recursive?, missing_ok?)` | 删除文件或目录 | `recursive` 默认 `true`；`missing_ok` 默认 `true` | 删除数量 |
| `fs.copy_file(source, destination, overwrite?)` | 复制单个文件并创建父目录 | 源；目标；覆盖默认 `true` | `true` |
| `fs.copy_tree(source, destination, overwrite?)` | 递归复制目录树 | 源目录；目标目录；覆盖默认 `true` | `true` |
| `fs.copy_files(source_dir, destination_dir, names, reset?)` | 按相对路径清单复制 | 源目录；目标目录；字符串数组；重建默认 `false` | 复制数量 |
| `fs.move(source, destination, overwrite?)` | 移动或改名，支持跨卷 | 源；目标；覆盖默认 `true` | `true` |
| `fs.remove_matching(root, patterns, recursive?)` | 删除匹配的文件 | 根目录；模式或模式数组；递归默认 `false` | 删除数量 |

`reset_dir`、递归 `remove` 和 `remove_matching` 拒绝空路径和卷根目录。调用者仍应传入明确的发布目录。

### 枚举和校验

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `fs.list(root, options?)` | 枚举并排序普通文件 | 根目录；选项表 | 路径数组 |
| `fs.count(root, patterns, recursive?)` | 统计匹配文件 | 根目录；模式或数组；递归默认 `false` | 整数 |
| `fs.assert_no_match(root, patterns, recursive?)` | 确认不存在禁用文件 | 根目录；模式或数组；递归默认 `true` | `true`；发现文件时抛错 |

`fs.list` 选项：

| 字段 | 类型 | 默认值 | 作用 |
|---|---|---:|---|
| `pattern` | 字符串 | `"*"` | 单个 Windows 通配模式 |
| `patterns` | 字符串数组 | 无 | 多个模式，优先于 `pattern` |
| `recursive` | 布尔值 | `false` | 是否递归 |
| `relative` | 布尔值 | `false` | 是否返回相对根目录的路径 |

模式只匹配文件名，例如 `"*.dll"` 或 `{"*.pdb", "*.ilk"}`。

## `star.process`

### 通用选项

| 字段 | 类型 | 默认值 | 作用 |
|---|---|---:|---|
| `cwd` | 路径 | 当前目录 | 子进程工作目录 |
| `timeout_ms` | 整数 | 无限等待 | 超时时间，毫秒 |
| `capture` | 布尔值 | `false` | 合并捕获标准输出和错误输出 |
| `check` | 布尔值 | `true` | 非零退出码或超时时是否抛错 |
| `hide` | 布尔值 | `false` | 是否隐藏子进程窗口 |

### 结果表

| 字段 | 类型 | 作用 |
|---|---|---|
| `ok` | 布尔值 | 未超时且退出码为 0 |
| `exit_code` | 整数 | 进程退出码；超时终止为 `124` |
| `timed_out` | 布尔值 | 是否超时 |
| `output` | 字符串 | 捕获的输出；未捕获时为空 |
| `command` | 字符串 | 实际命令行，便于诊断 |

### 接口

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `process.run(program, args?, options?)` | 不经过 shell 执行程序，自动引用参数 | 程序；字符串数组；通用选项 | 结果表 |
| `process.shell(command, options?)` | 通过系统命令解释器执行 | 命令文本；通用选项 | 结果表 |
| `process.batch(content, options?)` | 生成临时批处理并执行 | 批处理文本；通用选项 | `ok, exit_code, output` |
| `process.which(name, candidates?)` | 从候选路径和 `PATH` 查找程序 | 文件名；可选文件或目录数组 | 绝对路径或 `nil` |
| `process.quote(argument)` | 按 Windows C 参数规则引用文本 | 参数字符串 | 引用后的字符串 |

直接执行 EXE 优先使用 `process.run`。管道、重定向、批处理文件和 shell 内建命令使用 `process.shell` 或 `process.batch`。

## `star.release`

这一层只编排外部工具参数，不把外部工具集成进 DLL。工具查找顺序为：`exe`、环境变量、候选路径、系统 `PATH`。

### 通用工具选项

| 字段 | 类型 | 作用 |
|---|---|---|
| `exe` | 路径 | 显式工具文件，优先级最高 |
| `env` | 字符串或数组 | 追加要检查的环境变量 |
| `paths` | 路径数组 | 追加候选文件或目录 |
| `required` | 布尔值 | `find_tool` 找不到时是否抛错，默认 `true` |

所有发布接口也接受 `process` 的通用选项。

| 名称 | 作用 | 参数 | 返回值 |
|---|---|---|---|
| `release.find_tool(name, options?)` | 查找任意工具 | 文件名；工具选项 | 绝对路径；允许缺失时为 `nil` |
| `release.upx(source, destination, options?)` | 压缩文件并创建输出目录 | 源；目标；选项 | 进程结果表 |
| `release.nsis(script, options?)` | 根据安装脚本生成安装包 | 脚本；选项 | 进程结果表 |
| `release.msbuild(project, options?)` | 构建解决方案或工程 | 工程路径；选项 | 进程结果表 |

`release.upx` 选项：

| 字段 | 默认值 | 作用 |
|---|---:|---|
| `exe` | `UPX_EXE` 后查 `PATH` | 工具路径 |
| `arguments` | `{"-9kf"}` | 工具附加参数 |
| `force` | `true` | 执行前删除已有目标 |

`release.nsis` 选项：

| 字段 | 默认值 | 作用 |
|---|---:|---|
| `exe` | `MAKENSIS_EXE`、`NSIS` 后查 `PATH` | 工具路径 |
| `warnings_as_errors` | `false` | 是否添加 `/WX` |
| `charset` | 无 | 输入字符集 |
| `defines` | 无 | 定义键值表 |
| `arguments` | 无 | 其他参数数组 |
| `cwd` | 脚本目录 | 工作目录 |

`release.msbuild` 选项：

| 字段 | 默认值 | 作用 |
|---|---:|---|
| `exe` | `MSBUILD_EXE` 后查 `PATH` | 工具路径 |
| `restore` | `false` | 是否先还原 |
| `max_cpu` | `true` | 是否启用并行构建 |
| `target` | 无 | 构建目标 |
| `configuration` | 无 | 构建配置 |
| `platform` | 无 | 目标平台 |
| `properties` | 无 | MSBuild 属性键值表 |
| `arguments` | 无 | 其他参数数组 |
| `cwd` | 工程目录 | 工作目录 |
