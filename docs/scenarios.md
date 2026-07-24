# 发布场景与接口分类

这里仅归纳通用流程，不记录任何业务名称、真实目录、产品文件名或私有发布清单。

## 当前已覆盖

| 场景 | 常见操作 | 推荐接口 |
|---|---|---|
| 定位流程文件 | 获取脚本目录、程序目录、工作目录 | `star.script_dir`、`exe_dir`、`cwd` |
| 路径处理 | 拼接、规范化、取父目录和文件名 | `star.path.*` |
| 准备输出目录 | 新建、清空、删除旧文件 | `fs.mkdir`、`reset_dir`、`remove` |
| 组装发布目录 | 单文件复制、目录复制、清单复制、移动 | `fs.copy_file`、`copy_tree`、`copy_files`、`move` |
| 清理与质量门 | 通配删除、枚举、计数、禁止文件检查 | `fs.remove_matching`、`list`、`count`、`assert_no_match` |
| 配置加工 | 二进制读写、普通文本替换、哈希校验 | `fs.read`、`write`、`replace_text`、`sha256` |
| 调用工具 | 直接执行、shell、批处理、超时和输出捕获 | `process.run`、`shell`、`batch` |
| 工具发现 | 显式路径、环境变量、候选目录、`PATH` | `process.which`、`release.find_tool` |
| 构建和打包 | 构建工程、压缩程序、生成安装包 | `release.msbuild`、`upx`、`nsis` |
| 人工确认 | 流程末尾等待查看结果 | `star.pause` |

通用流程示例见 [examples/publish.lua](../examples/publish.lua)。

## 建议保留在项目脚本中的内容

- 产品名称、版本规则和渠道名称。
- 私有目录、服务器、证书和账号。
- 业务文件清单及授权文件。
- 安装脚本中的产品元数据。
- 只出现一次的特殊转换逻辑。

`star.dll` 只封装稳定动作，不保存业务知识。

## 可选的下一批轻量接口

这些接口尚未实现，可按实际使用频率选择：

| 编号 | 场景 | 候选接口 | 说明 |
|---:|---|---|---|
| A1 | 目录镜像 | `fs.sync_tree(source, destination, options)` | 支持包含、排除、删除多余文件和预览 |
| A2 | 发布清单 | `fs.manifest(root, options)`、`fs.verify_manifest(file)` | 保存相对路径、大小和哈希并校验 |
| A3 | 简单模板 | `fs.render(source, destination, values)` | 只替换 `${name}`，避免引入模板引擎 |
| A4 | PE 信息 | `star.pe_version(path)` | 读取 Windows 文件版本 |
| B1 | 代码签名 | `release.sign(files, options)`、`release.verify_signature(file)` | 调用系统签名工具并检查结果 |
| B2 | 版本控制信息 | `release.git_info(root)`、`release.require_clean(root)` | 读取提交号并阻止脏目录发布 |
| B3 | VS 自动发现 | `release.find_msbuild(options)` | 通过官方发现工具定位 MSBuild |
| C1 | 压缩包 | 独立 `stararchive.dll` | ZIP、7z、tar 解压缩不进入核心 |
| C2 | 网络下载 | 独立 `starnet.dll` | 需要完整处理 TLS、代理、重试和校验 |
| C3 | JSON | 独立小模块或成熟纯 Lua 库 | 不把格式解析器堆入核心 |

建议优先考虑 `A1`、`A2`、`A4`、`B1`、`B3`。复杂数据处理、网页自动化、大型依赖管理等任务继续使用更适合的工具。

## 收录原则

新接口进入 `star.dll` 前应满足：

1. 在多个发布流程中重复出现。
2. 能明显减少路径、参数引用、代码页或退出码错误。
3. 不引入大型第三方运行时。
4. 参数和返回值可以在一张小表内说明。
5. 失败行为明确，并能直接中止错误发布。
