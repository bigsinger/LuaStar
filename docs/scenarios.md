# 发布场景与设计边界

本文只归纳通用流程，不记录业务名称、真实目录、产品文件名或私有发布清单。

## 当前覆盖

| 场景 | 接口 |
|---|---|
| 获取运行版本 | `version` |
| 输出调试信息 | `debug` |
| 定位和拆分路径 | `path`、`join`、`dir`、`name`、`ext` |
| 临时扩展工具搜索目录 | `env` |
| 执行构建、打包、签名和脚本 | `run` |
| 新建或增量更新 ZIP | `zip` |
| 准备、移动、创建和清理产物 | `copy`、`move`、`mkdir`、`remove` |
| 判断文件或目录 | `exists` |
| 等待人工检查 | `pause` |

普通流程示例：

```lua
local star = require "star"
local root = star.path()
local output = star.path.join(root, "output")

star.debug(true)
assert(star.remove(output))
assert(star.mkdir(output))

local ok, err = star.copy(root .. "input", output)
assert(ok, err)

local code, text = star.run(
    "tools\\pack.exe", "--input", output)
assert(code == 0, text)

code, text = star.zip(
    output,
    star.path.join(root, "release", "package.zip"))
assert(code == 0, text)
```

## 设计边界

- 外部工具统一由 `star.run` 调用，核心不维护工具专用参数和安装位置。
- `star.path` 只保留四个高频路径动作。
- `star.fs` 只保留五个高频文件动作。
- 文件动作同时提供顶层写法，避免简单脚本必须声明局部模块。
- 文件内容处理继续使用 Lua 的 `io`。
- 普通环境变量读取使用 `os.getenv`；`star.env` 只负责为当前流程追加 `PATH`。
- `star.zip` 只覆盖常用 ZIP 新建和更新；加密、分卷、过滤等复杂压缩继续交给专用工具。
- 哈希、网络、JSON、注册表和桌面自动化按需放入独立 DLL。
- 产品名称、版本规则、证书、账号、私有目录和业务清单留在私有脚本。

## 新接口准入

新能力进入 `star.dll` 前必须满足：

1. 多类发布流程都会重复使用。
2. Lua 标准库或 `run` 无法简洁完成。
3. 不引入大型第三方依赖。
4. 名称必须是一个简单单词。
5. 参数、返回值和失败行为容易说明。
