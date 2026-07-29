local star = require "star"

local scriptDir = star.path()
local absoluteRoot = star.path.join(
    scriptDir, "zip-test absolute ' literal")
local relativeRoot = "zip-test-relative"

local function write(path, content)
    local stream = assert(io.open(path, "wb"))
    assert(stream:write(content))
    assert(stream:close())
end

local function read(path)
    local stream = assert(io.open(path, "rb"))
    local content = assert(stream:read("*a"))
    assert(stream:close())
    return content
end

local function ps(value)
    return "'" .. value:gsub("'", "''") .. "'"
end

local function expand(archive, destination)
    local command = table.concat({
        "$ErrorActionPreference = 'Stop';",
        "Expand-Archive -LiteralPath", ps(archive),
        "-DestinationPath", ps(destination), "-Force",
    }, " ")
    local code, output = star.run(
        "powershell.exe",
        "-NoLogo",
        "-NoProfile",
        "-NonInteractive",
        "-Command",
        command)
    assert(code == 0, output)
end

local function touch(path)
    local command = "[IO.File]::WriteAllBytes(" ..
        ps(path) .. ", [byte[]]@())"
    local code, output = star.run(
        "powershell.exe",
        "-NoLogo",
        "-NoProfile",
        "-NonInteractive",
        "-Command",
        command)
    assert(code == 0, output)
end

local function clean()
    local ok, err = star.remove(absoluteRoot)
    assert(ok, err)
    ok, err = star.remove(relativeRoot)
    assert(ok, err)
end

local function test()
    clean()

    local sourceDir = star.path.join(absoluteRoot, "source")
    local nestedDir = star.path.join(sourceDir, "nested [1]")
    local emptyDir = star.path.join(sourceDir, "empty")
    local outputDir = star.path.join(absoluteRoot, "output")
    local extractDir = star.path.join(absoluteRoot, "extract")
    local archive = star.path.join(outputDir, "bundle.zip")
    local changedFile = star.path.join(nestedDir, "change.txt")
    local stableFile = star.path.join(sourceDir, "stable.txt")

    local relativeDir = star.path.join(relativeRoot, "relative dir")
    local relativeNested = star.path.join(relativeDir, "nested")
    local relativeFile = star.path.join(relativeNested, "relative.txt")
    local keepFile = star.path.join(relativeRoot, "keep.txt")
    local unicodeFile = star.path.join(sourceDir, "中文文件.txt")

    local ok, err = star.mkdir(
        nestedDir,
        emptyDir,
        relativeNested)
    assert(ok, err)
    assert(not star.exists(outputDir))
    write(changedFile, "before")
    write(stableFile, "stable")
    write(relativeFile, "relative")
    write(keepFile, "keep")
    touch(unicodeFile)

    -- 新建：一次混合压缩绝对目录、相对目录和相对文件。
    local code, output = star.zip(
        sourceDir,
        relativeDir,
        keepFile,
        archive)
    assert(code == 0, output)
    assert(output == "")
    local found, kind = star.exists(archive)
    assert(found and kind == "file")

    expand(archive, extractDir)
    assert(read(star.path.join(
        extractDir,
        star.path.name(sourceDir),
        "nested [1]",
        "change.txt")) == "before")
    assert(read(star.path.join(
        extractDir,
        star.path.name(sourceDir),
        "stable.txt")) == "stable")
    assert(read(star.path.join(
        extractDir,
        star.path.name(relativeDir),
        "nested",
        "relative.txt")) == "relative")
    assert(read(star.path.join(extractDir, "keep.txt")) == "keep")
    found, kind = star.exists(star.path.join(
        extractDir,
        star.path.name(sourceDir),
        "empty"))
    assert(found and kind == "dir")
    found, kind = star.exists(star.path.join(
        extractDir,
        star.path.name(sourceDir),
        "中文文件.txt"))
    assert(found and kind == "file")
    assert(not star.exists(star.path.join(
        extractDir, "nested [1]", "change.txt")))

    -- 更新：匹配条目即使时间较旧也必须替换，新条目必须加入，
    -- 本次未输入的 keep.txt 必须继续留在原 ZIP 中。
    write(changedFile, "after")
    local timeCommand = table.concat({
        "$item = Get-Item -LiteralPath", ps(changedFile), ";",
        "$item.LastWriteTime = [datetime]'2000-01-01T00:00:00'",
    }, " ")
    code, output = star.run(
        "powershell.exe",
        "-NoLogo",
        "-NoProfile",
        "-NonInteractive",
        "-Command",
        timeCommand)
    assert(code == 0, output)

    local addedFile = star.path.join(nestedDir, "added.bin")
    local extraFile = star.path.join(absoluteRoot, "extra file.bin")
    write(addedFile, "\0\1\2new")
    write(extraFile, "extra")

    code, output = star.zip(sourceDir, extraFile, archive)
    assert(code == 0, output)
    assert(output == "")
    ok, err = star.remove(extractDir)
    assert(ok, err)
    expand(archive, extractDir)

    local archivedSource = star.path.join(
        extractDir, star.path.name(sourceDir))
    assert(read(star.path.join(
        archivedSource, "nested [1]", "change.txt")) == "after")
    assert(read(star.path.join(
        archivedSource, "nested [1]", "added.bin")) == "\0\1\2new")
    assert(read(star.path.join(archivedSource, "stable.txt")) == "stable")
    assert(read(star.path.join(extractDir, "extra file.bin")) == "extra")
    assert(read(star.path.join(extractDir, "keep.txt")) == "keep")

    -- 输出 ZIP 也支持相对路径，目标父目录已存在时直接创建。
    local relativeArchive = star.path.join(relativeRoot, "relative.zip")
    code, output = star.zip(keepFile, relativeArchive)
    assert(code == 0, output)
    assert(output == "")
    found, kind = star.exists(relativeArchive)
    assert(found and kind == "file")

    -- 参数和路径错误必须在启动 PowerShell 前以 Lua 错误报告。
    local called, message = pcall(star.zip, archive)
    assert(not called and message:find("至少需要", 1, true))

    called, message = pcall(
        star.zip,
        star.path.join(absoluteRoot, "missing"),
        star.path.join(outputDir, "missing.zip"))
    assert(not called and message:find("不是普通文件或目录", 1, true))

    called, message = pcall(
        star.zip,
        keepFile,
        star.path.join(outputDir, "bad.rar"))
    assert(not called and message:find(".zip", 1, true))

    called, message = pcall(
        star.zip,
        sourceDir,
        star.path.join(sourceDir, "inside.zip"))
    assert(not called and message:find("输入目录内部", 1, true))

    called, message = pcall(
        star.zip,
        keepFile,
        keepFile,
        star.path.join(outputDir, "duplicate.zip"))
    assert(not called and message:find("不能重复", 1, true))

    local otherDir = star.path.join(relativeRoot, "other")
    ok, err = star.mkdir(otherDir)
    assert(ok, err)
    local sameName = star.path.join(otherDir, "keep.txt")
    write(sameName, "same name")
    called, message = pcall(
        star.zip,
        keepFile,
        sameName,
        star.path.join(outputDir, "same-name.zip"))
    assert(not called and message:find("顶层名称", 1, true))

    -- PowerShell 运行期错误继续使用 code/text 返回，不改成 Lua 异常。
    local corrupt = star.path.join(outputDir, "corrupt.zip")
    write(corrupt, "not a zip archive")
    code, output = star.zip(extraFile, corrupt)
    assert(code ~= 0)
    assert(type(output) == "string" and #output > 0)
    assert(not output:find("CLIXML", 1, true), output)
end

local ok, message = xpcall(test, debug.traceback)
local cleaned, cleanError = pcall(clean)
if not ok then
    error(message, 0)
end
assert(cleaned, cleanError)
print("star.zip 集成测试通过")
