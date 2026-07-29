local star = require "star"
local root = star.path()
local outputDir = star.path.join(root, "output")

star.debug(true)

local ok, err = star.remove(outputDir)
assert(ok, err)
ok, err = star.mkdir(outputDir, star.path.join(outputDir, "cache"))
assert(ok, err)
ok, err = star.copy(root .. "input", outputDir)
assert(ok, err)

local code, output = star.run(
    "tools\\pack.exe",
    "--input", outputDir)

print(output)
assert(code == 0, "发布命令失败，退出码：" .. code)

code, output = star.zip(
    outputDir,
    star.path.join(root, "release", "package.zip"))
assert(code == 0, output)
