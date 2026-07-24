local star = require "star"
local p, fs, release = star.path, star.fs, star.release

local root = star.script_dir()
local source = star.require_dir(p.join(root, "input"), "输入目录")
local output = p.join(root, "output")

fs.reset_dir(output)
fs.copy_tree(source, output)
fs.remove_matching(output, {"*.pdb", "*.ilk"}, true)
fs.assert_no_match(output, {"*.tmp", "*.log"}, true)

-- 外部工具可通过 exe、环境变量或 PATH 定位。
local setup = p.join(root, "setup.nsi")
if fs.is_file(setup) then
    release.nsis(setup, { warnings_as_errors = true })
end

print("发布目录已准备：" .. output)
