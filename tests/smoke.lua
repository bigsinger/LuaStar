require "star"
assert(type(star) == "table")
local star = require "star"

local expected = {
    version = "function",
    debug = "function",
    path = "table",
    run = "function",
    zip = "function",
    copy = "function",
    move = "function",
    env = "function",
    mkdir = "function",
    remove = "function",
    exists = "function",
    pause = "function",
    fs = "table",
}

local count = 0
for name, value in pairs(star) do
    assert(expected[name], "发现多余接口：" .. name)
    assert(type(value) == expected[name])
    count = count + 1
end
assert(count == 13)

local function checkNames(value, names)
    local found = 0
    for key, item in pairs(value) do
        assert(names[key], "发现多余接口：" .. key)
        assert(type(item) == "function")
        found = found + 1
    end
    assert(found == names.count)
end

checkNames(star.path, {
    join = true,
    dir = true,
    name = true,
    ext = true,
    count = 4,
})
checkNames(star.fs, {
    copy = true,
    move = true,
    mkdir = true,
    remove = true,
    exists = true,
    count = 5,
})

assert(star.process == nil)
assert(star.release == nil)

local starVersion, luaVersion = star.version()
assert(starVersion == "1.1.0")
assert(luaVersion == "Lua 5.5.0")
assert(star.lua_version == nil)

local root, file = star.path()
assert(root:match("[\\/]$"))
assert(file == "smoke.lua")

local sample = star.path.join(root, "folder", "sample.txt")
assert(star.path.name(sample) == "sample.txt")
assert(star.path.ext(sample) == ".txt")
assert(star.path.dir(sample):match("folder[\\/]$"))

local base = star.path.join(root, ".smoke space")
local sourceDir = star.path.join(base, "source")
local copyDir = star.path.join(base, "copy")
local movedDir = star.path.join(base, "moved")

local function write(filePath, text)
    local stream = assert(io.open(filePath, "wb"))
    assert(stream:write(text))
    assert(stream:close())
end

local function read(filePath)
    local stream = assert(io.open(filePath, "rb"))
    local text = assert(stream:read("*a"))
    assert(stream:close())
    return text
end

local ok, err = star.remove(base)
assert(ok, err)
local nestedDir = star.path.join(base, "nested", "deep")
ok, err = star.mkdir(sourceDir, nestedDir)
assert(ok, err)
ok, err = star.fs.mkdir(sourceDir, nestedDir)
assert(ok, err)
local exists, kind = star.exists(nestedDir)
assert(exists and kind == "dir")
ok, err = star.remove("")
assert(not ok and type(err) == "string")

local sourceFile = star.path.join(sourceDir, "source.txt")
local copiedFile = star.path.join(base, "copied.txt")
local movedFile = star.path.join(base, "moved.txt")
write(sourceFile, "LuaStar")
local archive = star.path.join(base, "archive.zip")
local zipCode, zipOutput = star.zip(sourceDir, archive)
assert(zipCode == 0, zipOutput)
exists, kind = star.exists(archive)
assert(exists and kind == "file")
write(copiedFile, "old")

local found, kind = star.exists(sourceDir)
assert(found and kind == "dir")
found, kind = star.exists(sourceFile)
assert(found and kind == "file")
found, kind = star.exists(base .. "\\missing")
assert(not found and kind == nil)

ok, err = star.copy(sourceFile, copiedFile)
assert(ok, err)
ok, err = star.move(copiedFile, movedFile)
assert(ok, err)
assert(read(movedFile) == "LuaStar")

ok, err = star.fs.copy(sourceDir, copyDir)
assert(ok, err)
ok, err = star.fs.move(copyDir, movedDir)
assert(ok, err)
assert(read(movedDir .. "\\source.txt") == "LuaStar")

ok, err = star.copy(base .. "\\missing", base .. "\\unused")
assert(not ok and type(err) == "string")

ok, err = star.env(root)
assert(ok, err)
ok, err = star.env(root)
assert(ok, err)
local envCode, envText = star.run("echo %PATH%")
assert(envCode == 0)
assert(envText:lower():find(
    root:sub(1, -2):lower(), 1, true))

assert(star.debug() == false)
assert(star.debug(true) == true)
local code, output = star.run("echo", "LuaStar")
assert(code == 0 and output:match("LuaStar"))
code, output = star.run(
    "echo failure 1>&2", "&", "exit /B", 7)
assert(code == 7 and output:match("failure"))
assert(star.debug(false) == false)

ok, err = star.remove(movedFile)
assert(ok, err)
found, kind = star.exists(movedFile)
assert(not found and kind == nil)

ok, err = star.fs.remove(base)
assert(ok, err)
found, kind = star.fs.exists(base)
assert(not found and kind == nil)
print("star 冒烟测试通过")
