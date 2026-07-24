local star = require "star"
local path, fs, process = star.path, star.fs, star.process

assert(star.version() == "1.0.0")
assert(star.lua_version() == "Lua 5.5.0")
assert(type(star.pause) == "function")
assert(star.script_path():match("smoke%.lua$"))

local root = path.join(
    star.script_dir(), ".smoke-" .. path.filename(star.exe_dir()))
fs.reset_dir(root)

local ok, message = pcall(function()
    local source = path.join(root, "source.txt")
    local copy = path.join(root, "nested", "copy.txt")

    fs.write(source, "alpha beta alpha")
    assert(fs.read(source) == "alpha beta alpha")
    assert(fs.replace_text(source, "alpha", "gamma") == 2)
    fs.copy_file(source, copy)
    assert(fs.is_file(copy))
    assert(fs.size(copy) == 16)
    assert(#fs.sha256(copy) == 64)
    assert(fs.count(root, "*.txt", true) == 2)
    assert(#fs.list(root, {
        pattern = "*.txt",
        recursive = true,
    }) == 2)
    fs.assert_no_match(root, {"*.pdb", "*.ilk"}, true)

    local result = process.run(
        "cmd.exe", {"/D", "/C", "exit", "0"})
    assert(result.ok and result.exit_code == 0)

    local captured = process.shell("echo LuaStar", {
        capture = true,
        hide = true,
    })
    assert(captured.ok and captured.output:match("LuaStar"))
end)

fs.remove(root)
assert(ok, message)

print("star 冒烟测试通过")
