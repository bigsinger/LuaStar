local star = require "star"
local root = star.path()
local outputDir = star.path.join(root, "output")

star.debug(true)

local ok, err = star.remove(outputDir)
assert(ok, err)
ok, err = star.mkdir(outputDir)
assert(ok, err)
ok, err = star.copy(root .. "input", outputDir)
assert(ok, err)

-- 参数按顺序用空格拼接；含空格的路径由脚本显式加引号。
local code, output = star.run(
    [["tools\pack.exe"]],
    "--input", [["output"]])

print(output)
assert(code == 0, "发布命令失败，退出码：" .. code)
